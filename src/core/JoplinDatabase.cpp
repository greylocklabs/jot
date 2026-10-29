#include "jot/JoplinExport.h"

#include "jot/JoplinPatch.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QScopeGuard>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QStandardPaths>
#include <QTimeZone>

#include <algorithm>

using namespace Qt::StringLiterals;

namespace jot {

namespace {

/// One row, read by column name, so a column an older or newer Joplin lacks reads as empty.
class Row
{
public:
    explicit Row(const QSqlQuery &query) : m_record(query.record()) {}

    [[nodiscard]] QString text(const QString &column) const
    {
        return m_record.value(column).toString();
    }
    [[nodiscard]] bool flag(const QString &column) const
    {
        return m_record.value(column).toLongLong() != 0;
    }
    /// Joplin keeps times as ms since 1970, and 0 for none.
    [[nodiscard]] QDateTime time(const QString &column) const
    {
        const qint64 ms = m_record.value(column).toLongLong();
        return ms > 0 ? QDateTime::fromMSecsSinceEpoch(ms, QTimeZone::UTC) : QDateTime();
    }

private:
    QSqlRecord m_record;
};

QString count(int n, const QString &one, const QString &many)
{
    return QString::number(n) + u' ' + (n == 1 ? one : many);
}

} // namespace

std::optional<JoplinExport> readJoplinDatabase(const QString &path, QString *error)
{
    const auto fail = [&](const QString &why) -> std::optional<JoplinExport> {
        if (error)
            *error = why;
        return std::nullopt;
    };
    if (!QFileInfo(path).isFile())
        return fail(u"There is no Joplin database at %1."_s.arg(path));

    const QString connection = u"jot-joplin-%1"_s.arg(quintptr(&path), 0, 16);
    const auto removed = qScopeGuard([&] { QSqlDatabase::removeDatabase(connection); });
    JoplinExport out;
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(u"QSQLITE"_s, connection);
        db.setDatabaseName(path);
        db.setConnectOptions(u"QSQLITE_OPEN_READONLY"_s);
        if (!db.open())
            return fail(u"Could not open the Joplin database: %1"_s.arg(db.lastError().text()));
        const QStringList tables = db.tables();
        if (!tables.contains(u"notes"_s) || !tables.contains(u"folders"_s))
            return fail(u"This is not a Joplin database: it has no notes or notebooks."_s);

        QSqlQuery query(db);
        const auto rows = [&](const QString &table, const std::function<void(const Row &)> &each) {
            if (!tables.contains(table))
                return true;
            if (!query.exec(u"SELECT * FROM %1"_s.arg(table)))
                return false;
            while (query.next())
                each(Row(query));
            return true;
        };

        int encrypted = 0;
        const auto readable = [&](const Row &row) {
            if (!row.flag(u"encryption_applied"_s))
                return true;
            ++encrypted;
            return false;
        };
        const QDir resources(QFileInfo(path).dir().filePath(u"resources"_s));
        const bool read =
            rows(u"notes"_s,
                 [&](const Row &row) {
                     if (!readable(row))
                         return;
                     out.notes.append({
                         .id = row.text(u"id"_s),
                         .notebookId = row.text(u"parent_id"_s),
                         .title = row.text(u"title"_s),
                         .body = row.text(u"body"_s),
                         .created = row.time(u"user_created_time"_s),
                         .updated = row.time(u"user_updated_time"_s),
                         .isTodo = row.flag(u"is_todo"_s),
                         .due = row.time(u"todo_due"_s),
                         .completed = row.time(u"todo_completed"_s),
                         .html = row.text(u"markup_language"_s) == u"2"_s,
                         .sourceUrl = row.text(u"source_url"_s),
                         .author = row.text(u"author"_s),
                         .conflict = row.flag(u"is_conflict"_s),
                         .deleted = row.flag(u"deleted_time"_s),
                         .revisions = {},
                     });
                 }) &&
            rows(u"folders"_s,
                 [&](const Row &row) {
                     if (!readable(row))
                         return;
                     // The icon is JSON, as {"emoji":"🦊","name":"fox","type":1}.
                     static const QRegularExpression emoji(u"\"emoji\"\\s*:\\s*\"([^\"]*)\""_s);
                     out.notebooks.append({
                         .id = row.text(u"id"_s),
                         .parentId = row.text(u"parent_id"_s),
                         .title = row.text(u"title"_s),
                         .emoji = emoji.match(row.text(u"icon"_s)).captured(1),
                         .deleted = row.flag(u"deleted_time"_s),
                     });
                 }) &&
            rows(u"resources"_s,
                 [&](const Row &row) {
                     if (!readable(row))
                         return;
                     // Its details may be readable while the file itself is still encrypted.
                     if (row.flag(u"encryption_blob_encrypted"_s)) {
                         ++encrypted;
                         return;
                     }
                     const QString id = row.text(u"id"_s);
                     const QString extension = row.text(u"file_extension"_s);
                     // Stored as <id>.<extension>, or as <id> alone when it has none.
                     const QString file =
                         resources.filePath(extension.isEmpty() ? id : id + u'.' + extension);
                     out.resources.append({
                         .id = id,
                         .title = row.text(u"title"_s),
                         .mime = row.text(u"mime"_s),
                         .fileName = row.text(u"filename"_s),
                         .extension = extension,
                         .data = {},
                         .file = QFileInfo(file).isFile() ? file : QString(),
                     });
                 }) &&
            rows(u"tags"_s,
                 [&](const Row &row) {
                     if (readable(row))
                         out.tags.append({.id = row.text(u"id"_s), .title = row.text(u"title"_s)});
                 }) &&
            rows(u"note_tags"_s, [&](const Row &row) {
                if (readable(row))
                    out.noteTags[row.text(u"note_id"_s)].append(row.text(u"tag_id"_s));
            });
        if (!read)
            return fail(u"Could not read the Joplin database: %1"_s.arg(query.lastError().text()));

        // Each revision is a patch on the one before it, so a note's are pieced together from
        // nothing, in order, as Joplin does; the first that will not apply ends the note's.
        struct Patch
        {
            QDateTime updated;
            QString title;
            QString body;
            bool encrypted = false;
        };
        QHash<QString, QList<Patch>> patches;
        if (!rows(u"revisions"_s, [&](const Row &row) {
                if (!row.text(u"item_type"_s).isEmpty() && row.text(u"item_type"_s) != u"1"_s)
                    return;
                patches[row.text(u"item_id"_s)].append(
                    {row.time(u"item_updated_time"_s), row.text(u"title_diff"_s),
                     row.text(u"body_diff"_s), row.flag(u"encryption_applied"_s)});
            }))
            return fail(u"Could not read the Joplin database: %1"_s.arg(query.lastError().text()));
        int unread = 0;
        for (JoplinExport::Note &note : out.notes) {
            QList<Patch> list = patches.take(note.id);
            std::ranges::stable_sort(
                list, [](const Patch &a, const Patch &b) { return a.updated < b.updated; });
            QString title;
            QString body;
            for (qsizetype i = 0; i < list.size(); ++i) {
                const std::optional<QString> nextTitle =
                    list[i].encrypted ? std::nullopt : applyJoplinPatch(title, list[i].title);
                const std::optional<QString> nextBody =
                    list[i].encrypted ? std::nullopt : applyJoplinPatch(body, list[i].body);
                if (!nextTitle || !nextBody) {
                    unread += int(list.size() - i);
                    break;
                }
                title = *nextTitle;
                body = *nextBody;
                note.revisions.append({list[i].updated, title, body});
            }
        }

        if (encrypted > 0)
            out.skipped.append(count(encrypted, u"encrypted item"_s, u"encrypted items"_s) +
                               u", which Joplin has to decrypt first"_s);
        if (unread > 0)
            out.skipped.append(count(unread, u"note revision"_s, u"note revisions"_s) +
                               u" that could not be pieced together"_s);
    }
    return out;
}

std::optional<JoplinExport> readJoplin(const QString &path, QString *error)
{
    if (QFileInfo(path).isDir())
        return readJoplinDatabase(QDir(path).filePath(u"database.sqlite"_s), error);
    QFile file(path);
    if (file.open(QIODevice::ReadOnly) && file.peek(16) == QByteArrayView("SQLite format 3\0", 16))
        return readJoplinDatabase(path, error);
    return readJoplinExport(path, error);
}

QString joplinProfileDatabase()
{
    const QString config = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    for (const QString &profile :
         {config + u"/joplin-desktop"_s,
          QDir::homePath() + u"/.var/app/net.cozic.joplin_desktop/config/joplin-desktop"_s}) {
        const QString database = profile + u"/database.sqlite"_s;
        if (QFileInfo(database).isFile())
            return database;
    }
    return {};
}

} // namespace jot
