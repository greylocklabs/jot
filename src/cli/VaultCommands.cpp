#include "VaultCommands.h"

#include "jot/Fuzzy.h"
#include "jot/JoplinExport.h"
#include "jot/JoplinImport.h"
#include "jot/Layout.h"
#include "jot/QuickAdd.h"
#include "jot/Settings.h"
#include "jot/ToDos.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

#include <algorithm>

using namespace Qt::StringLiterals;

namespace jot::cli {

namespace {

QString when(qint64 modified)
{
    return QDateTime::fromMSecsSinceEpoch(modified / 1000000).toString(u"yyyy-MM-dd HH:mm"_s);
}

void print(QTextStream &out, const QJsonValue &value)
{
    out << (value.isArray() ? QJsonDocument(value.toArray()) : QJsonDocument(value.toObject()))
               .toJson(QJsonDocument::Indented);
}

/// A notebook named on the command line, if it is one inside the vault.
bool insideVault(const QString &notebook)
{
    const QString clean = QDir::cleanPath(notebook);
    return notebook.isEmpty() ||
           (clean != u".." && !clean.startsWith(u"../"_s) && !QDir::isAbsolutePath(clean));
}

} // namespace

VaultCommands::VaultCommands(const Vault &vault, VaultIndex &index, QTextStream &out,
                             QTextStream &err)
    : m_vault(vault), m_index(index), m_out(out), m_err(err)
{}

int VaultCommands::status(const QString &indexPath, bool json)
{
    const QList<NoteInfo> notes = m_index.notesIn({});
    const int notebooks = int(std::ranges::count_if(m_vault.entries(), &VaultEntry::folder));
    const int tasks = int(m_index.openTasks().size());
    const int taskNotes = int(m_index.openTaskNotes().size());
    if (json) {
        print(m_out, QJsonObject{{u"vault"_s, m_vault.root()},
                                 {u"writable"_s, m_vault.writable()},
                                 {u"notes"_s, int(notes.size())},
                                 {u"notebooks"_s, notebooks},
                                 {u"tags"_s, int(m_index.tags().size())},
                                 {u"openTasks"_s, tasks},
                                 {u"openTaskNotes"_s, taskNotes},
                                 {u"index"_s, indexPath}});
        return 0;
    }
    const auto row = [this](const QString &label, const QString &value) {
        m_out << m_paint.bold(label.leftJustified(12)) << value << "\n";
    };
    row(tr("Vault"), m_vault.root() + (m_vault.writable() ? QString() : tr(" (read only)")));
    row(tr("Notes"),
        (notes.size() == 1 ? tr("1 note") : tr("%1 notes").arg(notes.size())) +
            (notebooks == 1 ? tr(" in 1 notebook") : tr(" in %1 notebooks").arg(notebooks)));
    row(tr("Tags"), QString::number(m_index.tags().size()));
    row(tr("Tasks"), (taskNotes == 1 ? tr("1 task note") : tr("%1 task notes").arg(taskNotes)) +
                         u", "_s + (tasks == 1 ? tr("1 task") : tr("%1 tasks").arg(tasks)));
    row(tr("Index"), indexPath);
    return 0;
}

int VaultCommands::list(const QString &notebook, const QString &order, bool json)
{
    if (!order.isEmpty() && !Settings::noteSorts().contains(order)) {
        m_err << tr("usage: jot ls [notebook] [--order updated|created|title]") << "\n";
        return 2;
    }
    if (!insideVault(notebook)) {
        m_err << tr("jot: %1 is not a notebook in this vault").arg(notebook) << "\n";
        return 2;
    }
    if (!notebook.isEmpty() && !QFileInfo(m_vault.absolute(notebook)).isDir()) {
        m_err << tr("jot: no notebook called %1").arg(notebook) << "\n";
        return 1;
    }
    QList<NoteInfo> notes = m_index.notesIn(notebook);
    // Without the pins when the layout cannot be read, which is said once.
    QString error;
    const std::optional<Layout> layout = Layout::read(m_vault, &error);
    if (!layout)
        m_err << tr("jot: %1").arg(error) << "\n";
    const QStringList pinned = layout ? layout->pinned() : QStringList();
    sortNotes(notes, order.isEmpty() ? u"updated"_s : order, pinned);
    if (json) {
        QJsonArray array;
        for (const NoteInfo &note : std::as_const(notes))
            array.append(QJsonObject{{u"path"_s, note.path},
                                     {u"title"_s, note.title},
                                     {u"tags"_s, QJsonArray::fromStringList(note.tags)},
                                     {u"modified"_s, when(note.modified)},
                                     {u"pinned"_s, pinned.contains(note.path)}});
        print(m_out, array);
        return 0;
    }
    for (const NoteInfo &note : std::as_const(notes))
        m_out << m_paint.dim(when(note.modified)) << "  " << m_paint.bold(note.title) << "  "
              << m_paint.dim(note.path) << "\n";
    return 0;
}

int VaultCommands::search(const QString &query, bool json)
{
    if (query.trimmed().isEmpty()) {
        m_err << tr("usage: jot search <words> [tag:name] [in:notebook] [task:open]") << "\n";
        return 2;
    }
    const QList<SearchHit> hits = m_index.search(query);
    if (json) {
        QJsonArray array;
        for (const SearchHit &hit : hits) {
            QString snippet = hit.snippet;
            snippet.remove(u"<mark>"_s).remove(u"</mark>"_s);
            array.append(QJsonObject{
                {u"path"_s, hit.path}, {u"title"_s, hit.title}, {u"snippet"_s, snippet}});
        }
        print(m_out, array);
        return 0;
    }
    static const QRegularExpression mark(u"<mark>(.*?)</mark>"_s);
    for (const SearchHit &hit : hits) {
        m_out << m_paint.bold(hit.title) << "  " << m_paint.dim(hit.path) << "\n";
        if (hit.snippet.isEmpty())
            continue;
        QString snippet = hit.snippet.simplified();
        QString shown;
        qsizetype from = 0;
        for (const QRegularExpressionMatch &m : mark.globalMatch(snippet)) {
            shown += snippet.mid(from, m.capturedStart() - from) + m_paint.accent(m.captured(1));
            from = m.capturedEnd();
        }
        m_out << "    " << shown + snippet.mid(from) << "\n";
    }
    if (hits.isEmpty())
        m_err << tr("Nothing matches.") << "\n";
    return 0;
}

int VaultCommands::create(const QString &title, const QString &notebook, const QString &body,
                          bool json)
{
    if (title.trimmed().isEmpty()) {
        m_err << tr("usage: jot new <title> [--notebook name]") << "\n";
        return 2;
    }
    if (!insideVault(notebook)) {
        m_err << tr("jot: %1 is not a notebook in this vault").arg(notebook) << "\n";
        return 2;
    }
    QString error;
    const std::optional<QString> path =
        m_vault.createNote(notebook, title.trimmed(), QDateTime::currentDateTimeUtc(), &error);
    if (!path) {
        m_err << tr("jot: could not make the note: %1").arg(error) << "\n";
        return 1;
    }
    if (!body.isEmpty()) {
        // Read back, so the front matter it was made with stays above the body.
        const std::optional<QString> made = notefile::read(m_vault.absolute(*path), &error);
        if (!made) {
            m_err << tr("jot: could not read the note: %1").arg(error) << "\n";
            return 1;
        }
        const QString text = *made + (body.endsWith(u'\n') ? body : body + u'\n');
        if (!notefile::write(m_vault.absolute(*path), text, &error)) {
            m_err << tr("jot: could not write the note: %1").arg(error) << "\n";
            return 1;
        }
    }
    m_index.update({*path});
    if (json)
        print(m_out, QJsonObject{{u"path"_s, *path}, {u"file"_s, m_vault.absolute(*path)}});
    else
        m_out << *path << "\n";
    return 0;
}

int VaultCommands::addTask(const QString &typed, bool json)
{
    const QuickTask task = readQuickTask(typed, QDateTime::currentDateTime());
    if (task.title.isEmpty()) {
        m_err << tr("usage: jot add <to-do, with a day, time, repeat, @labels and #tags>") << "\n";
        return 2;
    }
    if (!m_vault.writable()) {
        m_err << tr("jot: this vault is from a newer Jot, so this one only reads it") << "\n";
        return 1;
    }
    const QString path = inboxPath(m_vault);
    QString error;
    const QString file = m_vault.absolute(path);
    QString note;
    if (QFileInfo::exists(file)) {
        const std::optional<QString> read = notefile::read(file, &error);
        if (!read) {
            m_err << tr("jot: could not read %1: %2").arg(path, error) << "\n";
            return 1;
        }
        note = *read;
    }
    if (!QDir().mkpath(QFileInfo(file).path()) ||
        !notefile::write(file, withTaskAdded(note, task.line), &error)) {
        m_err << tr("jot: could not add to %1: %2").arg(path, error) << "\n";
        return 1;
    }
    m_index.update({path});
    if (json)
        print(m_out, QJsonObject{{u"path"_s, path},
                                 {u"line"_s, task.line},
                                 {u"title"_s, task.title},
                                 {u"due"_s, task.due},
                                 {u"repeat"_s, task.repeat}});
    else
        m_out << path << ": - [ ] " << task.line << "\n";
    return 0;
}

QString VaultCommands::find(const QString &name) const
{
    if (name.isEmpty())
        return {};
    // A path or a name as a link would write it; else the closest title.
    const QString linked = m_index.resolve(name, {});
    if (linked.endsWith(u".md"_s))
        return linked;
    QString best;
    int bestScore = 0;
    for (const NoteInfo &note : m_index.notesIn({})) {
        for (const QString &candidate : QStringList{note.title} + note.aliases) {
            const std::optional<int> score = fuzzyScore(name, candidate);
            if (score && (best.isEmpty() || *score > bestScore)) {
                best = note.path;
                bestScore = *score;
            }
        }
    }
    return best;
}

int VaultCommands::show(const QString &name)
{
    const QString path = find(name);
    if (path.isEmpty()) {
        m_err << tr("jot: no note called %1").arg(name) << "\n";
        return 1;
    }
    QString error;
    const std::optional<QString> text = notefile::read(m_vault.absolute(path), &error);
    if (!text) {
        m_err << tr("jot: could not read %1: %2").arg(path, error) << "\n";
        return 1;
    }
    m_out << *text;
    return 0;
}

int VaultCommands::listTasks(bool json, const QString &view, const QString &label,
                             const QString &tag)
{
    const std::optional<ToDoView> shown = toDoView(view.isEmpty() ? u"all"_s : view);
    if (!shown) {
        m_err << tr("usage: jot tasks [--view today|upcoming|anytime|all] [--label name] "
                    "[--tag name]")
              << "\n";
        return 2;
    }
    const QDate today = QDate::currentDate();
    // Written with their mark or without: @minutes or minutes, #calls or calls.
    const QList<ToDo> list =
        toDos(m_index, *shown, today, label.startsWith(u'@') ? label.mid(1) : label,
              tag.startsWith(u'#') ? tag.mid(1) : tag);
    if (json) {
        QJsonArray array;
        for (const ToDo &each : list)
            array.append(QJsonObject{{u"kind"_s, each.kind},
                                     {u"path"_s, each.path},
                                     {u"line"_s, each.line},
                                     {u"title"_s, each.title},
                                     {u"note"_s, each.note},
                                     {u"due"_s, each.due},
                                     {u"repeat"_s, each.repeat},
                                     {u"labels"_s, QJsonArray::fromStringList(each.labels)},
                                     {u"tags"_s, QJsonArray::fromStringList(each.tags)}});
        print(m_out, array);
        return 0;
    }
    // Under the day it is due, or the note it is in, as the app lists them.
    QString last;
    for (const ToDo &each : list) {
        const QDate due = each.dueDate();
        const QString group = due.isValid() && due < today ? tr("Overdue")
                              : due == today               ? tr("Today")
                              : due.isValid()              ? due.toString(Qt::ISODate)
                              : each.kind == u"note"       ? tr("Task notes")
                                                           : each.path;
        if (group != last) {
            m_out << m_paint.dim(group) << "\n";
            last = group;
        }
        QStringList extra;
        for (const QString &each : each.labels)
            extra << u'@' + each;
        if (!each.repeat.isEmpty())
            extra << each.repeat;
        if (due.isValid() && due != today)
            extra << (due < today ? m_paint.danger(each.due) : each.due);
        else if (due.isValid() && each.due.size() > 10)
            extra << each.due.mid(11);
        m_out << "  [ ] " << (each.kind == u"note" ? m_paint.bold(each.title) : each.title)
              << (extra.isEmpty() ? QString() : u"  "_s + m_paint.dim(extra.join(u"  "_s))) << "\n";
    }
    if (list.isEmpty())
        m_err << tr("Nothing left to do.") << "\n";
    return 0;
}

int VaultCommands::open(const QString &name, const QString &program, const Launcher &launch)
{
    const QString path = find(name);
    if (path.isEmpty()) {
        m_err << tr("jot: no note called %1").arg(name) << "\n";
        return 1;
    }
    if (program.isEmpty()) {
        m_err << tr("jot: jot-gui is not installed") << "\n";
        return 1;
    }
    if (!launch(program, {u"--vault"_s, m_vault.root(), u"--note"_s, path})) {
        m_err << tr("jot: could not start %1").arg(program) << "\n";
        return 1;
    }
    return 0;
}

int VaultCommands::listTags(bool json)
{
    const QList<TagCount> tags = m_index.tagCounts();
    if (json) {
        QJsonArray array;
        for (const TagCount &each : tags)
            array.append(QJsonObject{{u"tag"_s, each.tag}, {u"notes"_s, each.notes}});
        print(m_out, array);
        return 0;
    }
    for (const TagCount &each : tags)
        m_out << m_paint.bold(each.tag) << "  " << m_paint.dim(QString::number(each.notes)) << "\n";
    return 0;
}

int VaultCommands::importJoplin(const QString &file, const QString &notebook, bool dryRun,
                                bool json)
{
    // With nothing named, Joplin's own database on this computer, history and all.
    const QString source = file.isEmpty() ? joplinProfileDatabase() : file;
    if (source.isEmpty()) {
        m_err << tr("jot: Joplin's database is not on this computer, so name an export: %1")
                     .arg(tr("usage: jot import joplin [export.jex | database.sqlite | profile "
                             "folder] [--notebook name] [--dry-run]"))
              << "\n";
        return 2;
    }
    if (!insideVault(notebook)) {
        m_err << tr("jot: %1 is not a notebook in this vault").arg(notebook) << "\n";
        return 2;
    }
    QString error;
    const std::optional<JoplinExport> from = readJoplin(source, &error);
    if (!from) {
        m_err << tr("jot: could not read %1: %2").arg(source, error) << "\n";
        return 1;
    }
    // "." and "a/.." are the top of the vault, which the importer calls "".
    const QString folder = QDir::cleanPath(notebook);
    const std::optional<JoplinImport> done =
        jot::importJoplin(m_vault, *from, folder == u"." ? QString() : folder, dryRun, &error);
    if (!done) {
        m_err << tr("jot: %1").arg(error) << "\n";
        return 1;
    }
    if (!dryRun)
        m_index.refresh();

    using Outcome = JoplinImport::Outcome;
    const auto name = [](Outcome outcome) {
        switch (outcome) {
        case Outcome::Created: return u"created"_s;
        case Outcome::Updated: return u"updated"_s;
        case Outcome::Unchanged: return u"unchanged"_s;
        case Outcome::ChangedInJot: return u"changedInJot"_s;
        case Outcome::Removed: return u"removed"_s;
        }
        return QString();
    };
    if (json) {
        QJsonArray notes;
        for (const auto &note : done->notes)
            notes.append(QJsonObject{{u"path"_s, note.path},
                                     {u"title"_s, note.title},
                                     {u"outcome"_s, name(note.outcome)}});
        print(m_out, QJsonObject{{u"dryRun"_s, dryRun},
                                 {u"notes"_s, notes},
                                 {u"notebooks"_s, QJsonArray::fromStringList(done->notebooks)},
                                 {u"attachments"_s, done->attachments},
                                 {u"skipped"_s, QJsonArray::fromStringList(done->skipped)}});
        return 0;
    }

    const auto plural = [](int n, const QString &one, const QString &many) {
        return n == 1 ? one : many.arg(n);
    };
    const int created = done->count(Outcome::Created);
    const int updated = done->count(Outcome::Updated);
    m_out << m_paint.bold(dryRun ? tr("Would import from %1").arg(QFileInfo(file).fileName())
                                 : tr("Imported from %1").arg(QFileInfo(file).fileName()))
          << "\n";
    m_out << "  " << plural(created, tr("1 new note"), tr("%1 new notes")) << ", "
          << plural(updated, tr("1 updated"), tr("%1 updated")) << ", "
          << plural(int(done->notebooks.size()), tr("1 new notebook"), tr("%1 new notebooks"))
          << ", " << plural(done->attachments, tr("1 attachment"), tr("%1 attachments")) << "\n";
    if (const int same = done->count(Outcome::Unchanged))
        m_out << "  "
              << plural(same, tr("1 note unchanged since the last import"),
                        tr("%1 notes unchanged since the last import"))
              << "\n";
    // What was left alone is worth naming, so nothing seems lost.
    for (const auto &note : done->notes) {
        if (note.outcome == Outcome::ChangedInJot)
            m_out << "  " << tr("Kept your edits to %1").arg(m_paint.bold(note.path)) << "\n";
        else if (note.outcome == Outcome::Removed)
            m_out
                << "  "
                << tr("Not brought back, as it was removed or moved and edited: %1").arg(note.path)
                << "\n";
    }
    for (const QString &why : done->skipped)
        m_out << "  " << m_paint.dim(tr("Left out: %1").arg(why)) << "\n";
    if (dryRun)
        m_out << m_paint.dim(tr("Nothing was written; run it again without --dry-run to import."))
              << "\n";
    return 0;
}

} // namespace jot::cli
