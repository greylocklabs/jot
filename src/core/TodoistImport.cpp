#include "jot/TodoistImport.h"

#include "jot/FrontMatter.h"
#include "jot/QuickAdd.h"
#include "jot/TaskLine.h"
#include "jot/Vault.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QObject>
#include <QRegularExpression>
#include <QSet>
#include <QTimeZone>
#include <QUrlQuery>

#include <algorithm>
#include <functional>

using namespace Qt::StringLiterals;

namespace jot {

namespace {

const QString kLedger = u".jot/todoist.yaml"_s;

QString join(const QString &folder, const QString &name)
{
    return folder.isEmpty() ? name : folder + u'/' + name;
}

QString sha(const QString &text)
{
    return QString::fromLatin1(
        QCryptographicHash::hash(text.toUtf8(), QCryptographicHash::Sha256).toHex());
}

/// Todoist's ids are strings now, and were numbers once.
QString idOf(const QJsonValue &value)
{
    if (value.isString())
        return value.toString();
    if (value.isDouble())
        return QString::number(qint64(value.toDouble()));
    return {};
}

/// What earlier imports brought into the vault, as `.jot/todoist.yaml` keeps it.
struct Ledger
{
    struct Note
    {
        QString path;
        /// The file's text as the import wrote it.
        QString sha;
    };
    QHash<QString, Note> projects;
    /// The Inbox's tasks already added to the inbox note.
    QSet<QString> inbox;
};

QString unquote(const QString &value)
{
    if (value.size() < 2 || !value.startsWith(u'"') || !value.endsWith(u'"'))
        return value;
    QString out;
    for (qsizetype i = 1; i + 1 < value.size(); ++i) {
        if (value[i] == u'\\' && i + 2 < value.size())
            ++i;
        out += value[i];
    }
    return out;
}

Ledger readLedger(const QString &path)
{
    Ledger ledger;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return ledger;
    static const QRegularExpression line(u"^( *)([^:#]+):(?: (.*))?$"_s);
    QString section;
    QString id;
    for (const QString &raw : QString::fromUtf8(file.readAll()).split(u'\n')) {
        const auto m = line.match(raw);
        if (!m.hasMatch())
            continue;
        const int indent = int(m.captured(1).size());
        const QString key = m.captured(2).trimmed();
        const QString value = unquote(m.captured(3).trimmed());
        if (indent == 0) {
            section = key;
        } else if (indent == 2) {
            id = key;
            if (section == u"inbox"_s)
                ledger.inbox.insert(id);
        } else if (section == u"projects"_s) {
            if (key == u"path"_s)
                ledger.projects[id].path = value;
            else if (key == u"sha"_s)
                ledger.projects[id].sha = value;
        }
    }
    // A path the file was edited to point outside the notes, or into Jot's own folder, is
    // forgotten, so the project is imported as new rather than written there.
    ledger.projects.removeIf([](const auto &entry) {
        const QString &path = entry.value().path;
        const QString clean = QDir::cleanPath(path);
        return path.isEmpty() || QDir::isAbsolutePath(path) || clean != path ||
               clean.startsWith(u"../"_s) || clean == u".."_s || clean.startsWith(u".jot/"_s) ||
               !clean.endsWith(u".md"_s);
    });
    return ledger;
}

QString ledgerText(const Ledger &ledger)
{
    QString out =
        u"# Written by Jot's Todoist import, so a later import knows what came from where.\n"_s;
    out += u"projects:\n"_s;
    QStringList ids = ledger.projects.keys();
    std::sort(ids.begin(), ids.end());
    for (const QString &id : std::as_const(ids)) {
        const auto &n = ledger.projects[id];
        out += u"  %1:\n    path: %2\n    sha: %3\n"_s.arg(id, yamlScalar(n.path), n.sha);
    }
    out += u"inbox:\n"_s;
    QStringList inbox(ledger.inbox.cbegin(), ledger.inbox.cend());
    std::sort(inbox.begin(), inbox.end());
    for (const QString &id : std::as_const(inbox))
        out += u"  %1: true\n"_s.arg(id);
    return out;
}

/// A label as a #tag: its spaces and other characters a tag cannot hold become hyphens.
QString tagFor(const QString &label)
{
    static const QRegularExpression other(uR"([^\p{L}\p{N}_/-]+)"_s);
    QString tag = label.simplified().replace(other, u"-"_s);
    while (tag.startsWith(u'-'))
        tag.remove(0, 1);
    while (tag.endsWith(u'-'))
        tag.chop(1);
    return tag;
}

/// A due day as Jot writes it: "2026-11-20", or "2026-11-20T14:00" with a time, one fixed in a
/// time zone said as it is there.
QString dueFor(const TodoistData::Task &task)
{
    if (task.due.size() <= 10)
        return task.due;
    if (!task.due.endsWith(u'Z'))
        return task.due.left(16);
    QDateTime at = QDateTime::fromString(task.due, Qt::ISODate);
    if (!at.isValid())
        return task.due.left(10);
    const QTimeZone zone(task.dueTimeZone.toUtf8());
    at = zone.isValid() ? at.toTimeZone(zone) : at.toLocalTime();
    return at.toString(u"yyyy-MM-ddTHH:mm"_s);
}

/// A description's lines as they go under a to-do: a heading becomes bold words, so the note's
/// outline holds only its sections.
QStringList descriptionLines(const QString &description)
{
    static const QRegularExpression heading(uR"(^(\s*)#{1,6}\s+(.*?)\s*#*\s*$)"_s);
    QStringList lines = description.split(u'\n');
    while (!lines.isEmpty() && lines.last().trimmed().isEmpty())
        lines.removeLast();
    for (QString &line : lines) {
        if (line.endsWith(u'\r'))
            line.chop(1);
        if (const auto m = heading.match(line); m.hasMatch()) {
            const QString words = m.captured(2);
            const bool bold =
                words.startsWith(u"**"_s) && words.endsWith(u"**"_s) && words.size() > 4;
            line = m.captured(1) + (bold ? words : u"**"_s + words + u"**"_s);
        }
    }
    return lines;
}

/// Writes a project's or the Inbox's tasks as Markdown to-dos.
class Writer
{
public:
    explicit Writer(const TodoistData &data, const QDate &today) : m_today(today)
    {
        for (const auto &task : data.tasks) {
            if (!task.checked)
                m_open.insert(task.id, &task);
        }
        for (const auto &task : data.tasks) {
            if (task.checked)
                continue;
            // A subtask whose parent is done or gone stands at the top of its project.
            const QString parent = m_open.contains(task.parentId) ? task.parentId : QString();
            m_children[parent].append(&task);
        }
        for (auto &list : m_children)
            std::stable_sort(list.begin(), list.end(),
                             [](const auto *a, const auto *b) { return a->order < b->order; });
        for (const auto &comment : data.comments)
            m_comments[comment.taskId].append(&comment);
    }

    /// The tasks at the top of `projectId`, in its sections or none, in Todoist's order.
    [[nodiscard]] QList<const TodoistData::Task *> topOf(const QString &projectId) const
    {
        QList<const TodoistData::Task *> out;
        for (const auto *task : m_children.value(QString())) {
            if (task->projectId == projectId)
                out.append(task);
        }
        return out;
    }

    /// `task` and its subtasks as lines, `depth` levels in.
    void write(const TodoistData::Task &task, int depth, QStringList &out, int &count) const
    {
        const QString indent(2 * depth, u' ');
        const QString inside(2 * depth + 2, u' ');
        QString line = indent + u"- [ ] "_s + task.content.simplified();
        const int priority = std::clamp(5 - task.priority, 1, 4);
        line += u' ' + priorityMarker(priority);
        QStringList notes;
        if (!task.due.isEmpty())
            line += u' ' + dueMarker(dueFor(task));
        if (task.recurring && !task.dueString.isEmpty()) {
            // A rule Jot can follow repeats; another is kept in Todoist's words.
            if (nextOccurrence(task.dueString, m_today).isValid())
                line += u' ' + repeatMarker(task.dueString);
            else
                notes.append(QObject::tr("Repeats %1").arg(task.dueString.simplified()));
        }
        for (const QString &label : task.labels) {
            if (const QString tag = tagFor(label); !tag.isEmpty())
                line += u" #"_s + tag;
        }
        out.append(line);
        ++count;
        if (!task.deadline.isEmpty())
            notes.append(QObject::tr("Deadline %1").arg(task.deadline));
        if (const int n = task.durationAmount; n > 0) {
            if (task.durationUnit == u"day"_s)
                notes.append(n == 1 ? QObject::tr("Takes 1 day")
                                    : QObject::tr("Takes %1 days").arg(n));
            else
                notes.append(n == 1 ? QObject::tr("Takes 1 minute")
                                    : QObject::tr("Takes %1 minutes").arg(n));
        }
        for (const QString &note : std::as_const(notes))
            out.append(inside + note);
        for (const QString &text : descriptionLines(task.description))
            out.append(text.trimmed().isEmpty() ? QString() : inside + text);
        for (const auto *comment : m_comments.value(task.id)) {
            for (const QString &text : descriptionLines(comment->content))
                out.append(text.trimmed().isEmpty() ? QString() : inside + text);
            if (!comment->fileUrl.isEmpty())
                out.append(inside + u"[%1](<%2>)"_s.arg(comment->fileName.isEmpty()
                                                            ? comment->fileUrl
                                                            : comment->fileName,
                                                        comment->fileUrl));
        }
        for (const auto *child : m_children.value(task.id))
            write(*child, depth + 1, out, count);
    }

    /// A subtask's ids and its own subtasks', for one written as a whole.
    void ids(const TodoistData::Task &task, QStringList &out) const
    {
        out.append(task.id);
        for (const auto *child : m_children.value(task.id))
            ids(*child, out);
    }

private:
    QDate m_today;
    QHash<QString, const TodoistData::Task *> m_open;
    QHash<QString, QList<const TodoistData::Task *>> m_children;
    QHash<QString, QList<const TodoistData::Comment *>> m_comments;
};

} // namespace

std::optional<QByteArray> fetchTodoistSync(const QString &token, QString *error, const QUrl &api,
                                           bool *refused)
{
    if (refused)
        *refused = false;
    if (token.trimmed().isEmpty()) {
        *error = QObject::tr("no Todoist API token");
        return std::nullopt;
    }
    QNetworkAccessManager network;
    QNetworkRequest request(api);
    // A large account answers slowly, but one that never answers should not hang the import.
    request.setTransferTimeout(120 * 1000);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
    request.setRawHeader("Authorization", "Bearer " + token.trimmed().toUtf8());
    QUrlQuery form;
    form.addQueryItem(u"sync_token"_s, u"*"_s);
    form.addQueryItem(u"resource_types"_s, uR"(["projects","sections","items","notes"])"_s);
    QNetworkReply *reply = network.post(request, form.query(QUrl::FullyEncoded).toUtf8());
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QByteArray body = reply->readAll();
    const QNetworkReply::NetworkError failure = reply->error();
    const QString why = reply->errorString();
    reply->deleteLater();
    if (status == 401 || status == 403) {
        if (refused)
            *refused = true;
        *error = QObject::tr("Todoist did not take the API token; copy it again from Todoist's "
                             "Settings, Integrations, Developer");
        return std::nullopt;
    }
    if (status == 0 || (failure != QNetworkReply::NoError && status < 400)) {
        *error = QObject::tr("could not reach Todoist: %1").arg(why);
        return std::nullopt;
    }
    if (status != 200) {
        *error = QObject::tr("Todoist answered with error %1").arg(status);
        return std::nullopt;
    }
    return body;
}

std::optional<TodoistData> readTodoistSync(const QByteArray &json, QString *error)
{
    QJsonParseError parse;
    const QJsonDocument document = QJsonDocument::fromJson(json, &parse);
    if (!document.isObject() || !document.object().contains(u"projects"_s)) {
        *error = parse.error != QJsonParseError::NoError
                     ? QObject::tr("Todoist's reply is not JSON: %1").arg(parse.errorString())
                     : QObject::tr("Todoist's reply has no projects");
        return std::nullopt;
    }
    const QJsonObject root = document.object();
    TodoistData data;
    for (const QJsonValue &value : root.value(u"projects"_s).toArray()) {
        const QJsonObject o = value.toObject();
        if (o.value(u"is_deleted"_s).toBool())
            continue;
        data.projects.append({idOf(o.value(u"id"_s)), o.value(u"name"_s).toString(),
                              idOf(o.value(u"parent_id"_s)), o.value(u"child_order"_s).toInt(),
                              o.value(u"inbox_project"_s).toBool(),
                              o.value(u"is_archived"_s).toBool()});
    }
    for (const QJsonValue &value : root.value(u"sections"_s).toArray()) {
        const QJsonObject o = value.toObject();
        if (o.value(u"is_deleted"_s).toBool())
            continue;
        data.sections.append({idOf(o.value(u"id"_s)), idOf(o.value(u"project_id"_s)),
                              o.value(u"name"_s).toString(), o.value(u"section_order"_s).toInt(),
                              o.value(u"is_archived"_s).toBool()});
    }
    for (const QJsonValue &value : root.value(u"items"_s).toArray()) {
        const QJsonObject o = value.toObject();
        if (o.value(u"is_deleted"_s).toBool())
            continue;
        TodoistData::Task task;
        task.id = idOf(o.value(u"id"_s));
        task.projectId = idOf(o.value(u"project_id"_s));
        task.sectionId = idOf(o.value(u"section_id"_s));
        task.parentId = idOf(o.value(u"parent_id"_s));
        task.content = o.value(u"content"_s).toString();
        task.description = o.value(u"description"_s).toString();
        task.priority = o.value(u"priority"_s).toInt(1);
        const QJsonObject due = o.value(u"due"_s).toObject();
        task.due = due.value(u"date"_s).toString();
        task.dueTimeZone = due.value(u"timezone"_s).toString();
        task.dueString = due.value(u"string"_s).toString();
        task.recurring = due.value(u"is_recurring"_s).toBool();
        task.deadline = o.value(u"deadline"_s).toObject().value(u"date"_s).toString();
        const QJsonObject duration = o.value(u"duration"_s).toObject();
        task.durationAmount = duration.value(u"amount"_s).toInt();
        task.durationUnit = duration.value(u"unit"_s).toString();
        for (const QJsonValue &label : o.value(u"labels"_s).toArray())
            task.labels.append(label.toString());
        task.order = o.value(u"child_order"_s).toInt();
        task.checked = o.value(u"checked"_s).toBool();
        data.tasks.append(task);
    }
    for (const QJsonValue &value : root.value(u"notes"_s).toArray()) {
        const QJsonObject o = value.toObject();
        if (o.value(u"is_deleted"_s).toBool())
            continue;
        const QJsonObject file = o.value(u"file_attachment"_s).toObject();
        data.comments.append({idOf(o.value(u"item_id"_s)), o.value(u"content"_s).toString(),
                              file.value(u"file_name"_s).toString(),
                              file.value(u"file_url"_s).toString()});
    }
    return data;
}

int TodoistImport::count(Outcome outcome) const
{
    return int(std::count_if(notes.cbegin(), notes.cend(),
                             [&](const Note &n) { return n.outcome == outcome; }));
}

std::optional<TodoistImport> importTodoist(const Vault &vault, const TodoistData &from, bool dryRun,
                                           QString *error)
{
    if (!dryRun && !vault.writable()) {
        *error = QObject::tr("this vault is from a newer Jot, so this one only reads it");
        return std::nullopt;
    }
    Ledger ledger = readLedger(vault.absolute(kLedger));
    TodoistImport result;
    const Writer writer(from, QDate::currentDate());
    QSet<QString> taken;
    const auto exists = [&](const QString &path) {
        return taken.contains(path) || QFileInfo::exists(vault.absolute(path));
    };
    const auto read = [&](const QString &path) {
        QString ignored;
        return notefile::read(vault.absolute(path), &ignored).value_or(QString());
    };

    QHash<QString, const TodoistData::Project *> projects;
    for (const auto &project : from.projects)
        projects.insert(project.id, &project);
    // Each project's folder, its parents' names in turn under Task Lists.
    std::function<QString(const TodoistData::Project &, int)> folderOf =
        [&](const TodoistData::Project &project, int depth) -> QString {
        const auto *parent = projects.value(project.parentId);
        if (!parent || parent->inbox || depth > 32)
            return kTaskListsFolder;
        return join(folderOf(*parent, depth + 1), Vault::fileNameFor(parent->name));
    };

    int archived = 0;
    QList<const TodoistData::Project *> ordered;
    for (const auto &project : from.projects) {
        if (project.archived)
            ++archived;
        else if (!project.inbox)
            ordered.append(&project);
    }
    std::stable_sort(ordered.begin(), ordered.end(), [&](const auto *a, const auto *b) {
        const QString fa = folderOf(*a, 0);
        const QString fb = folderOf(*b, 0);
        return fa != fb ? fa < fb : a->order < b->order;
    });

    QHash<QString, QList<const TodoistData::Section *>> sectionsOf;
    QSet<QString> archivedSections;
    for (const auto &section : from.sections) {
        if (section.archived)
            archivedSections.insert(section.id);
        else
            sectionsOf[section.projectId].append(&section);
    }
    for (auto &list : sectionsOf)
        std::stable_sort(list.begin(), list.end(),
                         [](const auto *a, const auto *b) { return a->order < b->order; });

    const auto saveLedger = [&](QString *why) {
        if (dryRun)
            return true;
        QDir().mkpath(vault.absolute(u".jot"_s));
        return notefile::write(vault.absolute(kLedger), ledgerText(ledger), why);
    };
    const auto fail = [&](const QString &why) -> std::optional<TodoistImport> {
        QString ignored;
        saveLedger(&ignored);
        *error = why;
        return std::nullopt;
    };
    const auto makeFolder = [&](const QString &path) {
        QString at;
        for (const QString &part : path.split(u'/')) {
            at = join(at, part);
            if (QFileInfo(vault.absolute(at)).isDir() || taken.contains(at))
                continue;
            if (!dryRun && !QDir().mkpath(vault.absolute(at)))
                return false;
            taken.insert(at);
            result.notebooks.append(at);
        }
        return true;
    };

    for (const auto *project : std::as_const(ordered)) {
        QStringList lines;
        int count = 0;
        QSet<QString> sectionIds;
        for (const auto *section : sectionsOf.value(project->id))
            sectionIds.insert(section->id);
        // Tasks in no section, or one Todoist did not send, come first; an archived section's
        // stay out, as Todoist hides them.
        for (const auto *task : writer.topOf(project->id)) {
            if (!sectionIds.contains(task->sectionId) &&
                !archivedSections.contains(task->sectionId))
                writer.write(*task, 0, lines, count);
        }
        for (const auto *section : sectionsOf.value(project->id)) {
            if (!lines.isEmpty())
                lines.append(QString());
            lines.append(u"## "_s + section->name.simplified());
            lines.append(QString());
            for (const auto *task : writer.topOf(project->id)) {
                if (task->sectionId == section->id)
                    writer.write(*task, 0, lines, count);
            }
        }
        QString text = lines.join(u'\n') + u'\n';
        const QString title =
            project->name.trimmed().isEmpty() ? QObject::tr("Untitled") : project->name.trimmed();
        if (Vault::fileNameFor(title) != title)
            text = setFrontMatterKey(text, u"title"_s, title);

        TodoistImport::Note note{project->id, title, {}, TodoistImport::Outcome::Created, count};
        if (ledger.projects.contains(project->id)) {
            const auto &before = ledger.projects[project->id];
            note.path = before.path;
            taken.insert(before.path);
            if (!QFileInfo::exists(vault.absolute(before.path))) {
                note.outcome = TodoistImport::Outcome::Removed;
            } else {
                const QString now = read(before.path);
                if (sha(now) != before.sha)
                    note.outcome = TodoistImport::Outcome::ChangedInJot;
                else if (now == text)
                    note.outcome = TodoistImport::Outcome::Unchanged;
                else
                    note.outcome = TodoistImport::Outcome::Updated;
            }
        } else {
            const QString folder = folderOf(*project, 0);
            if (!makeFolder(folder))
                return fail(QObject::tr("could not make the folder %1").arg(folder));
            const QString base = Vault::fileNameFor(title);
            note.path = join(folder, base + u".md"_s);
            for (int n = 2; exists(note.path); ++n)
                note.path = join(folder, u"%1 %2.md"_s.arg(base).arg(n));
            taken.insert(note.path);
        }
        if (note.outcome == TodoistImport::Outcome::Created ||
            note.outcome == TodoistImport::Outcome::Updated) {
            QString why;
            if (!dryRun && !notefile::write(vault.absolute(note.path), text, &why))
                return fail(QObject::tr("could not write %1: %2").arg(note.path, why));
            ledger.projects[project->id] = {note.path, sha(text)};
        }
        result.notes.append(note);
    }

    // The Inbox's tasks not added before go at the end of the inbox note.
    result.inboxPath = inboxPath(vault);
    QStringList added;
    QStringList addedIds;
    for (const auto &project : from.projects) {
        if (!project.inbox)
            continue;
        for (const auto *task : writer.topOf(project.id)) {
            if (ledger.inbox.contains(task->id))
                continue;
            int count = 0;
            writer.write(*task, 0, added, count);
            writer.ids(*task, addedIds);
            ++result.inboxTasks;
        }
    }
    if (!added.isEmpty()) {
        // An inbox note that is there but cannot be read is left alone, rather than written
        // over with only what this import adds.
        QString text;
        if (QFileInfo::exists(vault.absolute(result.inboxPath))) {
            QString why;
            const std::optional<QString> kept =
                notefile::read(vault.absolute(result.inboxPath), &why);
            if (!kept)
                return fail(QObject::tr("could not read %1: %2").arg(result.inboxPath, why));
            text = *kept;
        }
        const QString newline = text.contains(u"\r\n"_s) ? u"\r\n"_s : u"\n"_s;
        if (!text.isEmpty() && !text.endsWith(u'\n'))
            text += newline;
        text += added.join(newline) + newline;
        const QString folder = QFileInfo(result.inboxPath).path();
        QString why;
        if (!dryRun && ((folder != u"."_s && !QDir().mkpath(vault.absolute(folder))) ||
                        !notefile::write(vault.absolute(result.inboxPath), text, &why)))
            return fail(QObject::tr("could not write %1: %2").arg(result.inboxPath, why));
        for (const QString &id : std::as_const(addedIds))
            ledger.inbox.insert(id);
    }

    if (archived > 0)
        result.skipped.append(archived == 1 ? QObject::tr("1 archived project")
                                            : QObject::tr("%1 archived projects").arg(archived));
    QString why;
    if (!saveLedger(&why)) {
        *error = QObject::tr("could not write %1: %2").arg(kLedger, why);
        return std::nullopt;
    }
    return result;
}

} // namespace jot
