#include "VaultCommands.h"

#include "jot/Fuzzy.h"
#include "jot/JoplinExport.h"
#include "jot/JoplinImport.h"

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
    if (!order.isEmpty() && order != u"updated" && order != u"title") {
        m_err << tr("usage: jot ls [notebook] [--order updated|title]") << "\n";
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
    if (order == u"title")
        std::ranges::sort(notes, [](const NoteInfo &a, const NoteInfo &b) {
            return naturalCompare(a.title, b.title) < 0;
        });
    else
        std::ranges::sort(
            notes, [](const NoteInfo &a, const NoteInfo &b) { return a.modified > b.modified; });
    if (json) {
        QJsonArray array;
        for (const NoteInfo &note : std::as_const(notes))
            array.append(QJsonObject{{u"path"_s, note.path},
                                     {u"title"_s, note.title},
                                     {u"tags"_s, QJsonArray::fromStringList(note.tags)},
                                     {u"modified"_s, when(note.modified)}});
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

int VaultCommands::listTasks(bool json)
{
    const QList<NoteInfo> notes = m_index.openTaskNotes();
    const QList<NoteTask> tasks = m_index.openTasks();
    if (json) {
        QJsonArray array;
        for (const NoteInfo &note : notes)
            array.append(QJsonObject{{u"kind"_s, u"note"_s},
                                     {u"path"_s, note.path},
                                     {u"title"_s, note.title},
                                     {u"due"_s, note.due}});
        for (const NoteTask &task : tasks)
            array.append(QJsonObject{{u"kind"_s, u"task"_s},
                                     {u"path"_s, task.path},
                                     {u"line"_s, task.line},
                                     {u"text"_s, task.text}});
        print(m_out, array);
        return 0;
    }
    for (const NoteInfo &note : notes) {
        const bool overdue = isOverdue(note.due, QDate::currentDate());
        m_out << "[ ] " << m_paint.bold(note.title)
              << (note.due.isEmpty() ? QString()
                                     : u"  "_s + (overdue ? m_paint.danger(note.due.left(10))
                                                          : m_paint.dim(note.due.left(10))))
              << "\n";
    }
    QString last;
    for (const NoteTask &task : tasks) {
        if (task.path != last) {
            m_out << m_paint.dim(task.path) << "\n";
            last = task.path;
        }
        m_out << "  [ ] " << task.text << m_paint.dim(u"  :%1"_s.arg(task.line)) << "\n";
    }
    if (notes.isEmpty() && tasks.isEmpty())
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
    const QStringList tags = m_index.tags();
    if (json) {
        QJsonArray array;
        for (const QString &tag : tags)
            array.append(
                QJsonObject{{u"tag"_s, tag}, {u"notes"_s, int(m_index.tagged(tag).size())}});
        print(m_out, array);
        return 0;
    }
    for (const QString &tag : tags)
        m_out << m_paint.bold(tag) << "  "
              << m_paint.dim(QString::number(m_index.tagged(tag).size())) << "\n";
    return 0;
}

int VaultCommands::importJoplin(const QString &file, const QString &notebook, bool dryRun,
                                bool json)
{
    if (file.isEmpty()) {
        m_err << tr("usage: jot import joplin <export.jex> [--notebook name] [--dry-run]") << "\n";
        return 2;
    }
    if (!insideVault(notebook)) {
        m_err << tr("jot: %1 is not a notebook in this vault").arg(notebook) << "\n";
        return 2;
    }
    QString error;
    const std::optional<JoplinExport> from = readJoplinExport(file, &error);
    if (!from) {
        m_err << tr("jot: could not read %1: %2").arg(file, error) << "\n";
        return 1;
    }
    const std::optional<JoplinImport> done = jot::importJoplin(
        m_vault, *from, notebook.isEmpty() ? QString() : QDir::cleanPath(notebook), dryRun, &error);
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
