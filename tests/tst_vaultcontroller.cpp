#include "VaultController.h"

#include "JexBuilder.h"

#include "jot/Settings.h"

#include <QClipboard>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QGuiApplication>
#include <QImage>
#include <QRegularExpression>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <unistd.h>

using namespace jot;
using namespace Qt::StringLiterals;

class TestVaultController : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void init();
    void cleanup();

    void startsClosed();
    void opensAFolderAndRemembersIt();
    void refusesWhatIsNotAFolder();
    void switchesToAnotherFolder();
    void staysWhenTheNoteShownCannotBeSaved();
    void listsNotebooksThatFold();
    void listsTagsThatFold();
    void listsNotesByNotebookTagAndSearch();
    void ordersNotesAsAsked();
    void followsTheOrderInSettings();
    void pinsNotesToTheTop();
    void switcherFindsByTitleAliasAndPath();
    void makesRenamesAndMovesNotebooks();
    void givesNotebooksAnEmoji();
    void movesANoteToAnotherNotebook();
    void followsANotebookThatMovedThoughALinkCouldNotBeRewritten();
    void actsOnNotesThatAreNotShown();
    void trashesANotebook();
    void suggestsLinksThatResolve();
    void showsTheCurrentNote();
    void followsChangesOnDisk();
    void savesShortlyAfterTypingStops();
    void savesBeforeSwitchingNotes();
    void keepsPastVersionsWhileEditing();
    void restoresAPastVersion();
    void keepsTheOtherVersionWhenBothChanged();
    void mergesOutsideEditsToOtherLines();
    void mergesKeepingLineEndingsAndTheByteOrderMark();
    void makesNewNotesInTheNotebookShown();
    void renamingRewritesLinksAndKeepsTheTitle();
    void trashesTheNoteShown();
    void followsLinksAndMakesMissingNotes();
    void keepsLineEndingsAndTheByteOrderMark();
    void aRenameThatFailsHalfwayShowsWhereTheNoteIs();
    void aFailedSaveIsShownAndRetried();
    void attachesDroppedFilesAndPastedImages();
    void listsBacklinksAndMentions();
    void previewsWhatALinkLeadsTo();
    void listsAndTicksWhatIsLeftToDo();
    void headsToDosByDay();
    void addsToDosToTheInbox();
    void importsFromJoplinAfterSayingWhatItWouldDo();
    void anImportShowsWhatItWroteToTheNoteOpen();
    void anImportThatFailsPartwayStillShowsWhatItWrote();
    void nothingIsImportedOverTypingThatCannotBeSaved();

private:
    QStringList column(const QAbstractItemModel &model, const QByteArray &role) const;

    std::unique_ptr<QTemporaryDir> m_dir;
    std::unique_ptr<Settings> m_settings;
    std::unique_ptr<VaultController> m_vault;
    QString m_root;
};

namespace {

QString readFile(const QString &path)
{
    QString error;
    return notefile::read(path, &error).value_or(error);
}

} // namespace

void TestVaultController::init()
{
    m_dir = std::make_unique<QTemporaryDir>();
    m_root = m_dir->filePath(u"Notes"_s);
    const QString sample = QStringLiteral(JOT_SAMPLE_VAULT);
    QDirIterator files(sample, QDir::Files, QDirIterator::Subdirectories);
    while (files.hasNext()) {
        const QString from = files.next();
        const QString to = m_root + from.mid(sample.size());
        QVERIFY(QDir().mkpath(QFileInfo(to).path()));
        QVERIFY(QFile::copy(from, to));
    }
    m_settings = std::make_unique<Settings>(m_dir->filePath(u"settings.ini"_s));
    m_vault = std::make_unique<VaultController>(*m_settings);
    m_vault->setIndexPath(m_dir->filePath(u"index.sqlite"_s));
    m_vault->setSystemTrash(false);
}

void TestVaultController::cleanup()
{
    m_vault.reset();
}

QStringList TestVaultController::column(const QAbstractItemModel &model,
                                        const QByteArray &role) const
{
    const int id = model.roleNames().key(role);
    QStringList values;
    for (int row = 0; row < model.rowCount(); ++row)
        values.append(model.data(model.index(row, 0), id).toString());
    return values;
}

void TestVaultController::startsClosed()
{
    QVERIFY(!m_vault->isOpen());
    QVERIFY(!m_vault->openLast());
    QCOMPARE(m_vault->notebooks()->rowCount(), 0);
    QCOMPARE(m_vault->notes()->rowCount(), 0);
}

void TestVaultController::opensAFolderAndRemembersIt()
{
    QSignalSpy opened(m_vault.get(), &VaultController::openChanged);
    QVERIFY(m_vault->openFolder(QUrl::fromLocalFile(m_root)));
    QCOMPARE(opened.size(), 1);
    QVERIFY(m_vault->isOpen());
    QCOMPARE(m_vault->name(), u"Notes"_s);
    QCOMPARE(m_vault->noteCount(), 8);
    QCOMPARE(m_settings->vaultPath(), m_root);

    VaultController again(*m_settings);
    again.setIndexPath(m_dir->filePath(u"index.sqlite"_s));
    m_vault.reset();
    QVERIFY(again.openLast());
    QCOMPARE(again.noteCount(), 8);
}

void TestVaultController::refusesWhatIsNotAFolder()
{
    QFile file(m_dir->filePath(u"file"_s));
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.close();
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(u"could not open the vault.*"_s));
    QVERIFY(!m_vault->openPath(file.fileName()));
    QVERIFY(!m_vault->isOpen());
    QVERIFY(!m_vault->error().isEmpty());
}

void TestVaultController::switchesToAnotherFolder()
{
    QVERIFY(m_vault->openPath(m_root));
    m_vault->setCurrent(u"Welcome.md"_s);
    m_vault->edit(m_vault->currentText() + u"Typed just before switching.\n"_s);
    const QString other = m_dir->filePath(u"Other"_s);
    QVERIFY(QDir().mkpath(other));
    QFile note(other + u"/Only.md"_s);
    QVERIFY(note.open(QIODevice::WriteOnly));
    note.write("Just one note.\n");
    note.close();
    QSignalSpy current(m_vault.get(), &VaultController::currentChanged);

    m_vault->setIndexPath(m_dir->filePath(u"other.sqlite"_s));
    QVERIFY(m_vault->openPath(other));

    QCOMPARE(m_vault->name(), u"Other"_s);
    QCOMPARE(m_vault->noteCount(), 1);
    QCOMPARE(m_settings->vaultPath(), other);
    QVERIFY(m_vault->current().isEmpty());
    QVERIFY(!current.isEmpty());
    QString error;
    QVERIFY(notefile::read(m_root + u"/Welcome.md"_s, &error)
                ->endsWith(u"Typed just before switching.\n"_s));
}

void TestVaultController::staysWhenTheNoteShownCannotBeSaved()
{
    QVERIFY(m_vault->openPath(m_root));
    m_vault->setCurrent(u"Inbox/Groceries.md"_s);
    // The folder taken away, as a drive that drops out: no user, root included, can save.
    QVERIFY(QDir().rename(m_root + u"/Inbox"_s, m_root + u"/.away"_s));
    const QString typed = m_vault->currentText() + u"- [ ] eggs\n"_s;
    m_vault->edit(typed);
    const QString other = m_dir->filePath(u"Other"_s);
    QVERIFY(QDir().mkpath(other));

    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(u".*could not save.*"_s));
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(u"could not open the vault.*"_s));
    QVERIFY(!m_vault->openPath(other));

    QCOMPARE(m_vault->root(), m_root);
    QCOMPARE(m_vault->current(), u"Inbox/Groceries.md"_s);
    QCOMPARE(m_vault->currentText(), typed);
    QVERIFY(m_vault->unsaved());
    QVERIFY(!m_vault->error().isEmpty());
    QCOMPARE(m_settings->vaultPath(), m_root);
}

void TestVaultController::listsNotebooksThatFold()
{
    QVERIFY(m_vault->openPath(m_root));
    NotebookModel &notebooks = *m_vault->notebooks();
    QCOMPARE(column(notebooks, "path"),
             (QStringList{QString(), u"Ashfall"_s, u"Ashfall/Characters"_s, u"Ashfall/Places"_s,
                          u"Inbox"_s, u"Recipes"_s}));
    QCOMPARE(column(notebooks, "count"),
             (QStringList{u"8"_s, u"4"_s, u"2"_s, u"1"_s, u"1"_s, u"2"_s}));
    QCOMPARE(column(notebooks, "depth").at(2), u"1"_s);

    notebooks.toggle(u"Ashfall"_s);
    QCOMPARE(column(notebooks, "path"),
             (QStringList{QString(), u"Ashfall"_s, u"Inbox"_s, u"Recipes"_s}));
    QCOMPARE(m_settings->collapsedNotebooks(), QStringList{u"Ashfall"_s});
    notebooks.toggle(u"Ashfall"_s);
    QCOMPARE(notebooks.rowCount(), 6);
}

void TestVaultController::listsTagsThatFold()
{
    QVERIFY(m_vault->openPath(m_root));
    TagModel &tags = *m_vault->tagTree();
    QCOMPARE(column(tags, "tag"), (QStringList{u"ashfall"_s, u"baking"_s, u"cast"_s, u"cast/lead"_s,
                                               u"ferry"_s, u"food"_s}));
    const int cast = int(column(tags, "tag").indexOf(u"cast"_s));
    QCOMPARE(column(tags, "hasChildren").at(cast), u"true"_s);
    QCOMPARE(column(tags, "name").at(cast + 1), u"lead"_s);
    QCOMPARE(column(tags, "depth").at(cast + 1), u"1"_s);

    tags.toggle(u"cast"_s);
    QVERIFY(!column(tags, "tag").contains(u"cast/lead"_s));
    QCOMPARE(column(tags, "expanded").at(cast), u"false"_s);
    QCOMPARE(m_settings->collapsedTags(), QStringList{u"cast"_s});
    tags.toggle(u"cast"_s);
    QVERIFY(column(tags, "tag").contains(u"cast/lead"_s));
    // Folded under another spelling, it still opens.
    m_settings->setCollapsedTags({u"Cast"_s});
    tags.toggle(u"cast"_s);
    QVERIFY(m_settings->collapsedTags().isEmpty());
    QVERIFY(column(tags, "tag").contains(u"cast/lead"_s));

    // A parent tag lists the notes nested in it too.
    m_vault->notes()->setTag(u"cast"_s);
    QVERIFY(column(*m_vault->notes(), "path").contains(u"Ashfall/Characters/Mara Vey.md"_s));
}

void TestVaultController::listsNotesByNotebookTagAndSearch()
{
    QVERIFY(m_vault->openPath(m_root));
    NoteListModel &notes = *m_vault->notes();
    QCOMPARE(notes.rowCount(), 8);

    notes.setNotebook(u"Ashfall/Characters"_s);
    QCOMPARE(notes.rowCount(), 2);
    notes.setTag(u"cast"_s);
    QCOMPARE(notes.notebook(), QString());
    QCOMPARE(notes.rowCount(), 2);
    notes.setTag(u"food"_s);
    QCOMPARE(notes.rowCount(), 2);

    // A search stays inside the notebook or tag shown.
    notes.setQuery(u"starter"_s);
    QCOMPARE(column(notes, "path"), QStringList{u"Recipes/Sourdough.md"_s});
    notes.setNotebook(u"Ashfall"_s);
    QCOMPARE(notes.rowCount(), 0);
    // A quote left open neither swallows the notebook nor the words.
    notes.setNotebook(u"Recipes"_s);
    notes.setQuery(u"\"starter"_s);
    QCOMPARE(column(notes, "path"), QStringList{u"Recipes/Sourdough.md"_s});
    notes.setNotebook(u"Ashfall"_s);
    notes.setQuery(u"waxed ledger"_s);
    QCOMPARE(column(notes, "path"), QStringList{u"Ashfall/Characters/Mara Vey.md"_s});
    QVERIFY(column(notes, "snippet").first().contains(u"<mark>"_s));
}

void TestVaultController::ordersNotesAsAsked()
{
    QVERIFY(m_vault->openPath(m_root));
    NoteListModel &notes = *m_vault->notes();
    notes.setNotebook(u"Recipes"_s);
    notes.setOrder(u"title"_s);
    QCOMPARE(column(notes, "title"), (QStringList{u"Pancakes"_s, u"Sourdough"_s}));
    QCOMPARE(m_settings->noteSort(), u"title"_s);

    // Recently changed first.
    QTest::qWait(20);
    QFile touched(m_root + u"/Recipes/Pancakes.md"_s);
    QVERIFY(touched.open(QIODevice::Append));
    touched.write("more\n");
    touched.close();
    notes.setOrder(u"updated"_s);
    QSignalSpy changed(m_vault.get(), &VaultController::notesChanged);
    QVERIFY(changed.wait(3000));
    QCOMPARE(column(notes, "title").first(), u"Pancakes"_s);
}

void TestVaultController::followsTheOrderInSettings()
{
    QVERIFY(m_vault->openPath(m_root));
    m_vault->notes()->setOrder(u"title"_s);
    QCOMPARE(m_settings->noteSort(), u"title"_s);
    m_settings->reset();
    QCOMPARE(m_vault->notes()->order(), u"updated"_s);
}

void TestVaultController::pinsNotesToTheTop()
{
    QVERIFY(m_vault->openPath(m_root));
    NoteListModel &notes = *m_vault->notes();
    notes.setNotebook(u"Recipes"_s);
    notes.setOrder(u"title"_s);
    QVERIFY(m_vault->setPinned(u"Recipes/Sourdough.md"_s, true));
    QVERIFY(m_vault->isPinned(u"Recipes/Sourdough.md"_s));
    QCOMPARE(column(notes, "title"), (QStringList{u"Sourdough"_s, u"Pancakes"_s}));
    QCOMPARE(column(notes, "pinned"), (QStringList{u"true"_s, u"false"_s}));

    // Moved, it stays pinned; unpinned, it goes back in order.
    QVERIFY(m_vault->moveNoteTo(u"Recipes/Sourdough.md"_s, u"Ashfall"_s));
    QVERIFY(m_vault->isPinned(u"Ashfall/Sourdough.md"_s));
    QVERIFY(m_vault->setPinned(u"Ashfall/Sourdough.md"_s, false));
    QVERIFY(!m_vault->isPinned(u"Ashfall/Sourdough.md"_s));

    notes.setOrder(u"created"_s);
    QCOMPARE(m_settings->noteSort(), u"created"_s);
    notes.setOrder(u"oldest"_s);
    QCOMPARE(notes.order(), u"created"_s);
}

void TestVaultController::suggestsLinksThatResolve()
{
    // A second Pancakes makes the title ambiguous, so links to both say their path.
    QVERIFY(QDir().mkpath(m_root + u"/Breakfast"_s));
    QFile twin(m_root + u"/Breakfast/Pancakes.md"_s);
    QVERIFY(twin.open(QIODevice::WriteOnly));
    twin.write("Another stack.\n");
    twin.close();
    // A front matter title the file name cannot hold: links still use the file name.
    QFile titled(m_root + u"/Inbox/meeting-2026.md"_s);
    QVERIFY(titled.open(QIODevice::WriteOnly));
    titled.write("---\ntitle: Team sync\n---\nAgenda.\n");
    titled.close();
    QVERIFY(m_vault->openPath(m_root));
    SwitcherModel &links = *m_vault->linkSuggestions();

    links.setQuery(u"team sync"_s);
    const QVariantMap meeting = links.suggestions(8).first().toMap();
    QCOMPARE(meeting.value(u"text"_s).toString(), u"Team sync"_s);
    QCOMPARE(meeting.value(u"target"_s).toString(), u"meeting-2026"_s);
    QCOMPARE(m_vault->index()->resolve(u"meeting-2026"_s, {}), u"Inbox/meeting-2026.md"_s);

    links.setQuery(u"ferrywoman"_s);
    const QVariantMap mara = links.suggestions(8).first().toMap();
    QCOMPARE(mara.value(u"text"_s).toString(), u"Mara Vey"_s);
    QCOMPARE(mara.value(u"detail"_s).toString(), u"the Ferrywoman"_s);
    QCOMPARE(mara.value(u"target"_s).toString(), u"Mara Vey"_s);

    links.setQuery(u"pancakes"_s);
    QStringList targets;
    for (const QVariant &item : links.suggestions(8))
        targets.append(item.toMap().value(u"target"_s).toString());
    QVERIFY(targets.contains(u"Recipes/Pancakes"_s));
    QVERIFY(targets.contains(u"Breakfast/Pancakes"_s));
    for (const QString &target : std::as_const(targets))
        QCOMPARE(m_vault->index()->resolve(target, {}), target + u".md"_s);
    QCOMPARE(links.suggestions(1).size(), 1);
}

void TestVaultController::makesRenamesAndMovesNotebooks()
{
    QVERIFY(m_vault->openPath(m_root));
    m_settings->setCollapsedNotebooks({u"Ashfall"_s, u"Ashfall/Characters"_s});
    const QString made = m_vault->newNotebook(u"Ashfall"_s);
    QCOMPARE(made, u"Ashfall/New notebook"_s);
    QVERIFY(QFileInfo(m_root + u"/Ashfall/New notebook"_s).isDir());
    // Its parent opens so it shows.
    QCOMPARE(m_settings->collapsedNotebooks(), QStringList{u"Ashfall/Characters"_s});
    QVERIFY(column(*m_vault->notebooks(), "path").contains(made));

    m_vault->setCurrent(u"Ashfall/Characters/Mara Vey.md"_s);
    m_vault->notes()->setNotebook(u"Ashfall/Characters"_s);
    QVERIFY(m_vault->renameNotebook(u"Ashfall/Characters"_s, u"Cast"_s));
    QCOMPARE(m_vault->current(), u"Ashfall/Cast/Mara Vey.md"_s);
    QVERIFY(m_vault->currentText().contains(u"[[Soren]]"_s));
    QCOMPARE(m_vault->notes()->notebook(), u"Ashfall/Cast"_s);
    QCOMPARE(m_settings->collapsedNotebooks(), QStringList{u"Ashfall/Cast"_s});
    QVERIFY(!column(*m_vault->notebooks(), "path").contains(u"Ashfall/Characters"_s));

    QVERIFY(m_vault->moveNotebook(u"Ashfall/Cast"_s, {}));
    QCOMPARE(m_vault->current(), u"Cast/Mara Vey.md"_s);
    QCOMPARE(m_vault->index()->resolve(u"Mara"_s, {}), u"Cast/Mara Vey.md"_s);
    // Into itself, or where it is, does nothing and is no problem.
    QVERIFY(!m_vault->moveNotebook(u"Ashfall"_s, u"Ashfall/New notebook"_s));
    QVERIFY(!m_vault->moveNotebook(u"Cast"_s, {}));
    QVERIFY(m_vault->problem().isEmpty());
}

void TestVaultController::followsANotebookThatMovedThoughALinkCouldNotBeRewritten()
{
    // A folder Jot cannot write in makes the rewrite of a link fail after the move. Root
    // writes anywhere, as in CI's containers, so there the failure cannot be made.
    if (geteuid() == 0)
        QSKIP("root can write in any folder");
    const QString locked = m_root + u"/Locked"_s;
    QVERIFY(QDir().mkpath(locked));
    QFile board(locked + u"/Board.md"_s);
    QVERIFY(board.open(QIODevice::WriteOnly));
    board.write("[Mara](../Ashfall/Characters/Mara%20Vey.md)\n");
    board.close();
    QVERIFY(m_vault->openPath(m_root));
    m_vault->setCurrent(u"Ashfall/Characters/Mara Vey.md"_s);
    QVERIFY(QFile::setPermissions(locked, QFileDevice::ReadOwner | QFileDevice::ExeOwner));

    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(u".*could not move.*"_s));
    const bool moved = m_vault->renameNotebook(u"Ashfall/Characters"_s, u"Cast"_s);
    QFile::setPermissions(locked,
                          QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
    QVERIFY(!moved);
    QVERIFY(!m_vault->problem().isEmpty());
    // The folder did move, so the note shown and the index follow it there.
    QVERIFY(QFileInfo::exists(m_root + u"/Ashfall/Cast/Mara Vey.md"_s));
    QCOMPARE(m_vault->current(), u"Ashfall/Cast/Mara Vey.md"_s);
    QVERIFY(m_vault->index()->note(u"Ashfall/Cast/Mara Vey.md"_s));
    QVERIFY(!m_vault->index()->note(u"Ashfall/Characters/Mara Vey.md"_s));
}

void TestVaultController::givesNotebooksAnEmoji()
{
    QVERIFY(m_vault->openPath(m_root));
    QSignalSpy changed(m_vault.get(), &VaultController::notesChanged);
    QVERIFY(m_vault->setNotebookEmoji(u"Ashfall/Characters"_s, u"🎭"_s));
    QCOMPARE(m_vault->notebookEmoji(), (QVariantMap{{u"Ashfall/Characters"_s, u"🎭"_s}}));
    QVERIFY(!changed.isEmpty());
    QVERIFY(QFile::exists(m_root + u"/.jot/layout.yaml"_s));

    // It goes with the notebook, and away when taken off.
    QVERIFY(m_vault->renameNotebook(u"Ashfall/Characters"_s, u"Cast"_s));
    QCOMPARE(m_vault->notebookEmoji(), (QVariantMap{{u"Ashfall/Cast"_s, u"🎭"_s}}));
    QVERIFY(m_vault->setNotebookEmoji(u"Ashfall/Cast"_s, {}));
    QVERIFY(m_vault->notebookEmoji().isEmpty());

    // Opening the folder again reads it from the vault.
    QVERIFY(m_vault->setNotebookEmoji(u"Ashfall"_s, u"🔥"_s));
    VaultController again(*m_settings);
    again.setIndexPath(m_dir->filePath(u"index2.sqlite"_s));
    QVERIFY(again.openPath(m_root));
    QCOMPARE(again.notebookEmoji(), (QVariantMap{{u"Ashfall"_s, u"🔥"_s}}));

    // A layout file it cannot read is not written over, and the trouble is shown.
    QFile file(m_root + u"/.jot/layout.yaml"_s);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("folders:\n  Ashfall:\n    emoji: \xff\n");
    file.close();
    QVERIFY(!m_vault->setNotebookEmoji(u"Inbox"_s, u"📥"_s));
    QVERIFY(m_vault->problem().contains(u"layout.yaml"_s));
    QVERIFY(file.open(QIODevice::ReadOnly));
    QCOMPARE(file.readAll(), QByteArray("folders:\n  Ashfall:\n    emoji: \xff\n"));
}

void TestVaultController::movesANoteToAnotherNotebook()
{
    QVERIFY(m_vault->openPath(m_root));
    m_vault->setCurrent(u"Ashfall/Places/The Ferry.md"_s);
    QVERIFY(m_vault->moveNoteTo(u"Ashfall/Places/The Ferry.md"_s, u"Inbox"_s));
    QCOMPARE(m_vault->current(), u"Inbox/The Ferry.md"_s);
    QVERIFY(QFileInfo::exists(m_root + u"/Inbox/The Ferry.assets/ferry.png"_s));
    // Links to it still reach it, and its picture still shows.
    QCOMPARE(m_vault->index()->resolve(u"The Ferry"_s, {}), u"Inbox/The Ferry.md"_s);
    QVERIFY(!m_vault->attachmentPath(u"The Ferry.assets/ferry.png"_s, false).isEmpty());
    QVERIFY(!m_vault->moveNoteTo(u"Inbox/The Ferry.md"_s, u"Inbox"_s));
}

void TestVaultController::actsOnNotesThatAreNotShown()
{
    QVERIFY(m_vault->openPath(m_root));
    m_vault->setCurrent(u"Welcome.md"_s);

    QVERIFY(!m_vault->isTask(u"Recipes/Pancakes.md"_s));
    QVERIFY(m_vault->setTask(u"Recipes/Pancakes.md"_s, true));
    QVERIFY(m_vault->isTask(u"Recipes/Pancakes.md"_s));
    QVERIFY(m_vault->setTask(u"Inbox/Groceries.md"_s, false));
    const QString groceries = readFile(m_root + u"/Inbox/Groceries.md"_s);
    QVERIFY(!groceries.contains(u"task:"_s));
    QVERIFY(groceries.contains(u"due:"_s));

    QCOMPARE(m_vault->linkTo(u"Ashfall/Characters/Soren.md"_s), u"[[Soren]]"_s);
    // A name another note shares, in any case, needs the path to say which.
    QFile twin(m_root + u"/Inbox/pancakes.md"_s);
    QVERIFY(twin.open(QIODevice::WriteOnly));
    twin.write("Another stack.\n");
    twin.close();
    QTRY_VERIFY_WITH_TIMEOUT(m_vault->index()->note(u"Inbox/pancakes.md"_s), 3000);
    QCOMPARE(m_vault->linkTo(u"Recipes/Pancakes.md"_s), u"[[Recipes/Pancakes]]"_s);
    QCOMPARE(m_vault->notebookOf(u"Recipes/Pancakes.md"_s), u"Recipes"_s);
    QCOMPARE(m_vault->notebookOf(u"Welcome.md"_s), QString());
    QVERIFY(m_vault->notebookPaths().contains(u"Ashfall/Places"_s));
    QVERIFY(!m_vault->notebookPaths().contains(QString()));

    QVERIFY(m_vault->trashNote(u"Recipes/Sourdough.md"_s));
    QVERIFY(!QFileInfo::exists(m_root + u"/Recipes/Sourdough.md"_s));
    QCOMPARE(m_vault->current(), u"Welcome.md"_s);
}

void TestVaultController::trashesANotebook()
{
    QVERIFY(m_vault->openPath(m_root));
    m_vault->setCurrent(u"Ashfall/Characters/Soren.md"_s);
    m_vault->notes()->setNotebook(u"Ashfall/Characters"_s);
    m_settings->setCollapsedNotebooks({u"Ashfall/Characters"_s, u"Recipes"_s});
    QCOMPARE(m_vault->notesInNotebook(u"Ashfall"_s), 4);

    QVERIFY(m_vault->trashNotebook(u"Ashfall"_s));
    QVERIFY(!QFileInfo::exists(m_root + u"/Ashfall"_s));
    // Kept in the vault's own trash here, since the tests ask for it.
    QVERIFY(QDir(m_root + u"/.jot/trash"_s).entryList(QDir::Dirs | QDir::NoDotAndDotDot).size() ==
            1);
    QCOMPARE(m_vault->current(), QString());
    QCOMPARE(m_vault->notes()->notebook(), QString());
    QCOMPARE(m_settings->collapsedNotebooks(), QStringList{u"Recipes"_s});
    QVERIFY(!m_vault->index()->note(u"Ashfall/Pitch.md"_s));
    QCOMPARE(m_vault->noteCount(), 4);

    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(u".*could not move.*"_s));
    QVERIFY(!m_vault->trashNotebook(u".jot"_s));
}

void TestVaultController::switcherFindsByTitleAliasAndPath()
{
    QVERIFY(m_vault->openPath(m_root));
    SwitcherModel &switcher = *m_vault->switcher();
    QCOMPARE(switcher.rowCount(), 8);

    switcher.setQuery(u"sour"_s);
    QCOMPARE(column(switcher, "title").first(), u"Sourdough"_s);
    switcher.setQuery(u"ferrywoman"_s);
    QCOMPARE(column(switcher, "title").first(), u"Mara Vey"_s);
    QCOMPARE(column(switcher, "alias").first(), u"the Ferrywoman"_s);
    switcher.setQuery(u"places"_s);
    QCOMPARE(column(switcher, "path").first(), u"Ashfall/Places/The Ferry.md"_s);
    switcher.setQuery(u"zzz"_s);
    QCOMPARE(switcher.rowCount(), 0);
}

void TestVaultController::showsTheCurrentNote()
{
    QVERIFY(m_vault->openPath(m_root));
    QSignalSpy changed(m_vault.get(), &VaultController::currentChanged);
    m_vault->setCurrent(u"Ashfall/Characters/Mara Vey.md"_s);
    QCOMPARE(changed.size(), 1);
    QCOMPARE(m_vault->currentTitle(), u"Mara Vey"_s);
    QVERIFY(m_vault->currentText().contains(u"waxed ledger"_s));
}

void TestVaultController::followsChangesOnDisk()
{
    QVERIFY(m_vault->openPath(m_root));
    m_vault->setCurrent(u"Welcome.md"_s);
    QSignalSpy changed(m_vault.get(), &VaultController::notesChanged);

    QFile created(m_root + u"/Inbox/Idea.md"_s);
    QVERIFY(created.open(QIODevice::WriteOnly));
    created.write("#idea a new thought\n");
    created.close();
    QFile welcome(m_root + u"/Welcome.md"_s);
    QVERIFY(welcome.open(QIODevice::WriteOnly | QIODevice::Truncate));
    welcome.write("Rewritten elsewhere.\n");
    welcome.close();

    QTRY_COMPARE_WITH_TIMEOUT(m_vault->noteCount(), 9, 3000);
    QVERIFY(m_vault->tags().contains(u"idea"_s));
    QTRY_VERIFY_WITH_TIMEOUT(m_vault->currentText().contains(u"Rewritten"_s), 3000);
    QVERIFY(!changed.isEmpty());
}

void TestVaultController::savesShortlyAfterTypingStops()
{
    QVERIFY(m_vault->openPath(m_root));
    m_vault->setSaveDelay(50);
    m_vault->setCurrent(u"Welcome.md"_s);
    m_vault->edit(u"First draft\n"_s);
    QVERIFY(m_vault->unsaved());
    QVERIFY(readFile(m_root + u"/Welcome.md"_s) != u"First draft\n"_s);
    QTRY_VERIFY_WITH_TIMEOUT(!m_vault->unsaved(), 2000);
    QCOMPARE(readFile(m_root + u"/Welcome.md"_s), u"First draft\n"_s);

    // Jot's own save is not mistaken for someone else's.
    QSignalSpy current(m_vault.get(), &VaultController::currentChanged);
    QTest::qWait(700);
    QCOMPARE(current.size(), 0);
}

void TestVaultController::savesBeforeSwitchingNotes()
{
    QVERIFY(m_vault->openPath(m_root));
    m_vault->setSaveDelay(60000);
    m_vault->setCurrent(u"Welcome.md"_s);
    m_vault->edit(u"Kept\n"_s);
    m_vault->setCurrent(u"Recipes/Pancakes.md"_s);
    QCOMPARE(readFile(m_root + u"/Welcome.md"_s), u"Kept\n"_s);
    QVERIFY(!m_vault->unsaved());
}

void TestVaultController::keepsPastVersionsWhileEditing()
{
    QVERIFY(m_vault->openPath(m_root));
    m_vault->setSaveDelay(60000);
    const QString welcome = readFile(m_root + u"/Welcome.md"_s);
    m_vault->setCurrent(u"Welcome.md"_s);
    QVERIFY(m_vault->historyOf(u"Welcome.md"_s).isEmpty());

    // The note as it was before editing is kept, then no more until the interval passes.
    m_vault->edit(u"One\n"_s);
    m_vault->saveNow();
    m_vault->edit(u"Two\n"_s);
    m_vault->saveNow();
    QVariantList history = m_vault->historyOf(u"Welcome.md"_s);
    QCOMPARE(history.size(), 1);
    QCOMPARE(m_vault->versionText(history.first().toMap().value(u"path"_s).toString()), welcome);

    m_vault->setHistoryInterval(0);
    m_vault->edit(u"Three\n"_s);
    m_vault->saveNow();
    history = m_vault->historyOf(u"Welcome.md"_s);
    QCOMPARE(history.size(), 2);
    QCOMPARE(m_vault->versionText(history.first().toMap().value(u"path"_s).toString()), u"Two\n"_s);

    // Ticking a to-do in a note not shown keeps its version too.
    const QString soren = readFile(m_root + u"/Ashfall/Characters/Soren.md"_s);
    QVERIFY(m_vault->setTaskDone(u"Ashfall/Characters/Soren.md"_s, 7, true,
                                 u"decide what he wants by act two"_s));
    history = m_vault->historyOf(u"Ashfall/Characters/Soren.md"_s);
    QCOMPARE(history.size(), 1);
    QCOMPARE(m_vault->versionText(history.first().toMap().value(u"path"_s).toString()), soren);

    // A note made where one was moved from keeps its first version, though soon after.
    m_vault->setHistoryInterval(60 * 60 * 1000);
    m_vault->setCurrent(u"Welcome.md"_s);
    QVERIFY(m_vault->renameCurrent(u"Welcome back"_s));
    QVERIFY(m_vault->historyOf(u"Welcome.md"_s).isEmpty());
    QCOMPARE(m_vault->newNote(u"Welcome"_s), u"Welcome.md"_s);
    m_vault->edit(u"A new welcome\n"_s);
    m_vault->saveNow();
    QCOMPARE(m_vault->historyOf(u"Welcome.md"_s).size(), 1);

    // Only versions can be read this way.
    QVERIFY(m_vault->versionText(u"Welcome.md"_s).isEmpty());
    QVERIFY(m_vault->versionText(u".jot/history/../Welcome.md"_s).isEmpty());
}

void TestVaultController::restoresAPastVersion()
{
    QVERIFY(m_vault->openPath(m_root));
    m_vault->setSaveDelay(60000);
    m_vault->setHistoryInterval(0);
    const QString welcome = readFile(m_root + u"/Welcome.md"_s);
    m_vault->setCurrent(u"Welcome.md"_s);
    m_vault->edit(u"Rewritten\n"_s);
    m_vault->saveNow();
    const QString original =
        m_vault->historyOf(u"Welcome.md"_s).first().toMap().value(u"path"_s).toString();

    // Typing not yet saved is part of what the restore replaces and keeps, however soon.
    m_vault->setHistoryInterval(60 * 60 * 1000);
    m_vault->edit(u"Rewritten twice\n"_s);
    QSignalSpy current(m_vault.get(), &VaultController::currentChanged);
    QVERIFY(m_vault->restoreVersion(original));
    QCOMPARE(current.size(), 1);
    QCOMPARE(m_vault->currentText(), welcome);
    QCOMPARE(readFile(m_root + u"/Welcome.md"_s), welcome);
    const QVariantList history = m_vault->historyOf(u"Welcome.md"_s);
    QCOMPARE(m_vault->versionText(history.first().toMap().value(u"path"_s).toString()),
             u"Rewritten twice\n"_s);

    // So the restore itself can be undone.
    QVERIFY(m_vault->restoreVersion(history.first().toMap().value(u"path"_s).toString()));
    QCOMPARE(readFile(m_root + u"/Welcome.md"_s), u"Rewritten twice\n"_s);

    // Another note's versions are not restored over this one.
    m_vault->setCurrent(u"Recipes/Pancakes.md"_s);
    QVERIFY(!m_vault->restoreVersion(original));
}

void TestVaultController::keepsTheOtherVersionWhenBothChanged()
{
    QVERIFY(m_vault->openPath(m_root));
    m_vault->setSaveDelay(60000);
    m_vault->setCurrent(u"Welcome.md"_s);
    m_vault->edit(u"Mine\n"_s);
    QString error;
    QVERIFY(notefile::write(m_root + u"/Welcome.md"_s, u"Theirs\n"_s, &error));

    QTRY_COMPARE_WITH_TIMEOUT(QDir(m_root).entryList({u"Welcome (conflict *).md"_s}).size(), 1,
                              3000);
    const QString copy = QDir(m_root).entryList({u"Welcome (conflict *).md"_s}).first();
    QCOMPARE(readFile(m_root + u'/' + copy), u"Theirs\n"_s);
    m_vault->saveNow();
    QCOMPARE(readFile(m_root + u"/Welcome.md"_s), u"Mine\n"_s);
}

void TestVaultController::mergesOutsideEditsToOtherLines()
{
    QString error;
    QVERIFY(
        notefile::write(m_root + u"/List.md"_s, u"# List\n\n- one\n- two\n- three\n"_s, &error));
    QVERIFY(m_vault->openPath(m_root));
    m_vault->setSaveDelay(60000);
    m_vault->setCurrent(u"List.md"_s);
    QSignalSpy changed(m_vault.get(), &VaultController::currentChanged);
    m_vault->edit(u"# Shopping\n\n- one\n- two\n- three\n"_s);
    QVERIFY(notefile::write(m_root + u"/List.md"_s, u"# List\n\n- one\n- two\n- three\n- four\n"_s,
                            &error));

    // Both edits stay, with nothing kept aside, and the merge is yet to be saved.
    QTRY_COMPARE_WITH_TIMEOUT(m_vault->currentText(),
                              u"# Shopping\n\n- one\n- two\n- three\n- four\n"_s, 3000);
    QVERIFY(!changed.isEmpty());
    QVERIFY(m_vault->unsaved());
    QVERIFY(QDir(m_root).entryList({u"List (conflict *).md"_s}).isEmpty());
    m_vault->saveNow();
    QCOMPARE(readFile(m_root + u"/List.md"_s), u"# Shopping\n\n- one\n- two\n- three\n- four\n"_s);

    // A file that already holds what was typed leaves nothing to save.
    m_vault->edit(u"# Shopping\n\n- one\n- two\n- three\n- four\n- five\n"_s);
    QVERIFY(notefile::write(m_root + u"/List.md"_s,
                            u"# Shopping\n\n- one\n- two\n- three\n- four\n- five\n"_s, &error));
    QTRY_VERIFY_WITH_TIMEOUT(!m_vault->unsaved(), 3000);
}

void TestVaultController::mergesKeepingLineEndingsAndTheByteOrderMark()
{
    const QString start = u"\ufeff# List\r\n\r\n- one\r\n- two\r\n"_s;
    QString error;
    QVERIFY(notefile::write(m_root + u"/Windows.md"_s, start, &error));
    QVERIFY(m_vault->openPath(m_root));
    m_vault->setSaveDelay(60000);
    m_vault->setCurrent(u"Windows.md"_s);
    // The editor's text: plain line breaks, no mark.
    m_vault->edit(u"# Shopping\n\n- one\n- two\n"_s);
    QVERIFY(notefile::write(m_root + u"/Windows.md"_s, start + u"- three\r\n"_s, &error));

    const QString merged = u"\ufeff# Shopping\r\n\r\n- one\r\n- two\r\n- three\r\n"_s;
    QTRY_COMPARE_WITH_TIMEOUT(m_vault->currentText(), merged, 3000);
    QVERIFY(QDir(m_root).entryList({u"Windows (conflict *).md"_s}).isEmpty());
    m_vault->saveNow();
    QCOMPARE(readFile(m_root + u"/Windows.md"_s), merged);
    // The cursor, at the end of "two" in the editor, stays there.
    QCOMPARE(m_vault->movedCursor(u"# Shopping\n\n- one\n- two\n"_s, 23), 23);
}

void TestVaultController::makesNewNotesInTheNotebookShown()
{
    QVERIFY(m_vault->openPath(m_root));
    m_vault->notes()->setNotebook(u"Recipes"_s);
    QCOMPARE(m_vault->newNote(), u"Recipes/Untitled.md"_s);
    QCOMPARE(m_vault->newNote(), u"Recipes/Untitled 2.md"_s);
    QCOMPARE(m_vault->current(), u"Recipes/Untitled 2.md"_s);
    QCOMPARE(m_vault->notes()->rowCount(), 4);
    QVERIFY(m_vault->currentText().startsWith(u"---\ncreated: "_s));
}

void TestVaultController::renamingRewritesLinksAndKeepsTheTitle()
{
    QVERIFY(m_vault->openPath(m_root));
    m_vault->setCurrent(u"Ashfall/Places/The Ferry.md"_s);
    QVERIFY(m_vault->renameCurrent(u"The Rope Ferry"_s));
    QCOMPARE(m_vault->current(), u"Ashfall/Places/The Rope Ferry.md"_s);
    QCOMPARE(m_vault->currentTitle(), u"The Rope Ferry"_s);
    QVERIFY(readFile(m_root + u"/Ashfall/Pitch.md"_s).contains(u"[[The Rope Ferry]]"_s));
    QVERIFY(
        readFile(m_root + u"/Ashfall/Characters/Mara Vey.md"_s).contains(u"[[The Rope Ferry]]"_s));

    // A title the file name cannot hold goes into the front matter, and out again.
    QVERIFY(m_vault->renameCurrent(u"Ferry: the rope one"_s));
    QCOMPARE(m_vault->current(), u"Ashfall/Places/Ferry the rope one.md"_s);
    QCOMPARE(m_vault->currentTitle(), u"Ferry: the rope one"_s);
    QVERIFY(m_vault->currentText().contains(u"title: \"Ferry: the rope one\""_s));
    QVERIFY(m_vault->renameCurrent(u"Ferry"_s));
    QVERIFY(!m_vault->currentText().contains(u"title:"_s));
    QVERIFY(!m_vault->renameCurrent(u"  "_s));
}

void TestVaultController::trashesTheNoteShown()
{
    QVERIFY(m_vault->openPath(m_root));
    m_vault->setCurrent(u"Recipes/Pancakes.md"_s);
    QVERIFY(m_vault->trashCurrent());
    QVERIFY(!QFileInfo::exists(m_root + u"/Recipes/Pancakes.md"_s));
    QCOMPARE(m_vault->current(), QString());
    QCOMPARE(m_vault->noteCount(), 7);
}

void TestVaultController::followsLinksAndMakesMissingNotes()
{
    QVERIFY(m_vault->openPath(m_root));
    m_vault->setCurrent(u"Ashfall/Pitch.md"_s);
    QVERIFY(m_vault->follow(u"Mara"_s, true));
    QCOMPARE(m_vault->current(), u"Ashfall/Characters/Mara Vey.md"_s);
    QVERIFY(m_vault->follow(u"Soren"_s, true));
    QCOMPARE(m_vault->current(), u"Ashfall/Characters/Soren.md"_s);

    QVERIFY(m_vault->follow(u"Harbor Master#Debts"_s, true));
    QCOMPARE(m_vault->current(), u"Ashfall/Characters/Harbor Master.md"_s);
    QCOMPARE(m_vault->noteCount(), 9);
    // A Markdown link to nothing makes nothing.
    QVERIFY(!m_vault->follow(u"Nowhere.md"_s, false));
}

void TestVaultController::keepsLineEndingsAndTheByteOrderMark()
{
    QFile file(m_root + u"/Windows.md"_s);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("\xef\xbb\xbfone\r\ntwo\r\n");
    file.close();
    QVERIFY(m_vault->openPath(m_root));
    m_vault->setCurrent(u"Windows.md"_s);
    // As the editor hands text back: \n lines and no byte order mark.
    m_vault->edit(u"one\ntwo, three\n"_s);
    m_vault->saveNow();
    QVERIFY(file.open(QIODevice::ReadOnly));
    QCOMPARE(file.readAll(), QByteArray("\xef\xbb\xbfone\r\ntwo, three\r\n"));
}

void TestVaultController::aRenameThatFailsHalfwayShowsWhereTheNoteIs()
{
    QVERIFY(m_vault->openPath(m_root));
    // The note moves, but its attachments cannot: a folder of that name is in the way.
    QVERIFY(QDir().mkpath(m_root + u"/Recipes/Pancakes.assets"_s));
    QFile photo(m_root + u"/Recipes/Pancakes.assets/photo.png"_s);
    QVERIFY(photo.open(QIODevice::WriteOnly));
    photo.close();
    QVERIFY(QDir().mkpath(m_root + u"/Recipes/Crepes.assets/in-the-way"_s));
    m_vault->setCurrent(u"Recipes/Pancakes.md"_s);
    m_vault->edit(m_vault->currentText() + u"more butter\n"_s);

    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(u".*could not rename.*"_s));
    QVERIFY(!m_vault->renameCurrent(u"Crepes"_s));
    QVERIFY(!m_vault->problem().isEmpty());
    QCOMPARE(m_vault->current(), u"Recipes/Crepes.md"_s);
    QVERIFY(m_vault->currentText().contains(u"more butter"_s));
    m_vault->edit(m_vault->currentText() + u"and syrup\n"_s);
    m_vault->saveNow();
    // Nothing writes the old path back into being.
    QVERIFY(!QFileInfo::exists(m_root + u"/Recipes/Pancakes.md"_s));
    QVERIFY(readFile(m_root + u"/Recipes/Crepes.md"_s).contains(u"and syrup"_s));
}

void TestVaultController::aFailedSaveIsShownAndRetried()
{
    QVERIFY(m_vault->openPath(m_root));
    m_vault->setCurrent(u"Inbox/Groceries.md"_s);
    const QString folder = m_root + u"/Inbox"_s;
    // The folder taken away, as a drive that drops out: no user, root included, can save.
    QVERIFY(QDir().rename(folder, m_root + u"/.away"_s));
    m_vault->edit(m_vault->currentText() + u"- [ ] eggs\n"_s);
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(u".*could not save.*"_s));
    m_vault->saveNow();
    QVERIFY(m_vault->unsaved());
    QVERIFY(m_vault->problem().startsWith(u"Not saved"_s));

    QVERIFY(QDir().rename(m_root + u"/.away"_s, folder));
    QTRY_VERIFY_WITH_TIMEOUT(!m_vault->unsaved(), 8000);
    QVERIFY(m_vault->problem().isEmpty());
    QVERIFY(readFile(folder + u"/Groceries.md"_s).contains(u"eggs"_s));
}

void TestVaultController::attachesDroppedFilesAndPastedImages()
{
    QVERIFY(m_vault->openPath(m_root));
    m_vault->setCurrent(u"Ashfall/Characters/Mara Vey.md"_s);
    QImage picture(20, 10, QImage::Format_RGB32);
    picture.fill(Qt::red);
    QVERIFY(picture.save(m_dir->filePath(u"map of the river.png"_s)));
    QFile notes(m_dir->filePath(u"notes.txt"_s));
    QVERIFY(notes.open(QIODevice::WriteOnly));
    notes.write("plain\n");
    notes.close();

    const QString dropped =
        m_vault->attachFiles({QUrl::fromLocalFile(m_dir->filePath(u"map of the river.png"_s)),
                              QUrl::fromLocalFile(notes.fileName())});
    QCOMPARE(dropped, u"![](Mara%20Vey.assets/map%20of%20the%20river.png)\n"
                      u"[notes.txt](Mara%20Vey.assets/notes.txt)"_s);
    QVERIFY(QFileInfo::exists(m_root + u"/Ashfall/Characters/Mara Vey.assets/notes.txt"_s));
    QCOMPARE(m_vault->attachmentPath(u"Mara%20Vey.assets/map%20of%20the%20river.png"_s, false),
             m_root + u"/Ashfall/Characters/Mara Vey.assets/map of the river.png"_s);
    QCOMPARE(m_vault->attachmentPath(u"map of the river.png"_s, true),
             m_root + u"/Ashfall/Characters/Mara Vey.assets/map of the river.png"_s);
    QCOMPARE(m_vault->attachmentPath(u"https://example.org/x.png"_s, false), QString());

    // A file that cannot be read, or is not on this computer, is said so, not skipped.
    QCOMPARE(m_vault->attachFiles({QUrl::fromLocalFile(m_dir->filePath(u"missing.png"_s)),
                                   QUrl(u"smb://server/share/x.png"_s)}),
             QString());
    QVERIFY(m_vault->problem().contains(u"missing.png cannot be read"_s));
    QVERIFY(m_vault->problem().contains(u"not a file on this computer"_s));

    // Dropped mid-line, a block gets lines of its own.
    const QVariantMap place = m_vault->placeBlock(u"one two\nthree"_s, 3, u"![](x.png)"_s);
    QCOMPARE(place.value(u"text"_s).toString(), u"\n![](x.png)\n"_s);
    QCOMPARE(place.value(u"cursor"_s).toInt(), 3 + 12);
    QCOMPARE(m_vault->placeBlock(u"one\n"_s, 4, u"![](x.png)"_s).value(u"text"_s).toString(),
             u"![](x.png)\n"_s);
    QCOMPARE(m_vault->placeBlock(u"one\ntwo"_s, 3, u"![](x.png)"_s).value(u"text"_s).toString(),
             u"\n![](x.png)"_s);

    QGuiApplication::clipboard()->setImage(picture);
    const QString pasted = m_vault->pasteImage();
    QVERIFY2(pasted.startsWith(u"![](Mara%20Vey.assets/Pasted%20"_s), qPrintable(pasted));
    QGuiApplication::clipboard()->setText(u"just words"_s);
    QCOMPARE(m_vault->pasteImage(), QString());
}

void TestVaultController::listsBacklinksAndMentions()
{
    QVERIFY(m_vault->openPath(m_root));
    m_vault->setCurrent(u"Ashfall/Characters/Mara Vey.md"_s);
    BacklinksModel &links = *m_vault->backlinks();
    QCOMPARE(links.linked(), 2);
    QCOMPARE(column(links, "path").mid(0, 2),
             (QStringList{u"Ashfall/Pitch.md"_s, u"Ashfall/Places/The Ferry.md"_s}));
    // Read as text, not Markdown.
    QCOMPARE(column(links, "excerpt").first(), u"See Mara and The Ferry."_s);
    // Soren's note names her by her alias, without a link.
    QCOMPARE(links.unlinked(), 1);
    QCOMPARE(column(links, "path").last(), u"Ashfall/Characters/Soren.md"_s);

    // Another note starting to link here shows up once the watcher says so.
    QFile soren(m_root + u"/Ashfall/Characters/Soren.md"_s);
    QVERIFY(soren.open(QIODevice::Append));
    soren.write("\nAsks [[Mara Vey]] for the ledger.\n");
    soren.close();
    QTRY_COMPARE_WITH_TIMEOUT(links.linked(), 3, 3000);
    QCOMPARE(links.unlinked(), 0);

    m_vault->setCurrent(u"Recipes/Sourdough.md"_s);
    QCOMPARE(links.rowCount(), 0);
}

void TestVaultController::previewsWhatALinkLeadsTo()
{
    QVERIFY(m_vault->openPath(m_root));
    m_vault->setCurrent(u"Ashfall/Pitch.md"_s);
    const QVariantMap mara = m_vault->preview(u"Mara"_s);
    QCOMPARE(mara.value(u"path"_s).toString(), u"Ashfall/Characters/Mara Vey.md"_s);
    QCOMPARE(mara.value(u"title"_s).toString(), u"Mara Vey"_s);
    QVERIFY(mara.value(u"text"_s).toString().startsWith(u"Mara runs the last ferry"_s));
    QVERIFY(m_vault->preview(u"Nobody"_s).isEmpty());
    QVERIFY(m_vault->preview(u"The%20Ferry.assets/ferry.png"_s).isEmpty());
}

void TestVaultController::listsAndTicksWhatIsLeftToDo()
{
    QVERIFY(m_vault->openPath(m_root));
    TaskModel &tasks = *m_vault->tasks();
    // The groceries were due before today, which keeps them in Today; the rest has no day.
    QCOMPARE(tasks.view(), u"today"_s);
    QCOMPARE(column(tasks, "title"), QStringList{u"Groceries"_s});
    QCOMPARE(column(tasks, "group"), QStringList{u"Overdue"_s});
    QCOMPARE(tasks.todayCount(), 1);
    tasks.setView(u"all"_s);
    QCOMPARE(m_settings->tasksView(), u"all"_s);
    // The task note first, then the open boxes, note by note.
    QCOMPARE(column(tasks, "kind"), (QStringList{u"note"_s, u"task"_s, u"task"_s, u"task"_s}));
    QCOMPARE(column(tasks, "title"),
             (QStringList{u"Groceries"_s, u"decide what he wants by act two"_s,
                          u"watercolor paper"_s, u"oat milk"_s}));
    QCOMPARE(column(tasks, "note").mid(1),
             (QStringList{u"Soren"_s, u"Groceries"_s, u"Groceries"_s}));

    const QStringList lines = column(tasks, "line");
    // A box in a note that is not shown is ticked in its file.
    QVERIFY(m_vault->setTaskDone(u"Ashfall/Characters/Soren.md"_s, lines.at(1).toInt(), true));
    QVERIFY(readFile(m_root + u"/Ashfall/Characters/Soren.md"_s)
                .contains(u"- [x] decide what he wants"_s));
    QCOMPARE(tasks.rowCount(), 3);

    // One in the note shown goes through the editor's text, unsaved edits and all.
    m_vault->setCurrent(u"Inbox/Groceries.md"_s);
    // Unsaved: a line put in above the tasks, so the index's line numbers are one short.
    m_vault->setSaveDelay(60000);
    QString edited = m_vault->currentText();
    edited.insert(edited.indexOf(u"- [ ] watercolor"_s), u"Shop on Friday.\n"_s);
    m_vault->edit(edited + u"- [ ] eggs\n"_s);
    QVERIFY(m_vault->setTaskDone(u"Inbox/Groceries.md"_s, lines.at(2).toInt(), true,
                                 u"watercolor paper"_s));
    QVERIFY(m_vault->currentText().contains(u"- [x] watercolor paper"_s));
    QVERIFY(m_vault->currentText().contains(u"- [ ] eggs"_s));
    QVERIFY(!m_vault->unsaved());
    QVERIFY(!m_vault->setTaskDone(u"Inbox/Groceries.md"_s, 2, true)); // not a task

    QVERIFY(m_vault->completeTask(u"Inbox/Groceries.md"_s));
    QVERIFY(m_vault->currentText().contains(u"task: done"_s));
    QVERIFY(m_vault->currentText().contains(u"\ndone: 20"_s));
    QCOMPARE(column(tasks, "kind"), (QStringList{u"task"_s, u"task"_s}));

    QSignalSpy reveal(m_vault.get(), &VaultController::revealPosition);
    m_vault->openAt(u"Ashfall/Characters/Soren.md"_s, 7);
    QCOMPARE(m_vault->current(), u"Ashfall/Characters/Soren.md"_s);
    QCOMPARE(m_vault->currentText().mid(reveal.first().first().toInt(), 9), u"- [x] dec"_s);
}

void TestVaultController::headsToDosByDay()
{
    const QDate today = QDate::currentDate();
    const auto day = [&today](int days) { return today.addDays(days).toString(Qt::ISODate); };
    QString error;
    QVERIFY(
        notefile::write(m_root + u"/Plans.md"_s,
                        u"- [ ] Pay the fine @hours 📅 %1\n"
                        "- [ ] Call Soren @minutes 📅 %2 14:00\n"
                        "- [ ] Buy rope @minutes 📅 %3\n"
                        "- [ ] Water @day 🔁 every week 📅 %4\n"
                        "- [ ] Next year 📅 %5\n"_s.arg(day(-2), day(0), day(1), day(3), day(400)),
                        &error));
    QVERIFY(m_vault->openPath(m_root));
    TaskModel &tasks = *m_vault->tasks();
    const QLocale locale;

    tasks.setView(u"today"_s);
    QCOMPARE(column(tasks, "group"), (QStringList{u"Overdue"_s, u"Overdue"_s, u"Today"_s}));
    // Under a day's heading only a time is worth saying; an overdue one says its day.
    QCOMPARE(column(tasks, "due").last(), locale.toString(QTime(14, 0), QLocale::ShortFormat));
    QCOMPARE(column(tasks, "overdue"), (QStringList{u"true"_s, u"true"_s, u"false"_s}));
    QCOMPARE(column(tasks, "note").last(), u"Plans"_s);

    tasks.setView(u"upcoming"_s);
    const QStringList groups = column(tasks, "group");
    QCOMPARE(groups.at(0), u"Tomorrow"_s);
    QCOMPARE(groups.at(1), locale.toString(today.addDays(3), u"dddd, MMMM d"_s));
    // A day in another year says the year.
    QVERIFY(groups.at(2).endsWith(QString::number(today.addDays(400).year())));
    QCOMPARE(column(tasks, "repeats").at(1), u"true"_s);
    QCOMPARE(column(tasks, "due").first(), QString());

    tasks.setView(u"anytime"_s);
    QVERIFY(!column(tasks, "title").contains(u"Buy rope"_s));

    // The sidebar's labels, with how many open to-dos have each.
    QCOMPARE(m_vault->labels(),
             (QVariantList{QVariantMap{{u"label"_s, u"day"_s}, {u"count"_s, 1}},
                           QVariantMap{{u"label"_s, u"hours"_s}, {u"count"_s, 1}},
                           QVariantMap{{u"label"_s, u"minutes"_s}, {u"count"_s, 2}}}));
    tasks.setLabel(u"minutes"_s);
    tasks.setView(u"all"_s);
    QCOMPARE(column(tasks, "title"), (QStringList{u"Call Soren"_s, u"Buy rope"_s}));
}

void TestVaultController::addsToDosToTheInbox()
{
    QVERIFY(m_vault->openPath(m_root));
    const QVariantMap read = m_vault->readQuickTask(u"call Soren tomorrow 3pm @minutes"_s);
    QCOMPARE(read.value(u"title"_s).toString(), u"call Soren @minutes"_s);
    QVERIFY(read.value(u"when"_s).toString().startsWith(u"Tomorrow, "_s));
    QCOMPARE(read.value(u"labels"_s).toStringList(), QStringList{u"minutes"_s});
    QVERIFY(!m_vault->addQuickTask(u"   "_s));

    // The inbox is made on the first, and added to after.
    QVERIFY(m_vault->addQuickTask(u"call Soren tomorrow 3pm @minutes"_s));
    QVERIFY(m_vault->addQuickTask(u"buy rope"_s));
    const QString inbox = readFile(m_root + u"/Inbox.md"_s);
    QVERIFY2(inbox.startsWith(u"- [ ] call Soren @minutes 📅 "_s), qPrintable(inbox));
    QVERIFY(inbox.endsWith(u"15:00\n- [ ] buy rope\n"_s));
    m_vault->tasks()->setView(u"anytime"_s);
    QVERIFY(column(*m_vault->tasks(), "title").contains(u"buy rope"_s));

    // Shown and being edited, it is added through the editor, keeping what was typed.
    m_vault->setCurrent(u"Inbox.md"_s);
    m_vault->setSaveDelay(60000);
    m_vault->edit(m_vault->currentText() + u"Notes for later.\n"_s);
    QVERIFY(m_vault->addQuickTask(u"water plants every week"_s));
    QVERIFY(m_vault->currentText().contains(
        u"Notes for later.\n- [ ] water plants 🔁 every week 📅 "_s));
    QVERIFY(!m_vault->unsaved());

    // A vault can name its own inbox.
    QString error;
    QVERIFY(notefile::write(m_root + u"/.jot/vault.yaml"_s, u"format: 1\ninbox: Ashfall/To do\n"_s,
                            &error));
    QVERIFY(m_vault->addQuickTask(u"outline act two"_s));
    QCOMPARE(readFile(m_root + u"/Ashfall/To do.md"_s), u"- [ ] outline act two\n"_s);

    // An inbox that cannot be read says so, rather than leaving the to-do nowhere.
    QFile broken(m_root + u"/Ashfall/To do.md"_s);
    QVERIFY(broken.open(QIODevice::WriteOnly));
    broken.write("\xff\xfe not text\n");
    broken.close();
    m_vault->setCurrent({});
    QVERIFY(!m_vault->addQuickTask(u"outline act three"_s));
    QVERIFY(m_vault->problem().contains(u"Ashfall/To do.md"_s));
}

void TestVaultController::importsFromJoplinAfterSayingWhatItWouldDo()
{
    QVERIFY(m_vault->openPath(m_root));
    const int before = m_vault->noteCount();
    const QString jex = m_dir->filePath(u"export.jex"_s);
    QFile file(jex);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(tar({{u"n1.md"_s, item(u"From Joplin"_s, u"Hello."_s,
                                      u"id: fedcba9876543210fedcba9876543210\n"
                                      u"user_created_time: 2026-10-01T09:00:00.000Z\n"
                                      u"user_updated_time: 2026-10-02T10:00:00.000Z\n"
                                      u"encryption_applied: 0\ntype_: 1"_s)}}));
    file.close();

    const QVariantMap planned = m_vault->importJoplin(QUrl::fromLocalFile(jex), u"Inbox"_s, true);
    QVERIFY(planned.value(u"ok"_s).toBool());
    QCOMPARE(planned.value(u"created"_s).toInt(), 1);
    QCOMPARE(m_vault->noteCount(), before);

    const QSignalSpy changed(m_vault.get(), &VaultController::notesChanged);
    const QVariantMap done = m_vault->importJoplin(QUrl::fromLocalFile(jex), u"Inbox"_s, false);
    QVERIFY(done.value(u"ok"_s).toBool());
    // The lists show it straight away.
    QCOMPARE(m_vault->noteCount(), before + 1);
    QVERIFY(changed.count() > 0);
    QVERIFY(QFileInfo::exists(m_root + u"/Inbox/From Joplin.md"_s));

    const QVariantMap again = m_vault->importJoplin(QUrl::fromLocalFile(jex), u"Inbox"_s, true);
    QCOMPARE(again.value(u"unchanged"_s).toInt(), 1);
    QCOMPARE(again.value(u"created"_s).toInt(), 0);

    const QVariantMap bad =
        m_vault->importJoplin(QUrl::fromLocalFile(m_dir->filePath(u"missing.jex"_s)), {}, true);
    QVERIFY(!bad.value(u"ok"_s).toBool());
    QVERIFY(!bad.value(u"error"_s).toString().isEmpty());
}

void TestVaultController::anImportShowsWhatItWroteToTheNoteOpen()
{
    QVERIFY(m_vault->openPath(m_root));
    const QString jex = m_dir->filePath(u"export.jex"_s);
    const auto write = [&](const QString &body, const QString &updated) {
        QFile file(jex);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(tar({{u"n1.md"_s, item(u"From Joplin"_s, body,
                                          u"id: fedcba9876543210fedcba9876543210\n"
                                          u"user_created_time: 2026-10-01T09:00:00.000Z\n"
                                          u"user_updated_time: "_s +
                                              updated + u"\nencryption_applied: 0\ntype_: 1"_s)}}));
    };
    write(u"First."_s, u"2026-10-02T10:00:00.000Z"_s);
    QVERIFY(m_vault->importJoplin(QUrl::fromLocalFile(jex), {}, false).value(u"ok"_s).toBool());
    const QString path = u"From Joplin.md"_s;
    m_vault->setCurrent(path);

    // Changed in Joplin only: the editor shows the new text, and typing builds on it.
    write(u"Second."_s, u"2026-10-03T10:00:00.000Z"_s);
    QCOMPARE(m_vault->importJoplin(QUrl::fromLocalFile(jex), {}, false).value(u"updated"_s).toInt(),
             1);
    QVERIFY(m_vault->currentText().contains(u"Second."_s));
    m_vault->edit(m_vault->currentText() + u"Typed in Jot.\n"_s);
    m_vault->saveNow();
    const QString saved = readFile(m_root + u'/' + path);
    QVERIFY(saved.contains(u"Second."_s) && saved.contains(u"Typed in Jot."_s));

    // Typing not yet saved is saved before the import, which then keeps it.
    m_vault->edit(m_vault->currentText() + u"Still typing.\n"_s);
    write(u"Third."_s, u"2026-10-04T10:00:00.000Z"_s);
    const QVariantMap kept = m_vault->importJoplin(QUrl::fromLocalFile(jex), {}, false);
    QCOMPARE(kept.value(u"keptEdits"_s).toStringList(), QStringList{path});
    QVERIFY(readFile(m_root + u'/' + path).contains(u"Still typing."_s));
    QVERIFY(m_vault->currentText().contains(u"Still typing."_s));
}

void TestVaultController::anImportThatFailsPartwayStillShowsWhatItWrote()
{
    QVERIFY(m_vault->openPath(m_root));
    const QString jex = m_dir->filePath(u"export.jex"_s);
    const QString times = u"user_created_time: 2026-10-01T09:00:00.000Z\nencryption_applied: 0\n"_s;
    const QString locked = u"0123456789abcdef0123456789abcdef"_s;
    // A loose note, then one in a notebook the import will find it cannot write in.
    const auto write = [&](const QString &body, const QString &updated) {
        QFile file(jex);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(tar({
            {u"n1.md"_s, item(u"From Joplin"_s, body,
                              u"id: fedcba9876543210fedcba9876543210\n"_s + times +
                                  u"user_updated_time: "_s + updated + u"\ntype_: 1"_s)},
            {u"b1.md"_s,
             item(u"Locked"_s, {}, u"id: "_s + locked + u"\n"_s + times + u"type_: 2"_s)},
            {u"n2.md"_s,
             item(u"Inside"_s, u"y"_s,
                  u"id: 00112233445566778899aabbccddeeff\nparent_id: "_s + locked + u"\n"_s +
                      times + u"user_updated_time: "_s + updated + u"\ntype_: 1"_s)},
        }));
    };
    write(u"First."_s, u"2026-10-02T10:00:00.000Z"_s);
    QVERIFY(m_vault->importJoplin(QUrl::fromLocalFile(jex), {}, false).value(u"ok"_s).toBool());
    m_vault->setCurrent(u"From Joplin.md"_s);

    QFile folder(m_root + u"/Locked"_s);
    QVERIFY(folder.setPermissions(QFile::ReadOwner | QFile::ExeOwner));
    const auto unlock = qScopeGuard(
        [&] { folder.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner); });
    QFile probe(m_root + u"/Locked/probe"_s);
    if (probe.open(QIODevice::WriteOnly))
        QSKIP("permissions are not enforced here, as when tests run as root");

    write(u"Second."_s, u"2026-10-03T10:00:00.000Z"_s);
    QVERIFY(!m_vault->importJoplin(QUrl::fromLocalFile(jex), {}, false).value(u"ok"_s).toBool());
    // The open note was rewritten before the failure, and the editor shows it.
    QVERIFY(readFile(m_root + u"/From Joplin.md"_s).contains(u"Second."_s));
    QVERIFY(m_vault->currentText().contains(u"Second."_s));
}

void TestVaultController::nothingIsImportedOverTypingThatCannotBeSaved()
{
    QVERIFY(m_vault->openPath(m_root));
    const QString jex = m_dir->filePath(u"export.jex"_s);
    QFile file(jex);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(tar({{u"n1.md"_s, item(u"From Joplin"_s, u"x"_s,
                                      u"id: fedcba9876543210fedcba9876543210\n"
                                      u"user_created_time: 2026-10-01T09:00:00.000Z\n"
                                      u"user_updated_time: 2026-10-02T10:00:00.000Z\n"
                                      u"encryption_applied: 0\ntype_: 1"_s)}}));
    file.close();

    // The note being edited is in a folder it cannot be saved in.
    const QString path = u"Inbox/Groceries.md"_s;
    m_vault->setCurrent(path);
    QFile folder(m_root + u"/Inbox"_s);
    QVERIFY(folder.setPermissions(QFile::ReadOwner | QFile::ExeOwner));
    const auto unlock = qScopeGuard(
        [&] { folder.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner); });
    QFile probe(m_root + u"/Inbox/probe"_s);
    if (probe.open(QIODevice::WriteOnly))
        QSKIP("permissions are not enforced here, as when tests run as root");

    m_vault->edit(m_vault->currentText() + u"- [ ] typed in Jot\n"_s);
    const QVariantMap result = m_vault->importJoplin(QUrl::fromLocalFile(jex), {}, false);
    QVERIFY(!result.value(u"ok"_s).toBool());
    QVERIFY(m_vault->currentText().contains(u"typed in Jot"_s));
    QVERIFY(m_vault->unsaved());
    QVERIFY(!QFileInfo::exists(m_root + u"/From Joplin.md"_s));
}

QTEST_MAIN(TestVaultController)
#include "tst_vaultcontroller.moc"
