#pragma once

#include <QByteArray>
#include <QList>
#include <QString>
#include <QStringList>

#include <optional>

namespace jot {

class Vault;

/// The parts of a Todoist account an import reads, as its sync API gives them.
struct TodoistData
{
    struct Project
    {
        QString id;
        QString name;
        /// The project it is inside; empty at the top.
        QString parentId;
        int order = 0;
        /// Todoist's Inbox, whose tasks go to the vault's inbox note.
        bool inbox = false;
        bool archived = false;
    };
    struct Section
    {
        QString id;
        QString projectId;
        QString name;
        int order = 0;
        bool archived = false;
    };
    struct Task
    {
        QString id;
        QString projectId;
        /// Empty for one outside any section.
        QString sectionId;
        /// The task it is a subtask of; empty for one at the top.
        QString parentId;
        QString content;
        QString description;
        /// Todoist's number: 4 is what it shows as p1, 1 is p4.
        int priority = 1;
        /// "2026-11-20", "2026-11-20T14:00:00" for a time wherever one is, or
        /// "2026-11-20T19:00:00Z" for a time fixed in `dueTimeZone`; empty when not due.
        QString due;
        QString dueTimeZone;
        /// How the due day was said, "every monday" and the like.
        QString dueString;
        bool recurring = false;
        /// "2026-11-30"; empty without one.
        QString deadline;
        int durationAmount = 0;
        /// "minute" or "day".
        QString durationUnit;
        QStringList labels;
        int order = 0;
        bool checked = false;
    };
    struct Comment
    {
        QString taskId;
        QString content;
        QString fileName;
        QString fileUrl;
    };
    QList<Project> projects;
    QList<Section> sections;
    QList<Task> tasks;
    QList<Comment> comments;
};

/// Reads the JSON of a full sync, `/api/v1/sync` with `sync_token=*`, leaving out what was
/// deleted. Nothing when it is not one, with `error` set.
[[nodiscard]] std::optional<TodoistData> readTodoistSync(const QByteArray &json, QString *error);

/// What importing a Todoist account into a vault did, or would do in a dry run.
///
/// Each project becomes a note in the Task Lists notebook, with its sections as headings and its
/// tasks as to-dos; a project inside another is a note in a notebook named for that one. Tasks in
/// Todoist's Inbox go to the end of the vault's inbox note, each only once. Importing again is
/// safe: `.jot/todoist.yaml` records which note each project became and what it held, so a later
/// import updates notes Todoist changed, leaves alone ones edited in Jot since, and never brings
/// a second copy.
struct TodoistImport
{
    enum class Outcome {
        /// A new note.
        Created,
        /// Changed in Todoist since the last import, and not in Jot, so brought up to date.
        Updated,
        /// The same as the last import brought.
        Unchanged,
        /// Edited in Jot since the last import, so left as it is.
        ChangedInJot,
        /// Imported before, then removed in Jot, so not brought back.
        Removed,
    };
    struct Note
    {
        QString projectId;
        QString title;
        QString path;
        Outcome outcome = Outcome::Created;
        /// Its open tasks, subtasks included.
        int tasks = 0;
    };
    QList<Note> notes;
    /// Notebook folders made, by vault path.
    QStringList notebooks;
    /// The vault's inbox note, and how many of the Inbox's tasks were added to it this time.
    QString inboxPath;
    int inboxTasks = 0;
    /// What was left out, in words: archived projects and the like.
    QStringList skipped;

    [[nodiscard]] int count(Outcome outcome) const;
};

/// The notebook a Todoist import puts its project notes in.
inline const QString kTaskListsFolder = QStringLiteral("Task Lists");

/// Imports `from` into `vault`. Completed tasks are left out. With `dryRun` nothing is written.
/// Nothing when the vault cannot be written, with `error` set.
[[nodiscard]] std::optional<TodoistImport>
importTodoist(const Vault &vault, const TodoistData &from, bool dryRun, QString *error);

} // namespace jot
