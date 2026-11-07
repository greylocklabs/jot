#pragma once

#include "BacklinksModel.h"
#include "NoteListModel.h"
#include "NotebookModel.h"
#include "SwitcherModel.h"
#include "SyncController.h"
#include "TagModel.h"
#include "TaskModel.h"
#include "TrashModel.h"

#include "jot/Layout.h"
#include "jot/NoteTrail.h"
#include "jot/Vault.h"
#include "jot/VaultIndex.h"
#include "jot/VaultWatcher.h"

#include <QFile>
#include <QObject>
#include <QQmlEngine>
#include <QTimer>
#include <QUrl>
#include <QVariantMap>

#include <functional>
#include <memory>
#include <optional>

namespace jot {

class Settings;

/// The `Vault` singleton QML reads: the open vault, its index kept level with the files
/// by a watcher, and the models the sidebar, note list and switcher show.
class VaultController : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Vault)
    QML_SINGLETON

    Q_PROPERTY(bool isOpen READ isOpen NOTIFY openChanged)
    Q_PROPERTY(QString root READ root NOTIFY openChanged)
    /// The same folder, for a folder dialog to start in.
    Q_PROPERTY(QUrl rootUrl READ rootUrl NOTIFY openChanged)
    /// The kinds of data file New can make, as `{kind, label}`.
    Q_PROPERTY(QVariantList dataKinds READ dataKinds CONSTANT)
    /// The vault's folder name, for the title bar.
    Q_PROPERTY(QString name READ name NOTIFY openChanged)
    Q_PROPERTY(bool writable READ writable NOTIFY openChanged)
    /// Why the vault last asked for could not be opened, or empty.
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(int noteCount READ noteCount NOTIFY notesChanged)
    Q_PROPERTY(QStringList tags READ tags NOTIFY notesChanged)
    /// The @labels on open to-dos: `{label, count}` for each, alphabetically.
    Q_PROPERTY(QVariantList labels READ labels NOTIFY notesChanged)
    /// Each notebook's emoji, by its path, for those that have one.
    Q_PROPERTY(QVariantMap notebookEmoji READ notebookEmoji NOTIFY notesChanged)
    /// The emoji before Tasks and Trash in the sidebar, by view, each empty for none.
    Q_PROPERTY(QVariantMap viewEmoji READ viewEmoji NOTIFY notesChanged)
    Q_PROPERTY(jot::NotebookModel *notebooks READ notebooks CONSTANT)
    /// The tags, nested ones as a tree that folds.
    Q_PROPERTY(jot::TagModel *tagTree READ tagTree CONSTANT)
    Q_PROPERTY(jot::NoteListModel *notes READ notes CONSTANT)
    Q_PROPERTY(jot::SwitcherModel *switcher READ switcher CONSTANT)
    /// The notes offered while typing a wikilink.
    Q_PROPERTY(jot::SwitcherModel *linkSuggestions READ linkSuggestions CONSTANT)
    /// What is left to do across the vault.
    Q_PROPERTY(jot::TaskModel *tasks READ tasks CONSTANT)
    /// Where the vault syncs to, and how that stands.
    Q_PROPERTY(jot::SyncController *sync READ sync CONSTANT)
    /// The middle column shows tasks rather than notes.
    Q_PROPERTY(bool showingTasks READ showingTasks WRITE setShowingTasks NOTIFY showingTasksChanged)
    /// What the trash holds from the vault, read again each time Trash is shown.
    Q_PROPERTY(jot::TrashModel *trash READ trash CONSTANT)
    /// The notes in the trash that the note list's search finds, when settings say to look there.
    Q_PROPERTY(jot::TrashModel *trashHits READ trashHits CONSTANT)
    /// The note in the trash Trash shows, by where it is there; empty for none.
    Q_PROPERTY(
        QString chosenTrashed READ chosenTrashed WRITE setChosenTrashed NOTIFY chosenTrashedChanged)
    /// The middle column shows the trash rather than notes or tasks.
    Q_PROPERTY(bool showingTrash READ showingTrash WRITE setShowingTrash NOTIFY showingTrashChanged)
    /// What links to, or names, the note shown.
    Q_PROPERTY(jot::BacklinksModel *backlinks READ backlinks CONSTANT)
    /// The note shown, by vault path, and what it says.
    Q_PROPERTY(QString current READ current WRITE setCurrent NOTIFY currentChanged)
    Q_PROPERTY(QString currentTitle READ currentTitle NOTIFY currentTitleChanged)
    Q_PROPERTY(QString currentText READ currentText NOTIFY currentChanged)
    /// The open file's kind: "markdown" for a note, else its data format: "json", "csv", "tsv"
    /// or "yaml".
    Q_PROPERTY(QString currentKind READ currentKind NOTIFY currentChanged)
    Q_PROPERTY(QVariantList currentConflicts READ currentConflicts NOTIFY currentConflictsChanged)
    /// Whether a to-do ticked from the task view, or a task note completed, can be undone or
    /// redone. Ticks typed in the editor are the editor's own to undo.
    Q_PROPERTY(bool canUndoTask READ canUndoTask NOTIFY taskHistoryChanged)
    Q_PROPERTY(bool canRedoTask READ canRedoTask NOTIFY taskHistoryChanged)
    /// Whether there is a note opened before or after this one to go back or forward to.
    Q_PROPERTY(bool canGoBack READ canGoBack NOTIFY trailChanged)
    Q_PROPERTY(bool canGoForward READ canGoForward NOTIFY trailChanged)
    /// Where the editor has its cursor, which going back to the note later returns to.
    Q_PROPERTY(int cursorPosition MEMBER m_cursorPosition NOTIFY cursorPositionChanged)
    /// Where the cursor goes in the note just opened: where it was, for one gone back or
    /// forward to, and its start otherwise.
    Q_PROPERTY(int openingCursor READ openingCursor NOTIFY currentChanged)
    /// Edits not yet written to the note's file.
    Q_PROPERTY(bool unsaved READ unsaved NOTIFY unsavedChanged)
    /// What went wrong with the note shown, such as a save that failed and is being
    /// retried; empty when all is well.
    Q_PROPERTY(QString problem READ problem NOTIFY problemChanged)

public:
    explicit VaultController(Settings &settings, QObject *parent = nullptr);
    ~VaultController() override;

    /// The process-wide instance, which QML also receives. main() makes it first.
    static VaultController *instance();
    static void setInstance(VaultController *controller);
    static VaultController *create(QQmlEngine *, QJSEngine *);

    /// Opens the folder at `path`, making it a vault if it is not one, and remembers it.
    Q_INVOKABLE bool openPath(const QString &path);
    /// As openPath(), for a folder from a folder dialog.
    Q_INVOKABLE bool openFolder(const QUrl &folder);
    /// Opens the vault used last, if there is one.
    bool openLast();
    /// The text of the note shown, as the user typed it; written out shortly after typing
    /// stops, or at once on saveNow().
    Q_INVOKABLE void edit(const QString &text);
    /// Writes any edits waiting, as before switching notes or quitting.
    Q_INVOKABLE void saveNow();
    /// Makes a note in the notebook shown and shows it. Gives its path.
    Q_INVOKABLE QString newNote(const QString &title = {});
    /// Makes a data file of `kind` ("json", "csv", "tsv" or "yaml") in the notebook shown, with
    /// one example record, and shows it. Gives its path.
    Q_INVOKABLE QString newFile(const QString &kind);
    /// Gives the note shown a new title, renaming its file and the links to it. A note still
    /// empty, named with a data file's extension ("people.csv"), becomes that kind of file.
    Q_INVOKABLE bool renameCurrent(const QString &title);
    /// Moves the note shown, and its attachments, to the trash.
    Q_INVOKABLE bool trashCurrent();
    /// Moves the note at `path`, and its attachments unless `keepAttachments`, to the trash.
    Q_INVOKABLE bool trashNote(const QString &path, bool keepAttachments);
    /// The other notes that link into the attachments of the note at `path`, which trashing
    /// them with it would break.
    Q_INVOKABLE QStringList notesUsingAttachments(const QString &path) const;
    /// Moves the notebook at `path`, and everything in it, to the trash.
    Q_INVOKABLE bool trashNotebook(const QString &path);
    /// Brings the note at `location` in the trash back where it was, or beside a note that has
    /// its place now, and shows it. Gives its vault path, or empty when it could not.
    Q_INVOKABLE QString restoreTrashed(const QString &location);
    /// What the note at `location` in the trash says, to read before bringing it back.
    Q_INVOKABLE QString trashedText(const QString &location) const;
    /// The title of the note at `location` in the trash.
    Q_INVOKABLE QString trashedTitle(const QString &location) const;
    /// Deletes the note at `location` in the trash, and its attachments, for good.
    Q_INVOKABLE bool deleteTrashed(const QString &location);
    /// Deletes everything in the trash for good.
    Q_INVOKABLE bool emptyTrash();
    /// Shows Trash with the note at `location` in it chosen.
    Q_INVOKABLE void showTrashed(const QString &location);
    /// How many notes the notebook at `path` holds, inside notebooks within it too.
    Q_INVOKABLE int notesInNotebook(const QString &path) const;
    /// Makes the note at `path` a task, or no longer one. Its due date stays either way.
    Q_INVOKABLE bool setTask(const QString &path, bool task);
    /// Whether the note at `path` is a task, done or not.
    Q_INVOKABLE bool isTask(const QString &path) const;
    /// A wikilink to the note at `path`: by its name, or its path when another note shares it.
    Q_INVOKABLE QString linkTo(const QString &path) const;
    /// Opens the folder holding the note at `path`, or the notebook at `path`, in Files.
    Q_INVOKABLE void showInFiles(const QString &path) const;
    /// Every notebook's path, folded or not, in the order the sidebar lists them.
    Q_INVOKABLE QStringList notebookPaths() const;
    /// The title of the note at `path`, as the note list shows it.
    Q_INVOKABLE QString titleOf(const QString &path) const;
    /// The notebook the note at `path` is in; empty for the top of the vault.
    Q_INVOKABLE QString notebookOf(const QString &path) const { return Vault::notebookOf(path); }
    /// Keeps the note at `path` at the top of the note list, or lets it go back in order.
    Q_INVOKABLE bool setPinned(const QString &path, bool pinned);
    Q_INVOKABLE bool isPinned(const QString &path) const { return m_pinned.contains(path); }
    /// Shows `emoji` before the notebook's name, or no emoji when it is empty.
    Q_INVOKABLE bool setNotebookEmoji(const QString &path, const QString &emoji);
    /// Shows `emoji` before `view`, "tasks" or "trash", or none when it is empty.
    Q_INVOKABLE bool setViewEmoji(const QString &view, const QString &emoji);
    /// Makes a notebook in `parent`, or at the top for none, and gives its path.
    Q_INVOKABLE QString newNotebook(const QString &parent = {});
    /// Renames the notebook at `path`, rewriting links to the notes in it.
    Q_INVOKABLE bool renameNotebook(const QString &path, const QString &name);
    /// Moves the notebook at `path` into `parent`, or to the top for none, rewriting links.
    /// Moving it where it is, or into itself, does nothing and says false.
    Q_INVOKABLE bool moveNotebook(const QString &path, const QString &parent);
    /// Moves the note at `path`, and its attachments, into `notebook`, rewriting links.
    Q_INVOKABLE bool moveNoteTo(const QString &path, const QString &notebook);
    /// Follows a link written in the note shown: opens a note, makes one a wikilink names
    /// but nothing holds yet, or opens anything else with the desktop.
    Q_INVOKABLE bool follow(const QString &target, bool wiki);
    /// Keeps the clipboard's image as an attachment of the note shown and gives the Markdown
    /// that shows it, or nothing when the clipboard holds no image.
    Q_INVOKABLE QString pasteImage();
    /// Copies files dropped on the note into its attachments and gives the Markdown that
    /// shows images and links the rest, a line each.
    Q_INVOKABLE QString attachFiles(const QList<QUrl> &files);
    /// Where to put `markdown` so it sits on lines of its own at `position` in `text`:
    /// `{position, text, cursor}`, the text padded with the line breaks it needs and the
    /// cursor just after it.
    Q_INVOKABLE QVariantMap placeBlock(const QString &text, int position,
                                       const QString &markdown) const;
    /// How quick add reads `typed`: `{title, due, when, repeat, labels, line}`, where `when` says
    /// the day and time for people, and `line` is the checkbox it would add.
    Q_INVOKABLE QVariantMap readQuickTask(const QString &typed) const;
    /// A checkbox's words, `text`, with the day and repeat they say written as markers, as
    /// quick add does; empty when they say neither.
    Q_INVOKABLE QString scheduledFromWords(const QString &text) const;
    /// Where the words of the checkbox on the line at `position` in `text` are, as `{start,
    /// end, words}`; empty when that line is no checkbox.
    Q_INVOKABLE QVariantMap taskWordsAt(const QString &text, int position) const;
    /// A checkbox's words, `text`, due and repeating as `when` says, such as "friday 3pm every
    /// week", or with neither when `when` is empty; empty when `when` says neither.
    Q_INVOKABLE QString scheduledAs(const QString &text, const QString &when) const;
    /// The day and repeat a checkbox's words, `text`, are marked with, as words quick add reads
    /// back, such as "2026-11-07 15:00 every week"; empty when it has neither.
    Q_INVOKABLE QString scheduleWords(const QString &text) const;
    /// How to tick done today the checkbox on the line holding `position` in the editor's
    /// `text`: `{start, end, text}`, the line's stretch and what replaces it, with the next one
    /// above it when it repeats.
    Q_INVOKABLE QVariantMap tickedLineAt(const QString &text, int position) const;
    /// Adds the to-do `typed` says to the inbox note, making the note if need be.
    Q_INVOKABLE bool addQuickTask(const QString &typed);
    /// Where the editor's cursor at `cursor` in `shown`, the text it has, belongs in the note
    /// shown now that it has changed, such as by an outside edit merged in.
    Q_INVOKABLE int movedCursor(const QString &shown, int cursor) const;
    /// The file an image link in the note shown reaches, or empty for none.
    Q_INVOKABLE QString attachmentPath(const QString &target, bool wiki) const;
    /// What a link in the note shown leads to, for a preview: `{path, title, text}`, where
    /// `text` is the start of the note without its front matter. Empty when it leads to no note.
    Q_INVOKABLE QVariantMap preview(const QString &target) const;
    /// Ticks or unticks the task on `line` of the note at `path`, in its file or, for the
    /// note shown, in the editor's text.
    Q_INVOKABLE bool setTaskDone(const QString &path, int line, bool done,
                                 const QString &text = {});
    /// Marks a note that is a task as done, with when it was.
    Q_INVOKABLE bool completeTask(const QString &path);
    /// Marks a note that is a done task as open again, forgetting when it was done.
    Q_INVOKABLE bool reopenTask(const QString &path);
    /// Takes back the last tick or completion, or does again the last one taken back. Refused,
    /// and forgotten, when the note has changed since.
    Q_INVOKABLE bool undoTask();
    Q_INVOKABLE bool redoTask();
    [[nodiscard]] bool canUndoTask() const { return !m_undoTasks.isEmpty(); }
    [[nodiscard]] bool canRedoTask() const { return !m_redoTasks.isEmpty(); }
    /// Shows the note opened before this one, or after it once gone back, as a browser does.
    Q_INVOKABLE bool goBack();
    Q_INVOKABLE bool goForward();
    [[nodiscard]] bool canGoBack() const { return m_trail.canGoBack(); }
    [[nodiscard]] bool canGoForward() const { return m_trail.canGoForward(); }
    [[nodiscard]] int openingCursor() const { return m_openingCursor; }
    /// Shows the note at `path` with the cursor on `line`.
    Q_INVOKABLE void openAt(const QString &path, int line);
    /// The database of the Joplin app on this computer, or an empty URL when there is none.
    Q_INVOKABLE QUrl joplinProfile() const;
    /// Imports the Joplin export or database at `file` into the notebook `folder`, or with
    /// `dryRun` says what it would do. The map has `ok` and `error`, `source` naming where it
    /// came from, the counts `created`, `updated`, `unchanged`, `notebooks`, `attachments` and
    /// `versions`, and the lists `keptEdits` (notes left as edited in Jot), `notBroughtBack` and
    /// `skipped`.
    Q_INVOKABLE QVariantMap importJoplin(const QUrl &file, const QString &folder, bool dryRun);
    /// The past versions of the note at `path`, newest first, each a map with `path` (the
    /// version's own vault path), `written`, and `label`, when it was written in words.
    Q_INVOKABLE QVariantList historyOf(const QString &path) const;
    /// The text of the past version at vault path `version`.
    Q_INVOKABLE QString versionText(const QString &version) const;
    /// Makes the past version at `version` the current note's text, keeping what it replaces
    /// as a version of its own. When it cannot be written, the note stays as it was.
    Q_INVOKABLE bool restoreVersion(const QString &version);
    /// What changed from `older` to `newer`, each a version's path or empty for the note shown
    /// as it is now: `lines` of `{kind, text}`, kind being "same", "removed" or "added", and
    /// `changes`, how many places changed; or `error`.
    Q_INVOKABLE QVariantMap compareVersions(const QString &older, const QString &newer) const;
    /// Other versions of the note shown, kept beside it when two could not be merged, oldest
    /// first: each a map with `path`, `from`, the computer it came from when known, and `when`.
    [[nodiscard]] QVariantList currentConflicts() const;
    /// The note shown beside its conflict copy at `copy`, as `rows` of `{mine, theirs, change}`,
    /// change being "same", "changed", "mine" or "theirs" for a line in only one, and
    /// `differences`, how many places differ; or `error`.
    Q_INVOKABLE QVariantMap compareWith(const QString &copy) const;
    /// Keeps the note as it is, moving the conflict copy at `copy` to the trash.
    Q_INVOKABLE bool keepThisVersion(const QString &copy);
    /// Replaces the note with its conflict copy at `copy`, which goes to the trash. What it
    /// replaces is kept in the note's history, so it can be undone.
    Q_INVOKABLE bool useOtherVersion(const QString &copy);
    /// How long typing must pause before edits are written, in ms.
    void setSaveDelay(int ms)
    {
        m_saveDelay = ms;
        m_saveTimer.setInterval(ms);
    }
    /// The least time between two versions kept of a note while it is edited, in ms.
    void setHistoryInterval(int ms) { m_historyInterval = ms; }
    /// Index the database at `path` rather than the default one, as tests do.
    void setIndexPath(const QString &path) { m_indexPath = path; }
    /// How notes leave for the desktop's trash, which tests replace.
    void setDiscard(VaultTrash::Discard discard) { m_discard = std::move(discard); }

    [[nodiscard]] bool isOpen() const { return m_vault != nullptr; }
    [[nodiscard]] QString root() const;
    [[nodiscard]] QUrl rootUrl() const { return isOpen() ? QUrl::fromLocalFile(root()) : QUrl(); }
    [[nodiscard]] static QVariantList dataKinds();
    [[nodiscard]] QString name() const;
    [[nodiscard]] bool writable() const;
    [[nodiscard]] QString error() const { return m_error; }
    [[nodiscard]] int noteCount() const;
    [[nodiscard]] QStringList tags() const;
    [[nodiscard]] QVariantList labels() const;
    [[nodiscard]] QVariantMap notebookEmoji() const { return m_notebookEmoji; }
    [[nodiscard]] QVariantMap viewEmoji() const { return m_viewEmoji; }
    [[nodiscard]] NotebookModel *notebooks() { return &m_notebooks; }
    [[nodiscard]] TagModel *tagTree() { return &m_tagTree; }
    [[nodiscard]] NoteListModel *notes() { return &m_notes; }
    [[nodiscard]] SwitcherModel *switcher() { return &m_switcher; }
    [[nodiscard]] SwitcherModel *linkSuggestions() { return &m_linkSuggestions; }
    [[nodiscard]] BacklinksModel *backlinks() { return &m_backlinks; }
    [[nodiscard]] TaskModel *tasks() { return &m_tasks; }
    [[nodiscard]] SyncController *sync() { return &m_sync; }
    [[nodiscard]] bool showingTasks() const { return m_showingTasks; }
    void setShowingTasks(bool showing);
    [[nodiscard]] TrashModel *trash() { return &m_trash; }
    [[nodiscard]] TrashModel *trashHits() { return &m_trashHits; }
    [[nodiscard]] QString chosenTrashed() const { return m_chosenTrashed; }
    void setChosenTrashed(const QString &location);
    [[nodiscard]] bool showingTrash() const { return m_showingTrash; }
    void setShowingTrash(bool showing);
    [[nodiscard]] QString current() const { return m_current; }
    void setCurrent(const QString &path);
    [[nodiscard]] QString currentTitle() const;
    [[nodiscard]] QString currentKind() const;
    [[nodiscard]] QString currentText() const { return m_currentText; }
    [[nodiscard]] bool unsaved() const { return m_unsaved; }
    [[nodiscard]] QString problem() const { return m_problem; }
    [[nodiscard]] const VaultIndex *index() const { return m_index.get(); }

Q_SIGNALS:
    void openChanged();
    void errorChanged();
    void notesChanged();
    void currentChanged();
    /// The note shown changed by replacing `length` characters at `start` with `text`, just
    /// before currentChanged(), so the editor can make the same edit and keep its undo history.
    void currentPatched(int start, int length, const QString &text);
    void taskHistoryChanged();
    void trailChanged();
    void cursorPositionChanged();
    /// With every currentChanged(), and when saving changes the note's title, as editing its
    /// `title:` key does.
    void currentTitleChanged();
    /// The note shown gained or lost a conflict copy, or another note is shown.
    void currentConflictsChanged();
    /// Another note is about to be shown, just before currentChanged(). Not sent when the
    /// note shown is renamed or moved, which keeps it the same note.
    void opening();
    void unsavedChanged();
    void problemChanged();
    void showingTasksChanged();
    void showingTrashChanged();
    void chosenTrashedChanged();
    /// The editor should put the cursor here in the note shown, as the editor counts
    /// characters: line breaks as one, without a byte order mark.
    void revealPosition(int position);

private:
    void close();
    void notesChangedOnDisk(const QStringList &paths);
    void reloadModels();
    /// Reads the trash again for Trash.
    void reloadTrash();
    /// Hands notes in the trash longer than settings keep them to the desktop's trash.
    void expireTrash();
    /// Searches the trash again for what the note list's search finds there.
    void updateTrashHits();
    void readCurrent();
    void fail(const QString &error);
    void setUnsaved(bool unsaved);
    /// Keeps `before`, the text a write to the note at `path` replaces, as one of its past
    /// versions: always with `now`, otherwise once per history interval.
    void keepHistory(const QString &path, const QString &before, bool now = false);
    /// Forgets when versions were kept of the note or notebook at `path`, which has moved or
    /// gone, so a note later there keeps its first version.
    void forgetHistoryKept(const QString &path);
    void setProblem(const QString &problem);
    /// After a rename that did not finish, shows whichever of the two paths now holds
    /// the note, so nothing writes to one that is gone.
    void resyncAfterRename(const QString &from, const QString &to);
    /// Moves a notebook from one path to another, and what refers to it with it.
    bool relocateNotebook(const QString &from, const QString &to);
    /// After a notebook moved: the folded notebooks, the list's notebook, the note shown and
    /// the watcher follow it, and notes in `written` are read again.
    void followNotebook(const QString &from, const QString &to, const QStringList &written);
    /// After notes were moved or rewritten: shows the note shown where it went, read again.
    void afterMove(const QString &current, const QStringList &written);
    /// Goes back (`by` -1) or forward (1) through the notes opened.
    bool travel(int by);
    /// Puts `text` in place of the note shown, keeping what it replaces in the note's history.
    bool replaceCurrentText(const QString &text);
    /// Replaces the whole text of the note at `path`, through the editor's copy when shown.
    /// With `undoable`, the change joins the task history.
    bool rewriteNote(const QString &path,
                     const std::function<std::optional<QString>(const QString &)> &change,
                     bool undoable = false);
    /// Changes the note at `path` from `from` to `to`, for undo and redo. `stale` says whether it
    /// failed because the note is gone or no longer reads `from`, so the step can never apply.
    bool replaceTaskText(const QString &path, const QString &from, const QString &to, bool *stale);
    /// Brings what another program wrote to the note shown into the edits not yet saved, or,
    /// where both changed the same lines, keeps it beside the note as a copy.
    void takeOutsideEdit(const QString &theirs);
    /// Keeps what another program wrote to the note shown beside it, as the user's own
    /// edits are about to replace it.
    void keepConflict(const QString &theirs);
    void scheduleModelReload();

    Settings &m_settings;
    std::unique_ptr<Vault> m_vault;
    std::unique_ptr<VaultIndex> m_index;
    std::unique_ptr<VaultWatcher> m_watcher;
    NotebookModel m_notebooks;
    TagModel m_tagTree;
    NoteListModel m_notes;
    SwitcherModel m_switcher;
    SwitcherModel m_linkSuggestions;
    BacklinksModel m_backlinks;
    TaskModel m_tasks;
    TrashModel m_trash;
    TrashModel m_trashHits;
    /// Everything in the trash, as last read.
    QList<TrashedNote> m_trashed;
    QString m_chosenTrashed;
    VaultTrash::Discard m_discard = [](const QString &path) { return QFile::moveToTrash(path); };
    /// Hands old notes in the trash to the desktop's, when settings say to.
    QTimer m_expireTimer;
    bool m_showingTrash = false;
    SyncController m_sync{m_settings};
    bool m_showingTasks = false;
    QVariantMap m_notebookEmoji;
    QVariantMap m_viewEmoji;
    QStringList m_pinned;
    QString m_indexPath;
    QString m_error;
    QString m_current;
    QString m_currentText;
    /// The note shown as it was last read from or written to its file, which outside edits
    /// and the user's are merged from.
    QString m_savedText;
    bool m_unsaved = false;
    QString m_problem;
    /// Whether the note on disk starts with a byte order mark, which the editor does not keep
    /// but the file must. Its line endings are kept from the text it had.
    bool m_byteOrderMark = false;
    int m_saveDelay = 500;
    int m_historyInterval = 10 * 60 * 1000;
    /// When a version of each note was last kept while running.
    QHash<QString, QDateTime> m_historyKept;
    struct TaskChange
    {
        QString path;
        QString before;
        QString after;
    };
    QList<TaskChange> m_undoTasks;
    QList<TaskChange> m_redoTasks;
    NoteTrail m_trail;
    /// Going back or forward, which is no new visit.
    bool m_travelling = false;
    int m_cursorPosition = 0;
    int m_openingCursor = 0;
    QTimer m_saveTimer;
    QTimer m_reloadTimer;
};

} // namespace jot
