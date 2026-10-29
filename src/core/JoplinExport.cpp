#include "jot/JoplinExport.h"

#include <QFile>
#include <QRegularExpression>
#include <QTimeZone>

using namespace Qt::StringLiterals;

namespace jot {

namespace {

constexpr qsizetype kBlock = 512;

/// One file in a tar archive.
struct TarEntry
{
    QString path;
    QByteArray data;
};

qint64 octal(QByteArrayView field)
{
    qint64 value = 0;
    for (const char c : field) {
        if (c < '0' || c > '7')
            break;
        value = value * 8 + (c - '0');
    }
    return value;
}

QString cString(QByteArrayView field)
{
    const qsizetype end = field.indexOf('\0');
    return QString::fromUtf8(end < 0 ? field : field.first(end));
}

/// The path a pax extended header gives the next file, if it gives one.
QString paxPath(const QByteArray &records)
{
    qsizetype at = 0;
    while (at < records.size()) {
        const qsizetype space = records.indexOf(' ', at);
        if (space < 0)
            break;
        const qsizetype length = records.mid(at, space - at).toLongLong();
        if (length <= 0)
            break;
        const QByteArray record = records.mid(space + 1, length - (space + 1 - at) - 1);
        if (record.startsWith("path="))
            return QString::fromUtf8(record.sliced(5));
        at += length;
    }
    return {};
}

/// The regular files in a ustar archive, with pax and GNU long names honored. Nothing when
/// `tar` is not one.
std::optional<QList<TarEntry>> readTar(const QByteArray &tar)
{
    QList<TarEntry> entries;
    QString longName;
    qsizetype at = 0;
    while (at + kBlock <= tar.size()) {
        const QByteArrayView header(tar.constData() + at, kBlock);
        if (header.count('\0') == kBlock)
            return entries;
        if (!header.sliced(257, 5).startsWith("ustar"))
            return std::nullopt;
        const qint64 size = octal(header.sliced(124, 12));
        const char type = header[156];
        at += kBlock;
        if (size < 0 || at + size > tar.size())
            return std::nullopt;
        const QByteArray data = tar.mid(at, size);
        at += (size + kBlock - 1) / kBlock * kBlock;

        if (type == 'x') {
            longName = paxPath(data);
            continue;
        }
        if (type == 'L') {
            longName = cString(data);
            continue;
        }
        if (type != '0' && type != '\0') {
            longName.clear();
            continue;
        }
        QString path = longName;
        longName.clear();
        if (path.isEmpty()) {
            const QString prefix = cString(header.sliced(345, 155));
            const QString name = cString(header.first(100));
            path = prefix.isEmpty() ? name : prefix + u'/' + name;
        }
        entries.append({path, data});
    }
    // Some writers leave out the closing blocks; nothing at all means it was not a tar.
    if (entries.isEmpty())
        return std::nullopt;
    return entries;
}

/// One raw item: its first line, the lines between that and its properties, and the
/// properties, which follow the last blank line as `key: value`.
struct RawItem
{
    QString title;
    QString body;
    QHash<QString, QString> props;
};

RawItem parseItem(const QString &text)
{
    static const QRegularExpression prop(u"^([a-z_]+): ?(.*)$"_s);
    const QStringList lines = text.split(u'\n');
    qsizetype propsStart = lines.size();
    while (propsStart > 0 && prop.match(lines[propsStart - 1]).hasMatch())
        --propsStart;

    RawItem item;
    for (qsizetype i = propsStart; i < lines.size(); ++i) {
        const auto match = prop.match(lines[i]);
        item.props.insert(match.captured(1), match.captured(2));
    }
    // An item with only properties, as a note's tag, has no title.
    if (propsStart == 0)
        return item;
    const qsizetype textEnd = lines[propsStart - 1].isEmpty() ? propsStart - 1 : propsStart;
    item.title = lines.first();
    if (textEnd > 2)
        item.body = lines.mid(2, textEnd - 2).join(u'\n');
    return item;
}

/// A time as the export writes it: ISO 8601, or milliseconds since the epoch, with 0 for none.
QDateTime timeOf(const QString &value)
{
    bool number = false;
    const qint64 ms = value.toLongLong(&number);
    if (number)
        return ms > 0 ? QDateTime::fromMSecsSinceEpoch(ms, QTimeZone::UTC) : QDateTime();
    return QDateTime::fromString(value, Qt::ISODateWithMs);
}

bool flag(const RawItem &item, const QString &key)
{
    const QString value = item.props.value(key);
    return !value.isEmpty() && value != u"0"_s;
}

bool hasTime(const RawItem &item, const QString &key)
{
    return timeOf(item.props.value(key)).isValid();
}

enum ItemType {
    NoteType = 1,
    NotebookType = 2,
    ResourceType = 4,
    TagType = 5,
    NoteTagType = 6,
    RevisionType = 13,
};

} // namespace

std::optional<JoplinExport> readJoplinExport(const QByteArray &jex, QString *error)
{
    const auto fail = [&](const QString &why) -> std::optional<JoplinExport> {
        if (error)
            *error = why;
        return std::nullopt;
    };
    const auto entries = readTar(jex);
    if (!entries)
        return fail(u"This is not a JEX export: it is not a tar archive."_s);

    // Attachments sit under resources/ as <id> or <id>.<extension>.
    QHash<QString, QByteArray> files;
    for (const TarEntry &entry : *entries) {
        if (entry.path.startsWith(u"resources/"_s)) {
            const QString name = entry.path.sliced(10);
            files.insert(name.section(u'.', 0, 0), entry.data);
        }
    }

    JoplinExport out;
    int encrypted = 0;
    int revisions = 0;
    int unknown = 0;
    for (const TarEntry &entry : *entries) {
        if (entry.path.contains(u'/') || !entry.path.endsWith(u".md"_s))
            continue;
        const RawItem item = parseItem(QString::fromUtf8(entry.data));
        const QString id = item.props.value(u"id"_s);
        if (flag(item, u"encryption_applied"_s)) {
            ++encrypted;
            continue;
        }
        switch (item.props.value(u"type_"_s).toInt()) {
        case NoteType:
            out.notes.append({
                .id = id,
                .notebookId = item.props.value(u"parent_id"_s),
                .title = item.title,
                .body = item.body,
                .created = timeOf(item.props.value(u"user_created_time"_s)),
                .updated = timeOf(item.props.value(u"user_updated_time"_s)),
                .isTodo = flag(item, u"is_todo"_s),
                .due = timeOf(item.props.value(u"todo_due"_s)),
                .completed = timeOf(item.props.value(u"todo_completed"_s)),
                .html = item.props.value(u"markup_language"_s) == u"2"_s,
                .sourceUrl = item.props.value(u"source_url"_s),
                .author = item.props.value(u"author"_s),
                .conflict = flag(item, u"is_conflict"_s),
                .deleted = hasTime(item, u"deleted_time"_s),
            });
            break;
        case NotebookType:
            out.notebooks.append({
                .id = id,
                .parentId = item.props.value(u"parent_id"_s),
                .title = item.title,
                .emoji =
                    [&] {
                        // The icon is JSON, as {"emoji":"🦊","name":"fox","type":1}.
                        static const QRegularExpression emoji(u"\"emoji\"\\s*:\\s*\"([^\"]*)\""_s);
                        return emoji.match(item.props.value(u"icon"_s)).captured(1);
                    }(),
                .deleted = hasTime(item, u"deleted_time"_s),
            });
            break;
        case ResourceType:
            out.resources.append({
                .id = id,
                .title = item.title,
                .mime = item.props.value(u"mime"_s),
                .fileName = item.props.value(u"filename"_s),
                .extension = item.props.value(u"file_extension"_s),
                .data = files.value(id),
                .file = {},
            });
            break;
        case TagType: out.tags.append({.id = id, .title = item.title}); break;
        case NoteTagType:
            out.noteTags[item.props.value(u"note_id"_s)].append(item.props.value(u"tag_id"_s));
            break;
        case RevisionType: ++revisions; break;
        default: ++unknown; break;
        }
    }
    if (out.notes.isEmpty() && out.notebooks.isEmpty() && out.tags.isEmpty() && encrypted == 0)
        return fail(u"This is not a JEX export: it has no Joplin notes, notebooks or tags."_s);

    const auto count = [](int n, const QString &one, const QString &many) {
        return QString::number(n) + u' ' + (n == 1 ? one : many);
    };
    if (encrypted > 0)
        out.skipped.append(count(encrypted, u"encrypted item"_s, u"encrypted items"_s) +
                           u", which Joplin has to decrypt first"_s);
    if (revisions > 0)
        out.skipped.append(count(revisions, u"note revision"_s, u"note revisions"_s));
    if (unknown > 0)
        out.skipped.append(count(unknown, u"item of a kind Jot does not read"_s,
                                 u"items of a kind Jot does not read"_s));
    return out;
}

std::optional<JoplinExport> readJoplinExport(const QString &path, QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error)
            *error = file.errorString();
        return std::nullopt;
    }
    return readJoplinExport(file.readAll(), error);
}

} // namespace jot
