#include "jot/VaultIndex.h"

#include "greylock/Identity.h"
#include "jot/FrontMatter.h"
#include "jot/Logging.h"
#include "jot/NoteScan.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QRegularExpression>
#include <QSet>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QTimeZone>
#include <QUrl>

#include <sys/stat.h>

#include <algorithm>
#include <tuple>

using namespace Qt::StringLiterals;

namespace jot {

namespace {

const QString kNoteSuffix = u".md"_s;

struct Stamp
{
    qint64 size = -1;
    qint64 modified = -1;
};

std::optional<Stamp> stampOf(const QString &file)
{
    struct stat status{};
    if (::stat(QFile::encodeName(file).constData(), &status) != 0)
        return std::nullopt;
    return Stamp{qint64(status.st_size),
                 qint64(status.st_mtim.tv_sec) * 1000000000 + status.st_mtim.tv_nsec};
}

QStringList listOf(const QVariant &value)
{
    if (value.typeId() == QMetaType::QStringList)
        return value.toStringList();
    const QString text = value.toString().trimmed();
    return text.isEmpty() ? QStringList() : QStringList{text};
}

QString folderOf(const QString &path)
{
    const qsizetype slash = path.lastIndexOf(u'/');
    return slash < 0 ? QString() : path.left(slash);
}

/// `text` for LIKE, with its own % and _ taken literally.
QString likeEscaped(const QString &text)
{
    QString escaped = text;
    escaped.replace(u'\\', u"\\\\"_s).replace(u'%', u"\\%"_s).replace(u'_', u"\\_"_s);
    return escaped;
}

/// The search words, quotes keeping a phrase together.
QStringList wordsOf(const QString &query)
{
    QStringList words;
    QString word;
    bool quoted = false;
    for (const QChar c : query) {
        if (c == u'"') {
            quoted = !quoted;
        } else if (c.isSpace() && !quoted) {
            if (!word.isEmpty())
                words.append(std::exchange(word, {}));
        } else {
            word += c;
        }
    }
    if (!word.isEmpty())
        words.append(word);
    return words;
}

bool exec(QSqlQuery &query, const char *what)
{
    if (query.exec())
        return true;
    qCWarning(lcVault) << "index:" << what << "failed:" << query.lastError().text();
    return false;
}

} // namespace

bool isOverdue(const QString &due, const QDate &today)
{
    const QDate date = QDate::fromString(due.trimmed().left(10), Qt::ISODate);
    return date.isValid() && date < today;
}

void sortNotes(QList<NoteInfo> &notes, const QString &order, const QStringList &pinned)
{
    // When the note was made, in milliseconds, or when it last changed for want of that.
    const auto made = [](const NoteInfo &note) {
        QDateTime when = QDateTime::fromString(note.created.trimmed(), Qt::ISODate);
        if (!when.isValid()) {
            const QDate date = QDate::fromString(note.created.trimmed().left(10), Qt::ISODate);
            if (date.isValid())
                when = date.startOfDay(QTimeZone::UTC);
        }
        return when.isValid() ? when.toMSecsSinceEpoch() : note.modified / 1000000;
    };
    const QSet<QString> pins(pinned.cbegin(), pinned.cend());
    QHash<QString, qint64> created;
    if (order == u"created") {
        for (const NoteInfo &note : std::as_const(notes))
            created.insert(note.path, made(note));
    }
    std::ranges::stable_sort(notes, [&](const NoteInfo &a, const NoteInfo &b) {
        const bool aPinned = pins.contains(a.path);
        if (aPinned != pins.contains(b.path))
            return aPinned;
        if (order == u"title")
            return naturalCompare(a.title, b.title) < 0;
        if (order == u"created" && created[a.path] != created[b.path])
            return created[a.path] > created[b.path];
        return a.modified != b.modified ? a.modified > b.modified : a.path < b.path;
    });
}

VaultIndex::VaultIndex(Vault vault, QString databasePath)
    : m_vault(std::move(vault)),
      m_path(databasePath.isEmpty() ? defaultPath(m_vault) : std::move(databasePath)),
      m_connection(u"jot-index-%1"_s.arg(quintptr(this), 0, 16))
{}

VaultIndex::~VaultIndex()
{
    {
        QSqlDatabase db = QSqlDatabase::database(m_connection, false);
        if (db.isOpen())
            db.close();
    }
    QSqlDatabase::removeDatabase(m_connection);
}

QString VaultIndex::defaultPath(const Vault &vault)
{
    const QByteArray hash = QCryptographicHash::hash(QDir(vault.root()).absolutePath().toUtf8(),
                                                     QCryptographicHash::Sha256)
                                .toHex()
                                .left(16);
    return QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation) + u'/' +
           greylock::identity::dirName() + u"/index/"_s + QString::fromLatin1(hash) + u".sqlite"_s;
}

bool VaultIndex::open(QString *error)
{
    if (!QDir().mkpath(QFileInfo(m_path).path())) {
        *error = QObject::tr("could not create %1").arg(QFileInfo(m_path).path());
        return false;
    }
    QSqlDatabase db = QSqlDatabase::addDatabase(u"QSQLITE"_s, m_connection);
    db.setDatabaseName(m_path);
    if (!db.open()) {
        *error = db.lastError().text();
        return false;
    }
    QSqlQuery version(db);
    version.exec(u"PRAGMA user_version"_s);
    const int schema = version.next() ? version.value(0).toInt() : 0;
    if (schema != kSchema) {
        // A cache: an index from another version is thrown away and read again.
        qCInfo(lcVault) << "index schema" << schema << "is not" << kSchema << ", rebuilding";
        db.close();
        QFile::remove(m_path);
        if (!db.open()) {
            *error = db.lastError().text();
            return false;
        }
        if (!createSchema(error))
            return false;
    }
    QSqlQuery(u"PRAGMA journal_mode = WAL"_s, db);
    return true;
}

bool VaultIndex::createSchema(QString *error)
{
    QSqlDatabase db = QSqlDatabase::database(m_connection);
    const QStringList statements{
        u"CREATE TABLE notes (path TEXT PRIMARY KEY, title TEXT, size INTEGER, modified INTEGER, "
        u"task TEXT, due TEXT, created TEXT, type TEXT)"_s,
        u"CREATE TABLE tags (path TEXT, tag TEXT COLLATE NOCASE)"_s,
        u"CREATE INDEX tags_by_tag ON tags (tag)"_s,
        u"CREATE INDEX tags_by_path ON tags (path)"_s,
        u"CREATE TABLE aliases (path TEXT, alias TEXT COLLATE NOCASE)"_s,
        u"CREATE INDEX aliases_by_path ON aliases (path)"_s,
        u"CREATE TABLE links (source TEXT, raw TEXT, target TEXT, kind TEXT, line INTEGER)"_s,
        u"CREATE INDEX links_by_source ON links (source)"_s,
        u"CREATE INDEX links_by_target ON links (target)"_s,
        u"CREATE TABLE tasks (path TEXT, line INTEGER, done INTEGER, text TEXT)"_s,
        u"CREATE INDEX tasks_by_path ON tasks (path)"_s,
        u"CREATE VIRTUAL TABLE notes_fts USING fts5(path UNINDEXED, title, aliases, tags, body, "
        u"tokenize = 'unicode61 remove_diacritics 2')"_s,
        u"PRAGMA user_version = %1"_s.arg(kSchema),
    };
    for (const QString &statement : statements) {
        QSqlQuery query(db);
        if (!query.exec(statement)) {
            *error = query.lastError().text();
            return false;
        }
    }
    return true;
}

QStringList VaultIndex::refresh()
{
    QSqlDatabase db = QSqlDatabase::database(m_connection);
    QHash<QString, Stamp> known;
    QSqlQuery stored(u"SELECT path, size, modified FROM notes"_s, db);
    while (stored.next())
        known.insert(stored.value(0).toString(),
                     {stored.value(1).toLongLong(), stored.value(2).toLongLong()});

    QStringList changed;
    for (const QString &path : m_vault.notes()) {
        const std::optional<Stamp> stamp = stampOf(m_vault.absolute(path));
        const auto it = known.constFind(path);
        if (!stamp || it == known.cend() || it->size != stamp->size ||
            it->modified != stamp->modified)
            changed.append(path);
        known.remove(path);
    }
    changed.append(known.keys());
    if (!changed.isEmpty())
        update(changed);
    return changed;
}

void VaultIndex::update(const QStringList &paths)
{
    QSqlDatabase db = QSqlDatabase::database(m_connection);
    db.transaction();
    for (const QString &path : paths) {
        drop(path);
        if (QFileInfo(m_vault.absolute(path)).isFile())
            read(path);
    }
    resolveLinks();
    db.commit();
}

void VaultIndex::drop(const QString &path)
{
    QSqlDatabase db = QSqlDatabase::database(m_connection);
    // The search table is found by row, which matches the note's; by path it would be read whole.
    for (const QString &statement :
         {u"DELETE FROM notes_fts WHERE rowid = (SELECT rowid FROM notes WHERE path = ?)"_s,
          u"DELETE FROM notes WHERE path = ?"_s, u"DELETE FROM tags WHERE path = ?"_s,
          u"DELETE FROM aliases WHERE path = ?"_s, u"DELETE FROM links WHERE source = ?"_s,
          u"DELETE FROM tasks WHERE path = ?"_s}) {
        QSqlQuery query(db);
        query.prepare(statement);
        query.addBindValue(path);
        exec(query, "drop");
    }
}

void VaultIndex::read(const QString &path)
{
    QString error;
    const std::optional<QString> text = notefile::read(m_vault.absolute(path), &error);
    const std::optional<Stamp> stamp = stampOf(m_vault.absolute(path));
    if (!text || !stamp) {
        qCWarning(lcVault) << "index: could not read" << path << error;
        return;
    }
    const NoteParts parts = splitFrontMatter(*text);
    const QVariantMap meta = readFrontMatter(parts.frontMatter);
    const NoteScan scan = scanNote(parts.body, bodyFirstLine(*text, parts.body));

    QString title = meta.value(u"title"_s).toString().trimmed();
    if (title.isEmpty())
        title = Vault::titleFromPath(path);
    QStringList tags;
    for (QString tag : listOf(meta.value(u"tags"_s, meta.value(u"tag"_s)))) {
        if (tag.startsWith(u'#'))
            tag.remove(0, 1);
        if (!tag.isEmpty() && !tags.contains(tag, Qt::CaseInsensitive))
            tags.append(tag);
    }
    for (const QString &tag : scan.tags) {
        if (!tags.contains(tag, Qt::CaseInsensitive))
            tags.append(tag);
    }
    const QStringList aliases = listOf(meta.value(u"aliases"_s, meta.value(u"alias"_s)));
    QString task = meta.value(u"task"_s).toString().trimmed().toLower();
    if (task != u"open" && task != u"done")
        task.clear();

    QSqlDatabase db = QSqlDatabase::database(m_connection);
    QSqlQuery note(db);
    note.prepare(u"INSERT INTO notes VALUES (?, ?, ?, ?, ?, ?, ?, ?)"_s);
    for (const QVariant &value :
         {QVariant(path), QVariant(title), QVariant(stamp->size), QVariant(stamp->modified),
          QVariant(task), meta.value(u"due"_s), meta.value(u"created"_s), meta.value(u"type"_s)})
        note.addBindValue(value.isValid() ? value.toString() : QString());
    exec(note, "note");
    // Read now: SQLite reports the connection's latest insert, which the lists below change.
    const QVariant row = note.lastInsertId();

    const auto insertAll = [&db, &path](const QString &statement, const QStringList &values) {
        for (const QString &value : values) {
            QSqlQuery query(db);
            query.prepare(statement);
            query.addBindValue(path);
            query.addBindValue(value);
            exec(query, "list");
        }
    };
    insertAll(u"INSERT INTO tags VALUES (?, ?)"_s, tags);
    insertAll(u"INSERT INTO aliases VALUES (?, ?)"_s, aliases);

    for (const NoteScan::Link &link : scan.links) {
        QSqlQuery query(db);
        query.prepare(u"INSERT INTO links VALUES (?, ?, NULL, ?, ?)"_s);
        query.addBindValue(path);
        query.addBindValue(link.raw);
        query.addBindValue(link.kind);
        query.addBindValue(link.line);
        exec(query, "link");
    }
    for (const NoteScan::Task &task : scan.tasks) {
        QSqlQuery query(db);
        query.prepare(u"INSERT INTO tasks VALUES (?, ?, ?, ?)"_s);
        query.addBindValue(path);
        query.addBindValue(task.line);
        query.addBindValue(task.done ? 1 : 0);
        query.addBindValue(task.text);
        exec(query, "task");
    }

    QSqlQuery fts(db);
    fts.prepare(
        u"INSERT INTO notes_fts (rowid, path, title, aliases, tags, body) VALUES (?, ?, ?, ?, "
        u"?, ?)"_s);
    fts.addBindValue(row);
    fts.addBindValue(path);
    fts.addBindValue(title);
    fts.addBindValue(aliases.join(u' '));
    fts.addBindValue(tags.join(u' '));
    fts.addBindValue(parts.body.toString());
    exec(fts, "search text");
}

namespace {

/// Where links lead, worked out from the notes and aliases the index holds, in memory so a
/// whole vault's links can be resolved after every save.
class Resolver
{
public:
    Resolver(const Vault &vault, const QSqlDatabase &db) : m_vault(vault)
    {
        QSqlQuery notes(u"SELECT path FROM notes"_s, db);
        while (notes.next()) {
            const QString path = notes.value(0).toString();
            m_notes.insert(path);
            m_byName[Vault::titleFromPath(path).toLower()].append(path);
        }
        QSqlQuery aliases(u"SELECT alias, path FROM aliases ORDER BY length(path), path"_s, db);
        while (aliases.next()) {
            const QString alias = aliases.value(0).toString().toLower();
            if (!m_byAlias.contains(alias))
                m_byAlias.insert(alias, aliases.value(1).toString());
        }
    }

    QString resolve(const QString &raw, const QString &from) const
    {
        QString target = raw;
        const qsizetype anchor = target.indexOf(u'#');
        if (anchor >= 0)
            target.truncate(anchor);
        target = QUrl::fromPercentEncoding(target.toUtf8()).trimmed();
        if (target.isEmpty())
            return anchor >= 0 ? from : QString();

        // A path from the note's folder, then from the vault's root, never climbing out.
        for (const QString &base : {folderOf(from), QString()}) {
            const QString path = QDir::cleanPath(base.isEmpty() ? target : base + u'/' + target);
            if (path.isEmpty() || path == u".." || path.startsWith(u"../"_s) ||
                QDir::isAbsolutePath(path))
                continue;
            if (!path.endsWith(kNoteSuffix) && m_notes.contains(path + kNoteSuffix))
                return path + kNoteSuffix;
            if (m_notes.contains(path))
                return path;
            // Only a name with an extension can be an attachment, so others never touch the disk.
            if (!QFileInfo(path).suffix().isEmpty() && !path.endsWith(kNoteSuffix) &&
                QFileInfo(m_vault.absolute(path)).isFile())
                return path;
        }

        // By name: a note whose path ends with what was written, nearest then shortest.
        QString wanted = target;
        if (wanted.endsWith(kNoteSuffix, Qt::CaseInsensitive))
            wanted.chop(kNoteSuffix.size());
        const QString name = wanted.section(u'/', -1).toLower();
        QStringList matches;
        for (const QString &path : m_byName.value(name)) {
            if (!wanted.contains(u'/') ||
                path.endsWith(u'/' + wanted + kNoteSuffix, Qt::CaseInsensitive) ||
                path.compare(wanted + kNoteSuffix, Qt::CaseInsensitive) == 0)
                matches.append(path);
        }
        if (!matches.isEmpty()) {
            const QString folder = folderOf(from);
            std::ranges::sort(matches, [&folder](const QString &a, const QString &b) {
                const bool aHere = folderOf(a) == folder;
                const bool bHere = folderOf(b) == folder;
                if (aHere != bHere)
                    return aHere;
                return a.size() != b.size() ? a.size() < b.size() : a < b;
            });
            return matches.first();
        }
        if (const QString alias = m_byAlias.value(wanted.toLower()); !alias.isEmpty())
            return alias;

        // An attachment by name alone, as ![[map.png]] writes it, is looked for beside the
        // note, in its own attachments first. Only a name that ends like a file's does, so
        // a note called "v1.2 plan" never sends Jot to the disk.
        static const QRegularExpression extension(u"\\.[A-Za-z0-9]{1,5}$"_s);
        if (!target.contains(u'/') && extension.match(target).hasMatch()) {
            for (const QString &folder : {Vault::assetsFolderFor(from), folderOf(from)}) {
                const QString path = folder.isEmpty() ? target : folder + u'/' + target;
                if (QFileInfo(m_vault.absolute(path)).isFile())
                    return path;
            }
        }
        return {};
    }

private:
    const Vault &m_vault;
    QSet<QString> m_notes;
    QHash<QString, QStringList> m_byName;
    QHash<QString, QString> m_byAlias;
};

} // namespace

QString VaultIndex::resolve(const QString &raw, const QString &from) const
{
    return Resolver(m_vault, QSqlDatabase::database(m_connection)).resolve(raw, from);
}

void VaultIndex::resolveLinks()
{
    QSqlDatabase db = QSqlDatabase::database(m_connection);
    const Resolver resolver(m_vault, db);
    QSqlQuery links(u"SELECT rowid, source, raw, target FROM links"_s, db);
    QList<std::pair<qint64, QString>> changes;
    while (links.next()) {
        const QString target =
            resolver.resolve(links.value(2).toString(), links.value(1).toString());
        if (target != links.value(3).toString() || (target.isEmpty() != links.value(3).isNull()))
            changes.append({links.value(0).toLongLong(), target});
    }
    for (const auto &[row, target] : std::as_const(changes)) {
        QSqlQuery update(db);
        update.prepare(u"UPDATE links SET target = ? WHERE rowid = ?"_s);
        update.addBindValue(target.isEmpty() ? QVariant(QMetaType(QMetaType::QString))
                                             : QVariant(target));
        update.addBindValue(row);
        exec(update, "resolve");
    }
}

NoteInfo VaultIndex::infoFrom(const QSqlQuery &query) const
{
    NoteInfo info;
    info.path = query.value(0).toString();
    info.title = query.value(1).toString();
    info.task = query.value(2).toString();
    info.due = query.value(3).toString();
    info.created = query.value(4).toString();
    info.type = query.value(5).toString();
    info.modified = query.value(6).toLongLong();
    QSqlDatabase db = QSqlDatabase::database(m_connection);
    const auto listFor = [&db, &info](const QString &statement) {
        QStringList values;
        QSqlQuery list(db);
        list.prepare(statement);
        list.addBindValue(info.path);
        if (list.exec()) {
            while (list.next())
                values.append(list.value(0).toString());
        }
        return values;
    };
    info.tags = listFor(u"SELECT tag FROM tags WHERE path = ? ORDER BY rowid"_s);
    info.aliases = listFor(u"SELECT alias FROM aliases WHERE path = ? ORDER BY rowid"_s);
    return info;
}

std::optional<NoteInfo> VaultIndex::note(const QString &path) const
{
    QSqlQuery query(QSqlDatabase::database(m_connection));
    query.prepare(
        u"SELECT path, title, task, due, created, type, modified FROM notes WHERE path = ?"_s);
    query.addBindValue(path);
    if (!query.exec() || !query.next())
        return std::nullopt;
    return infoFrom(query);
}

QList<NoteInfo> VaultIndex::notesIn(const QString &folder) const
{
    QSqlQuery query(QSqlDatabase::database(m_connection));
    if (folder.isEmpty()) {
        query.prepare(u"SELECT path, title, task, due, created, type, modified FROM notes"_s);
    } else {
        query.prepare(u"SELECT path, title, task, due, created, type, modified FROM notes "
                      u"WHERE path LIKE ? ESCAPE '\\'"_s);
        query.addBindValue(likeEscaped(folder) + u"/%"_s);
    }
    // Tags and aliases come in one query each, not two for every note.
    QHash<QString, QStringList> tags;
    QHash<QString, QStringList> aliases;
    QSqlDatabase db = QSqlDatabase::database(m_connection);
    for (auto [table, column, into] :
         {std::tuple{"tags", "tag", &tags}, std::tuple{"aliases", "alias", &aliases}}) {
        QSqlQuery list(u"SELECT path, %1 FROM %2 ORDER BY rowid"_s.arg(QLatin1String(column),
                                                                       QLatin1String(table)),
                       db);
        while (list.next())
            (*into)[list.value(0).toString()].append(list.value(1).toString());
    }
    QList<NoteInfo> notes;
    if (exec(query, "notes in folder")) {
        while (query.next()) {
            NoteInfo info;
            info.path = query.value(0).toString();
            info.title = query.value(1).toString();
            info.task = query.value(2).toString();
            info.due = query.value(3).toString();
            info.created = query.value(4).toString();
            info.type = query.value(5).toString();
            info.modified = query.value(6).toLongLong();
            info.tags = tags.value(info.path);
            info.aliases = aliases.value(info.path);
            notes.append(info);
        }
    }
    return notes;
}

QHash<QString, int> VaultIndex::noteCounts() const
{
    QSqlQuery query(u"SELECT path FROM notes"_s, QSqlDatabase::database(m_connection));
    QHash<QString, int> counts;
    while (query.next()) {
        QString folder = folderOf(query.value(0).toString());
        while (!folder.isEmpty()) {
            ++counts[folder];
            folder = folderOf(folder);
        }
    }
    return counts;
}

int VaultIndex::noteCount() const
{
    QSqlQuery query(u"SELECT count(*) FROM notes"_s, QSqlDatabase::database(m_connection));
    return query.next() ? query.value(0).toInt() : 0;
}

QList<SearchHit> VaultIndex::search(const QString &query, int limit) const
{
    QStringList terms;
    QStringList where;
    QVariantList values;
    for (const QString &word : wordsOf(query)) {
        const QString key = word.section(u':', 0, 0).toLower();
        const QString value = word.section(u':', 1);
        if (word.contains(u':') && !value.isEmpty() &&
            (key == u"tag" || key == u"in" || key == u"task" || key == u"type")) {
            if (key == u"tag") {
                QString tag = value.startsWith(u'#') ? value.mid(1) : value;
                where.append(u"n.path IN (SELECT path FROM tags WHERE tag = ? OR tag LIKE ? "
                             u"ESCAPE '\\')"_s);
                values << tag << likeEscaped(tag) + u"/%"_s;
            } else if (key == u"in") {
                where.append(u"n.path LIKE ? ESCAPE '\\'"_s);
                values << likeEscaped(QDir::cleanPath(value)) + u"/%"_s;
            } else if (key == u"task") {
                where.append(u"n.task = ?"_s);
                values << value.toLower();
            } else {
                where.append(u"n.type = ? COLLATE NOCASE"_s);
                values << value;
            }
            continue;
        }
        // Each word quoted for FTS5, matched at the start of words.
        QString quoted = word;
        quoted.replace(u'"', u"\"\""_s);
        terms.append(u'"' + quoted + u"\"*"_s);
    }
    if (terms.isEmpty() && where.isEmpty())
        return {};

    QString sql;
    if (terms.isEmpty()) {
        sql = u"SELECT n.path, n.title, '', n.modified FROM notes n WHERE %1 ORDER BY n.title "
              u"COLLATE NOCASE "
              u"LIMIT ?"_s.arg(where.join(u" AND "_s));
    } else {
        where.prepend(u"notes_fts MATCH ?"_s);
        values.prepend(terms.join(u' '));
        sql = u"SELECT n.path, n.title, snippet(notes_fts, 4, '<mark>', '</mark>', '...', 12), "
              u"n.modified "
              u"FROM notes_fts JOIN notes n ON n.path = notes_fts.path WHERE %1 "
              u"ORDER BY bm25(notes_fts, 0.0, 10.0, 5.0, 3.0, 1.0) LIMIT ?"_s.arg(
                  where.join(u" AND "_s));
    }
    QSqlQuery select(QSqlDatabase::database(m_connection));
    select.prepare(sql);
    for (const QVariant &value : std::as_const(values))
        select.addBindValue(value);
    select.addBindValue(limit);
    QList<SearchHit> hits;
    if (!exec(select, "search"))
        return hits;
    while (select.next())
        hits.append(
            {select.value(0).toString(), select.value(1).toString(), select.value(2).toString()});
    return hits;
}

namespace {

QList<NoteLink> linksFrom(QSqlQuery &query)
{
    QList<NoteLink> links;
    if (!exec(query, "links"))
        return links;
    while (query.next())
        links.append({query.value(0).toString(), query.value(1).toString(),
                      query.value(2).toString(), query.value(3).toString(),
                      query.value(4).toInt()});
    return links;
}

} // namespace

QList<NoteLink> VaultIndex::linksFrom(const QString &path) const
{
    QSqlQuery query(QSqlDatabase::database(m_connection));
    query.prepare(u"SELECT source, raw, target, kind, line FROM links WHERE source = ? "
                  u"ORDER BY line, rowid"_s);
    query.addBindValue(path);
    return jot::linksFrom(query);
}

QList<NoteLink> VaultIndex::backlinks(const QString &path) const
{
    QSqlQuery query(QSqlDatabase::database(m_connection));
    query.prepare(u"SELECT source, raw, target, kind, line FROM links WHERE target = ? AND "
                  u"source != ? ORDER BY source, line"_s);
    query.addBindValue(path);
    query.addBindValue(path);
    return jot::linksFrom(query);
}

QStringList VaultIndex::sourcesLinkingInto(const QString &folder) const
{
    // A prefix compare rather than LIKE, which would read % and _ in a folder's name.
    const QString prefix = folder + u'/';
    QSqlQuery query(QSqlDatabase::database(m_connection));
    query.prepare(u"SELECT DISTINCT source FROM links WHERE substr(target, 1, ?) = ?"_s);
    query.addBindValue(prefix.size());
    query.addBindValue(prefix);
    QStringList sources;
    if (query.exec()) {
        while (query.next())
            sources.append(query.value(0).toString());
    }
    return sources;
}

QList<NoteLink> VaultIndex::unresolved() const
{
    QSqlQuery query(QSqlDatabase::database(m_connection));
    query.prepare(u"SELECT source, raw, target, kind, line FROM links WHERE target IS NULL "
                  u"ORDER BY raw, source"_s);
    return jot::linksFrom(query);
}

QStringList VaultIndex::tags() const
{
    QSqlQuery query(u"SELECT DISTINCT tag FROM tags ORDER BY tag COLLATE NOCASE"_s,
                    QSqlDatabase::database(m_connection));
    QStringList tags;
    while (query.next())
        tags.append(query.value(0).toString());
    return tags;
}

QStringList VaultIndex::tagged(const QString &tag) const
{
    QSqlQuery query(QSqlDatabase::database(m_connection));
    query.prepare(u"SELECT DISTINCT path FROM tags WHERE tag = ? OR tag LIKE ? ESCAPE '\\' "
                  u"ORDER BY path"_s);
    query.addBindValue(tag);
    query.addBindValue(likeEscaped(tag) + u"/%"_s);
    QStringList paths;
    if (exec(query, "tagged")) {
        while (query.next())
            paths.append(query.value(0).toString());
    }
    return paths;
}

QList<NoteTask> VaultIndex::openTasks() const
{
    QSqlQuery query(
        u"SELECT path, line, done, text FROM tasks WHERE done = 0 ORDER BY path, line"_s,
        QSqlDatabase::database(m_connection));
    QList<NoteTask> tasks;
    while (query.next())
        tasks.append({query.value(0).toString(), query.value(1).toInt(), query.value(2).toBool(),
                      query.value(3).toString()});
    return tasks;
}

QList<NoteInfo> VaultIndex::openTaskNotes() const
{
    // Those with a due date first, soonest first; then the rest by title.
    QSqlQuery query(
        u"SELECT path, title, task, due, created, type, modified FROM notes WHERE task = 'open' "
        u"ORDER BY coalesce(due, '') = '', due, title COLLATE NOCASE"_s,
        QSqlDatabase::database(m_connection));
    QList<NoteInfo> notes;
    while (query.next())
        notes.append(infoFrom(query));
    return notes;
}

} // namespace jot
