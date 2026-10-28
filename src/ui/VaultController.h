#pragma once

#include "BacklinksModel.h"
#include "NoteListModel.h"
#include "NotebookModel.h"
#include "SwitcherModel.h"
#include "TagModel.h"
#include "TaskModel.h"

#include "jot/Vault.h"
#include "jot/VaultIndex.h"
#include "jot/VaultWatcher.h"

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
    Q_PROPERTY(jot::NotebookModel *notebooks READ notebooks CONSTANT)
    /// The tags, nested ones as a tree that folds.
    Q_PROPERTY(jot::TagModel *tagTree READ tagTree CONSTANT)
    Q_PROPERTY(jot::NoteListModel *notes READ notes CONSTANT)
    Q_PROPERTY(jot::SwitcherModel *switcher READ switcher CONSTANT)
    /// The notes offered while typing a wikilink.
    Q_PROPERTY(jot::SwitcherModel *linkSuggestions READ linkSuggestions CONSTANT)
    /// What is left to do across the vault.
    Q_PROPERTY(jot::TaskModel *tasks READ tasks CONSTANT)
    /// The middle column shows tasks rather than notes.
    Q_PROPERTY(bool showingTasks READ showingTasks WRITE setShowingTasks NOTIFY showingTasksChanged)
    /// What links to, or names, the note shown.
    Q_PROPERTY(jot::BacklinksModel *backlinks READ backlinks CONSTANT)
    /// The note shown, by vault path, and what it says.
    Q_PROPERTY(QString current READ current WRITE setCurrent NOTIFY currentChanged)
    Q_PROPERTY(QString currentTitle READ currentTitle NOTIFY currentChanged)
    Q_PROPERTY(QString currentText READ currentText NOTIFY currentChanged)
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
    /// Gives the note shown a new title, renaming its file and the links to it.
    Q_INVOKABLE bool renameCurrent(const QString &title);
    /// Moves the note shown, and its attachments, to the system trash.
    Q_INVOKABLE bool trashCurrent();
    /// Moves the note at `path`, and its attachments, to the system trash.
    Q_INVOKABLE bool trashNote(const QString &path);
    /// Moves the notebook at `path`, and everything in it, to the system trash.
    Q_INVOKABLE bool trashNotebook(const QString &path);
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
    /// A whole checkbox line ticked done today: with the next one above it, when it repeats.
    Q_INVOKABLE QString completedTaskLine(const QString &line) const;
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
    /// Shows the note at `path` with the cursor on `line`.
    Q_INVOKABLE void openAt(const QString &path, int line);
    /// Imports the Joplin export at `file` into the notebook `folder`, or with `dryRun` says
    /// what it would do. The map has `ok` and `error`, the counts `created`, `updated`,
    /// `unchanged`, `notebooks` and `attachments`, and the lists `keptEdits` (notes left as
    /// edited in Jot), `notBroughtBack` and `skipped`.
    Q_INVOKABLE QVariantMap importJoplin(const QUrl &file, const QString &folder, bool dryRun);
    /// How long typing must pause before edits are written, in ms.
    void setSaveDelay(int ms)
    {
        m_saveDelay = ms;
        m_saveTimer.setInterval(ms);
    }
    /// Index the database at `path` rather than the default one, as tests do.
    void setIndexPath(const QString &path) { m_indexPath = path; }
    /// Trashes notes into the vault's own trash rather than the desktop's, as tests do.
    void setSystemTrash(bool use)
    {
        m_systemTrash = use;
        if (m_vault)
            m_vault->setSystemTrash(use);
    }

    [[nodiscard]] bool isOpen() const { return m_vault != nullptr; }
    [[nodiscard]] QString root() const;
    [[nodiscard]] QUrl rootUrl() const { return isOpen() ? QUrl::fromLocalFile(root()) : QUrl(); }
    [[nodiscard]] QString name() const;
    [[nodiscard]] bool writable() const;
    [[nodiscard]] QString error() const { return m_error; }
    [[nodiscard]] int noteCount() const;
    [[nodiscard]] QStringList tags() const;
    [[nodiscard]] QVariantList labels() const;
    [[nodiscard]] QVariantMap notebookEmoji() const { return m_notebookEmoji; }
    [[nodiscard]] NotebookModel *notebooks() { return &m_notebooks; }
    [[nodiscard]] TagModel *tagTree() { return &m_tagTree; }
    [[nodiscard]] NoteListModel *notes() { return &m_notes; }
    [[nodiscard]] SwitcherModel *switcher() { return &m_switcher; }
    [[nodiscard]] SwitcherModel *linkSuggestions() { return &m_linkSuggestions; }
    [[nodiscard]] BacklinksModel *backlinks() { return &m_backlinks; }
    [[nodiscard]] TaskModel *tasks() { return &m_tasks; }
    [[nodiscard]] bool showingTasks() const { return m_showingTasks; }
    void setShowingTasks(bool showing);
    [[nodiscard]] QString current() const { return m_current; }
    void setCurrent(const QString &path);
    [[nodiscard]] QString currentTitle() const;
    [[nodiscard]] QString currentText() const { return m_currentText; }
    [[nodiscard]] bool unsaved() const { return m_unsaved; }
    [[nodiscard]] QString problem() const { return m_problem; }
    [[nodiscard]] const VaultIndex *index() const { return m_index.get(); }

Q_SIGNALS:
    void openChanged();
    void errorChanged();
    void notesChanged();
    void currentChanged();
    /// Another note is about to be shown, just before currentChanged(). Not sent when the
    /// note shown is renamed or moved, which keeps it the same note.
    void opening();
    void unsavedChanged();
    void problemChanged();
    void showingTasksChanged();
    /// The editor should put the cursor here in the note shown, as the editor counts
    /// characters: line breaks as one, without a byte order mark.
    void revealPosition(int position);

private:
    void close();
    void notesChangedOnDisk(const QStringList &paths);
    void reloadModels();
    void readCurrent();
    void fail(const QString &error);
    void setUnsaved(bool unsaved);
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
    /// Replaces the whole text of the note at `path`, through the editor's copy when shown.
    bool rewriteNote(const QString &path,
                     const std::function<std::optional<QString>(const QString &)> &change);
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
    bool m_showingTasks = false;
    QVariantMap m_notebookEmoji;
    QStringList m_pinned;
    QString m_indexPath;
    bool m_systemTrash = true;
    QString m_error;
    QString m_current;
    QString m_currentText;
    /// The note shown as it was last read from or written to its file, which outside edits
    /// and the user's are merged from.
    QString m_savedText;
    bool m_unsaved = false;
    QString m_problem;
    /// How the note on disk ends its lines and whether it starts with a byte order mark,
    /// which the editor does not keep but the file must.
    bool m_crlf = false;
    bool m_byteOrderMark = false;
    int m_saveDelay = 500;
    QTimer m_saveTimer;
    QTimer m_reloadTimer;
};

} // namespace jot
