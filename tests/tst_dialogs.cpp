#include "JoplinDatabaseBuilder.h"
#include "SettingsForeign.h"
#include "ThemeController.h"
#include "VaultController.h"

#include "jot/GitSync.h"
#include "jot/Settings.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QProcess>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickTextDocument>
#include <QQuickWindow>
#include <QRegularExpression>
#include <QSignalSpy>
#include <QStyleHints>
#include <QTemporaryDir>
#include <QTest>
#include <QTextTable>
#include <QtQml/qqmlextensionplugin.h>

Q_IMPORT_QML_PLUGIN(Jot_UiPlugin)

using namespace jot;
using namespace Qt::StringLiterals;

/// Opens each of the title bar's dialogs over a window, so a binding that fails or a
/// singleton that is missing shows up as the warning it would print.
class TestDialogs : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void opensCleanly_data();
    void opensCleanly();
    void everyModalHasAClose_data();
    void everyModalHasAClose();
    void settingsTabsFitWithoutScrolling();
    void anEmptyDateReadsAsSomethingToPress();
    void gitSyncStartsFromTheVaultsOwnRepository();
    void stopSyncingSitsApartFromTheSyncButtons();
    void themeColorsFinishHoweverClosed();
    void showsAndRestoresPastVersions();
    void importsFromTheJoplinOnThisComputer();
    void asksBeforeTrashingSharedAttachments();
    void narrowWindowOverlapsNothing();
    void columnsGiveWayToTheNote();
    void showsTheSyntaxGuide();
    void filtersToDosByPriority();
    void viKeysMoveThroughTheListShownAndTheSidebar();
    void aTagShowsItsNotesAndToDos();
    void renamesANoteInTheWindow();
    void offersToRemoveATargetOnlyWhenThereIsOne();
    void holdingBackListsTheNotesBefore();
    void changesAShortcutByPressingIt();
    void aChosenShortcutWorksInTheWindow();

private:
    QTemporaryDir m_dir;
    std::unique_ptr<Settings> m_settings;
    std::unique_ptr<VaultController> m_vault;
};

void TestDialogs::initTestCase()
{
    QQuickStyle::setStyle(u"Basic"_s);
    // The user's themes and settings are never touched.
    qputenv("XDG_CONFIG_HOME", m_dir.filePath(u"config"_s).toUtf8());
    qputenv("HOME", m_dir.filePath(u"home"_s).toUtf8());
    m_settings = std::make_unique<Settings>(m_dir.filePath(u"settings.ini"_s));
    SettingsForeign::s_instance = m_settings.get();
    m_vault = std::make_unique<VaultController>(*m_settings);
    m_vault->setIndexPath(m_dir.filePath(u"index.sqlite"_s));
    VaultController::setInstance(m_vault.get());
    QVERIFY(m_vault->openPath(m_dir.filePath(u"Notes"_s)));
}

void TestDialogs::opensCleanly_data()
{
    // Each as Main.qml makes it, with what it is given there.
    QTest::addColumn<QString>("dialog");
    QTest::newRow("settings") << u"SettingsDialog {}"_s;
    QTest::newRow("shortcuts") << u"KeyHelp { KeyRouter { id: keys } actions: keys.actions }"_s;
    QTest::newRow("shortcuts, vi mode")
        << u"KeyHelp { KeyRouter { id: keys } actions: keys.actions; tab: 'vi' }"_s;
    QTest::newRow("about") << u"AboutDialog {}"_s;
    QTest::newRow("what's new") << u"WhatsNewDialog {}"_s;
    QTest::newRow("update") << u"UpdateDialog {}"_s;
    QTest::newRow("theme colors") << u"ThemeEditor {}"_s;
    QTest::newRow("report a problem") << u"ReportDialog {}"_s;
    QTest::newRow("past versions") << u"HistoryDialog {}"_s;
    QTest::newRow("word target") << u"NotebookTarget { path: 'Ashfall' }"_s;
    QTest::newRow("syntax guide") << u"SyntaxGuide {}"_s;
    QTest::newRow("export") << u"ExportDialog { path: 'Welcome.md' }"_s;
    QTest::newRow("import from joplin")
        << uR"(JoplinImport { summary: ({ ok: true, created: 3, updated: 1, unchanged: 2, notebooks: 1,
              attachments: 1, keptEdits: ["Stories/Mara.md"], notBroughtBack: ["Old.md"],
              skipped: ["1 note in Joplin's trash"] }) })"_s;
}

void TestDialogs::opensCleanly()
{
    QFETCH(QString, dialog);
    QTest::failOnWarning();

    QQuickWindow window;
    window.setColor(ThemeController::instance()->bg());
    window.resize(900, 700);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    // In a LazyPopup and opened through it, as the app does, since that is what places it.
    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.setData(u"import Jot.Ui\nLazyPopup { sourceComponent: %1 }\n"_s.arg(dialog).toUtf8(),
                      QUrl(u"qrc:/test.qml"_s));
    std::unique_ptr<QObject> lazy(component.create());
    QVERIFY2(lazy, qPrintable(component.errorString()));
    qobject_cast<QQuickItem *>(lazy.get())->setParentItem(window.contentItem());
    QMetaObject::invokeMethod(lazy.get(), "open");
    QTRY_VERIFY(lazy->property("opened").toBool());
    auto *popup = lazy->property("popup").value<QObject *>();
    QVERIFY(popup->property("height").toReal() > 100);
    QVERIFY(popup->property("width").toReal() > 100);
    // Settings' tabs are as wide as their longest label.
    if (popup->property("tabsWidth").isValid()) {
        QVERIFY(popup->property("tabsWidth").toReal() > 40);
        // And each takes the keyboard, ringed while it has it.
        const std::function<QQuickItem *(QQuickItem *)> tab = [&tab](QQuickItem *at) {
            for (QQuickItem *child : at->childItems()) {
                if (child->objectName() == u"settingsTab"_s)
                    return child;
                if (QQuickItem *found = tab(child))
                    return found;
            }
            return static_cast<QQuickItem *>(nullptr);
        };
        QQuickItem *first = tab(window.contentItem());
        QVERIFY(first);
        first->forceActiveFocus(Qt::TabFocusReason);
        QTRY_VERIFY(first->hasActiveFocus());
        auto *ring = first->property("background").value<QQuickItem *>();
        QTRY_COMPARE(ring->property("border").value<QObject *>()->property("width").toReal(), 2.0);
    }
}

void TestDialogs::everyModalHasAClose_data()
{
    QTest::addColumn<QString>("dialog");
    for (const QString &dialog :
         {u"SettingsDialog {}"_s, u"KeyHelp { actions: [] }"_s, u"AboutDialog {}"_s,
          u"WhatsNewDialog {}"_s, u"UpdateDialog {}"_s, u"ThemeEditor {}"_s, u"TrashConfirm {}"_s,
          u"QuickSwitcher {}"_s, u"CommandPalette {}"_s, u"EmojiPicker {}"_s, u"QuickAdd {}"_s,
          u"JoplinImport {}"_s, u"HistoryDialog {}"_s, u"NotebookTarget {}"_s, u"SyntaxGuide {}"_s,
          u"CommitDialog {}"_s})
        QTest::newRow(qPrintable(dialog)) << dialog;
}

void TestDialogs::everyModalHasAClose()
{
    QFETCH(QString, dialog);

    QQuickWindow window;
    window.resize(900, 700);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.setData(u"import Jot.Ui\n%1\n"_s.arg(dialog).toUtf8(), QUrl(u"qrc:/test.qml"_s));
    std::unique_ptr<QObject> popup(component.create());
    QVERIFY2(popup, qPrintable(component.errorString()));
    popup->setProperty("parent", QVariant::fromValue(window.contentItem()));
    QMetaObject::invokeMethod(popup.get(), "open");
    QTRY_VERIFY(popup->property("opened").toBool());

    // The X may sit in the content or in the card's corner.
    QQuickItem *close = nullptr;
    for (const char *part : {"contentItem", "background"}) {
        auto *root = popup->property(part).value<QQuickItem *>();
        if (!close && root)
            close = root->findChild<QQuickItem *>(u"close"_s);
    }
    QVERIFY(close);
    QTRY_VERIFY(close->isVisible() && close->width() > 0);
    const QPointF center = close->mapToScene(QPointF(close->width() / 2, close->height() / 2));
    QTest::mouseClick(&window, Qt::LeftButton, {}, center.toPoint());
    QTRY_VERIFY(!popup->property("visible").toBool());
}

void TestDialogs::settingsTabsFitWithoutScrolling()
{
    QQuickWindow window;
    window.resize(900, 700);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.setData("import Jot.Ui\nSettingsDialog {}\n", QUrl(u"qrc:/test.qml"_s));
    std::unique_ptr<QObject> settings(component.create());
    QVERIFY2(settings, qPrintable(component.errorString()));
    settings->setProperty("parent", QVariant::fromValue(window.contentItem()));
    QMetaObject::invokeMethod(settings.get(), "open");
    QTRY_VERIFY(settings->property("opened").toBool());
    auto *pane = settings->property("contentItem")
                     .value<QQuickItem *>()
                     ->findChild<QQuickItem *>(u"settingsPane"_s);
    QVERIFY(pane);
    for (const QString &tab : {u"general"_s, u"files"_s, u"writing"_s, u"tasks"_s}) {
        settings->setProperty("tab", tab);
        QTRY_VERIFY2(pane->property("contentHeight").toReal() <= pane->height(),
                     qPrintable(u"%1: %2 tall in %3"_s.arg(tab)
                                    .arg(pane->property("contentHeight").toReal())
                                    .arg(pane->height())));
    }
}

void TestDialogs::anEmptyDateReadsAsSomethingToPress()
{
    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.setData("import Jot.Ui\nDatePicker { placeholder: \"Pick a day\" }\n",
                      QUrl(u"qrc:/test.qml"_s));
    std::unique_ptr<QObject> picker(component.create());
    QVERIFY2(picker, qPrintable(component.errorString()));
    auto *caption =
        qobject_cast<QQuickItem *>(picker.get())->findChild<QQuickItem *>(u"dateCaption"_s);
    QVERIFY(caption);
    const ThemeController *theme = ThemeController::instance();
    QCOMPARE(caption->property("color").value<QColor>(), theme->accent());
    QVERIFY(caption->property("color").value<QColor>() != theme->textMuted());
    picker->setProperty("day", QDateTime(QDate(2026, 11, 14), QTime(0, 0)));
    QCOMPARE(caption->property("color").value<QColor>(), theme->text());
    picker->setProperty("enabled", false);
    QCOMPARE(caption->property("color").value<QColor>(), theme->textFaint());
}

void TestDialogs::gitSyncStartsFromTheVaultsOwnRepository()
{
    if (!GitSync::available())
        QSKIP("git is not installed");
    const QString notes = m_dir.filePath(u"Notes"_s);
    const auto git = [&notes](const QStringList &arguments) {
        QProcess process;
        process.setWorkingDirectory(notes);
        process.start(u"git"_s, arguments);
        QVERIFY(process.waitForFinished(10000));
        QCOMPARE(process.exitCode(), 0);
    };
    git({u"init"_s, u"-q"_s, u"-b"_s, u"notes"_s});
    git({u"remote"_s, u"add"_s, u"origin"_s, u"git@example.org:me/notes.git"_s});

    QQuickWindow window;
    window.resize(900, 700);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.setData("import Jot.Ui\nSettingsDialog {}\n", QUrl(u"qrc:/test.qml"_s));
    std::unique_ptr<QObject> settings(component.create());
    QVERIFY2(settings, qPrintable(component.errorString()));
    settings->setProperty("parent", QVariant::fromValue(window.contentItem()));
    settings->setProperty("tab", u"sync"_s);
    QMetaObject::invokeMethod(settings.get(), "open");
    QTRY_VERIFY(settings->property("opened").toBool());
    auto *content = settings->property("contentItem").value<QQuickItem *>();
    auto *start = content->findChild<QQuickItem *>(u"syncWithGit"_s);
    QVERIFY(start);
    QMetaObject::invokeMethod(start, "clicked");
    auto *url = content->findChild<QQuickItem *>(u"gitUrl"_s);
    auto *branch = content->findChild<QQuickItem *>(u"gitBranch"_s);
    QTRY_COMPARE(url->property("text").toString(), u"git@example.org:me/notes.git"_s);
    QCOMPARE(branch->property("text").toString(), u"notes"_s);
    QVERIFY(content->findChild<QQuickItem *>(u"gitOwnRepository"_s)->isVisible());
}

void TestDialogs::stopSyncingSitsApartFromTheSyncButtons()
{
    const QString nas = m_dir.filePath(u"nas"_s);
    QVERIFY(QDir().mkpath(nas));
    SyncController *sync = m_vault->sync();
    QSignalSpy finished(sync, &SyncController::finished);
    QVERIFY(sync->syncWithFolder(QUrl::fromLocalFile(nas)));
    QVERIFY(finished.wait(10000));

    QQuickWindow window;
    window.resize(900, 700);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.setData("import Jot.Ui\nSettingsDialog {}\n", QUrl(u"qrc:/test.qml"_s));
    std::unique_ptr<QObject> settings(component.create());
    QVERIFY2(settings, qPrintable(component.errorString()));
    settings->setProperty("parent", QVariant::fromValue(window.contentItem()));
    settings->setProperty("tab", u"sync"_s);
    QMetaObject::invokeMethod(settings.get(), "open");
    QTRY_VERIFY(settings->property("opened").toBool());
    auto *content = settings->property("contentItem").value<QQuickItem *>();
    auto *stop = content->findChild<QQuickItem *>(u"stopSyncing"_s);
    auto *buttons = content->findChild<QQuickItem *>(u"syncWithBucket"_s)->parentItem();
    QVERIFY(stop->isVisible());
    QVERIFY(stop->parentItem() != buttons);
    const qreal gap = stop->mapToItem(content, QPointF()).y() -
                      buttons->mapToItem(content, QPointF(0, buttons->height())).y();
    QCOMPARE(gap, stop->parentItem()->property("spacing").toReal());

    sync->stop();
    QVERIFY(!sync->configured());
}

void TestDialogs::themeColorsFinishHoweverClosed()
{
    QQuickWindow window;
    window.resize(900, 700);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.setData("import Jot.Ui\nThemeEditor {}\n", QUrl(u"qrc:/test.qml"_s));
    std::unique_ptr<QObject> editor(component.create());
    QVERIFY2(editor, qPrintable(component.errorString()));
    editor->setProperty("parent", QVariant::fromValue(window.contentItem()));
    QSignalSpy finished(editor.get(), SIGNAL(finished()));

    // The X, and Escape, go back to Settings as Done does.
    QMetaObject::invokeMethod(editor.get(), "open");
    QTRY_VERIFY(editor->property("opened").toBool());
    auto *close =
        editor->property("contentItem").value<QQuickItem *>()->findChild<QQuickItem *>(u"close"_s);
    QVERIFY(close);
    QTest::mouseClick(
        &window, Qt::LeftButton, {},
        close->mapToScene(QPointF(close->width() / 2, close->height() / 2)).toPoint());
    QTRY_COMPARE(finished.size(), 1);

    QMetaObject::invokeMethod(editor.get(), "open");
    QTRY_VERIFY(editor->property("opened").toBool());
    QTest::keyClick(&window, Qt::Key_Escape);
    QTRY_COMPARE(finished.size(), 2);
}

void TestDialogs::showsAndRestoresPastVersions()
{
    QTest::failOnWarning();
    const QString path = m_vault->newNote(u"Mara"_s);
    QVERIFY(!path.isEmpty());
    m_vault->setHistoryInterval(0);
    m_vault->edit(u"First\n"_s);
    m_vault->saveNow();
    m_vault->edit(u"Second\n"_s);
    m_vault->saveNow();

    QQuickWindow window;
    window.resize(900, 700);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.setData("import Jot.Ui\nHistoryDialog {}\n", QUrl(u"qrc:/test.qml"_s));
    std::unique_ptr<QObject> dialog(component.create());
    QVERIFY2(dialog, qPrintable(component.errorString()));
    dialog->setProperty("parent", QVariant::fromValue(window.contentItem()));
    QMetaObject::invokeMethod(dialog.get(), "open");
    QTRY_VERIFY(dialog->property("opened").toBool());

    auto *content = dialog->property("contentItem").value<QQuickItem *>();
    auto *versions = content->findChild<QQuickItem *>(u"versions"_s);
    auto *text = content->findChild<QQuickItem *>(u"versionText"_s);
    auto *restore = content->findChild<QQuickItem *>(u"restore"_s);
    QVERIFY(versions && text && restore);
    // The note as made, then "First", newest first.
    QCOMPARE(versions->property("count").toInt(), 2);
    QCOMPARE(text->property("text").toString(), u"First\n"_s);
    // It opens on what changed since, against the note now: "First" went, "Second" came.
    auto *changes = content->findChild<QQuickItem *>(u"versionChanges"_s);
    QVERIFY(changes && changes->isVisible() && !text->isVisible());
    const auto lines = [dialog = dialog.get()] {
        QStringList found;
        for (const QVariant &line :
             dialog->property("comparison").toMap().value(u"lines"_s).toList())
            found << line.toMap().value(u"kind"_s).toString() + u' ' +
                         line.toMap().value(u"text"_s).toString();
        return found;
    };
    QCOMPARE(lines(), (QStringList{u"removed First"_s, u"added Second"_s}));
    // Against the version before it, the note as made, "First" is what came.
    const QString older = m_vault->historyOf(path).last().toMap().value(u"path"_s).toString();
    dialog->setProperty("against", older);
    QVERIFY(lines().contains(u"added First"_s));
    QVERIFY(!lines().contains(u"added Second"_s));
    dialog->setProperty("against", QString());

    QTest::mouseClick(
        &window, Qt::LeftButton, {},
        restore->mapToScene(QPointF(restore->width() / 2, restore->height() / 2)).toPoint());
    QTRY_VERIFY(!dialog->property("visible").toBool());
    QCOMPARE(m_vault->currentText(), u"First\n"_s);
    QCOMPARE(
        m_vault->versionText(m_vault->historyOf(path).first().toMap().value(u"path"_s).toString()),
        u"Second\n"_s);
    // A version gone by the time Restore is pressed says so, and the dialog stays.
    QMetaObject::invokeMethod(dialog.get(), "open");
    QTRY_VERIFY(dialog->property("opened").toBool());
    auto *problem = content->findChild<QQuickItem *>(u"restoreProblem"_s);
    QVERIFY(problem && !problem->isVisible());
    // A version the same as the note now has nothing to restore.
    const QVariantList past = m_vault->historyOf(path);
    const auto same = std::ranges::find_if(past, [this](const QVariant &version) {
        return m_vault->versionText(version.toMap().value(u"path"_s).toString()) == u"First\n"_s;
    });
    QVERIFY(same != past.end());
    versions->setProperty("currentIndex", int(same - past.begin()));
    QTRY_VERIFY(!restore->isEnabled());
    versions->setProperty("currentIndex", 0);
    QTRY_VERIFY(restore->isEnabled());
    const QString chosen = m_vault->historyOf(path).first().toMap().value(u"path"_s).toString();
    QVERIFY(QFile::remove(m_vault->root() + u'/' + chosen));
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(u"could not read"_s));
    QTest::mouseClick(
        &window, Qt::LeftButton, {},
        restore->mapToScene(QPointF(restore->width() / 2, restore->height() / 2)).toPoint());
    QTRY_VERIFY(problem->isVisible());
    QVERIFY(dialog->property("visible").toBool());
    QCOMPARE(m_vault->currentText(), u"First\n"_s);
}

void TestDialogs::importsFromTheJoplinOnThisComputer()
{
    QTest::failOnWarning();
    const QString database = m_dir.filePath(u"config/joplin-desktop/database.sqlite"_s);
    QVERIFY(QDir().mkpath(QFileInfo(database).path()));
    QCOMPARE(makeJoplinDatabase(database, {u"INSERT INTO notes (id, title) VALUES "
                                           "('00112233445566778899aabbccddeef0', 'Desk')"_s}),
             QString());

    QQuickWindow window;
    window.resize(900, 700);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.setData("import Jot.Ui\nJoplinImport {}\n", QUrl(u"qrc:/test.qml"_s));
    std::unique_ptr<QObject> dialog(component.create());
    QVERIFY2(dialog, qPrintable(component.errorString()));
    dialog->setProperty("parent", QVariant::fromValue(window.contentItem()));

    // No file to choose: it says what importing the Joplin here would do.
    QMetaObject::invokeMethod(dialog.get(), "start", Q_ARG(QVariant, QString()));
    QTRY_VERIFY(dialog->property("opened").toBool());
    auto *content = dialog->property("contentItem").value<QQuickItem *>();
    QCOMPARE(content->findChild<QQuickItem *>(u"importSource"_s)->property("text").toString(),
             u"From the Joplin app on this computer"_s);
    QCOMPARE(content->findChild<QQuickItem *>(u"importCounts"_s)->property("text").toString(),
             u"1 new note"_s);
    QVERIFY(content->findChild<QQuickItem *>(u"chooseExport"_s)->isVisible());
}

void TestDialogs::asksBeforeTrashingSharedAttachments()
{
    QTest::failOnWarning();
    QString error;
    const QString root = m_vault->root();
    for (const auto &[path, text] : {std::pair{u"Ines.md"_s, u"![map](Ines.assets/map.png)\n"_s},
                                     {u"Ines.assets/map.png"_s, u"png"_s},
                                     {u"Atlas.md"_s, u"![map](Ines.assets/map.png)\n"_s}}) {
        QVERIFY(QDir().mkpath(QFileInfo(root + u'/' + path).path()));
        QVERIFY(notefile::write(root + u'/' + path, text, &error));
    }
    QVERIFY(m_vault->openPath(root));

    QQuickWindow window;
    window.resize(900, 700);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.setData("import Jot.Ui\nTrashConfirm {}\n", QUrl(u"qrc:/test.qml"_s));
    std::unique_ptr<QObject> dialog(component.create());
    QVERIFY2(dialog, qPrintable(component.errorString()));
    dialog->setProperty("parent", QVariant::fromValue(window.contentItem()));
    QMetaObject::invokeMethod(dialog.get(), "askForNote", Q_ARG(QVariant, u"Ines.md"_s));
    QTRY_VERIFY(dialog->property("opened").toBool());

    auto *content = dialog->property("contentItem").value<QQuickItem *>();
    auto *shared = content->findChild<QQuickItem *>(u"sharedAttachments"_s);
    QVERIFY(shared && shared->isVisible());
    QVERIFY2(shared->property("text").toString().contains(u"Atlas"_s),
             qPrintable(shared->property("text").toString()));
    QVERIFY(content->findChild<QQuickItem *>(u"trashAttachmentsToo"_s)->isVisible());

    // Move to trash takes the note and leaves what Atlas links to. Clicked once the row has
    // placed the extra button beside it, which Qt 6.8 does a frame later.
    auto *confirm = content->findChild<QQuickItem *>(u"confirmTrash"_s);
    auto *too = content->findChild<QQuickItem *>(u"trashAttachmentsToo"_s);
    QTRY_VERIFY(too->x() >= confirm->x() + confirm->width());
    QTest::mouseClick(
        &window, Qt::LeftButton, {},
        confirm->mapToScene(QPointF(confirm->width() / 2, confirm->height() / 2)).toPoint());
    QTRY_VERIFY(!dialog->property("visible").toBool());
    QVERIFY(!QFileInfo::exists(root + u"/Ines.md"_s));
    QVERIFY(QFileInfo::exists(root + u"/Ines.assets/map.png"_s));
}

void TestDialogs::holdingBackListsTheNotesBefore()
{
    for (const QString &name : {u"Trail one"_s, u"Trail two"_s, u"Trail three"_s}) {
        QFile note(m_dir.filePath(u"Notes/"_s + name + u".md"_s));
        QVERIFY(note.open(QIODevice::WriteOnly));
        note.write("Words.\n");
    }
    QTRY_VERIFY_WITH_TIMEOUT(m_vault->index()->note(u"Trail three.md"_s), 3000);
    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.loadFromModule("Jot.Ui", "Main");
    std::unique_ptr<QObject> main(component.create());
    QVERIFY2(main, qPrintable(component.errorString()));
    auto *window = qobject_cast<QQuickWindow *>(main.get());
    QVERIFY(QTest::qWaitForWindowExposed(window));
    for (const QString &name : {u"Trail one"_s, u"Trail two"_s, u"Trail three"_s})
        m_vault->setCurrent(name + u".md"_s);

    auto *back = main->findChild<QQuickItem *>(u"goBack"_s);
    auto *menu = main->findChild<QObject *>(u"trailMenu"_s);
    QVERIFY(back && menu);
    QTRY_VERIFY(back->isEnabled());
    QTest::mouseClick(window, Qt::RightButton, {},
                      back->mapToScene({back->width() / 2, back->height() / 2}).toPoint());
    QTRY_VERIFY(menu->property("opened").toBool());
    const QVariantList entries = menu->property("entries").toList();
    QVERIFY(entries.size() >= 2);
    QCOMPARE(entries.at(0).toMap().value(u"title"_s).toString(), u"Trail two"_s);
    QCOMPARE(entries.at(1).toMap().value(u"title"_s).toString(), u"Trail one"_s);

    // Choosing the second goes straight back two notes.
    QQuickItem *second = nullptr;
    QVERIFY(QMetaObject::invokeMethod(menu, "itemAt", Q_RETURN_ARG(QQuickItem *, second),
                                      Q_ARG(int, 1)));
    QVERIFY(second);
    QTRY_VERIFY(second->isVisible() && second->height() > 0);
    QTest::mouseClick(window, Qt::LeftButton, {},
                      second->mapToScene({second->width() / 2, second->height() / 2}).toPoint());
    QTRY_COMPARE(m_vault->current(), u"Trail one.md"_s);
    QVERIFY(m_vault->canGoForward());
}

void TestDialogs::changesAShortcutByPressingIt()
{
    QQuickWindow window;
    window.resize(900, 760);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.setData("import Jot.Ui\nKeyHelp { KeyRouter { id: keys } actions: keys.actions }\n",
                      QUrl(u"qrc:/test.qml"_s));
    std::unique_ptr<QObject> help(component.create());
    QVERIFY2(help, qPrintable(component.errorString()));
    help->setProperty("parent", QVariant::fromValue(window.contentItem()));
    QMetaObject::invokeMethod(help.get(), "open");
    QTRY_VERIFY(help->property("opened").toBool());
    auto *content = help->property("contentItem").value<QQuickItem *>();
    const std::function<QQuickItem *(QQuickItem *, const QString &)> find =
        [&find](QQuickItem *at, const QString &name) -> QQuickItem * {
        for (QQuickItem *child : at->childItems()) {
            if (child->objectName() == name && child->isVisible())
                return child;
            if (QQuickItem *found = find(child, name))
                return found;
        }
        return nullptr;
    };
    const auto click = [&](const QString &name) {
        QQuickItem *item = nullptr;
        QTRY_VERIFY(item = find(content, name));
        QTest::mouseClick(&window, Qt::LeftButton, {},
                          item->mapToScene({item->width() / 2, item->height() / 2}).toPoint());
    };
    const auto message = [&] {
        return find(content, u"keyMessage"_s)->property("text").toString();
    };

    // Clicked, a shortcut waits for the new keys, which the window's shortcuts do not take.
    click(u"keys_goTo"_s);
    QTRY_COMPARE(help->property("recording").toString(), u"goTo"_s);
    QTest::keyClick(&window, Qt::Key_G, Qt::ControlModifier | Qt::ShiftModifier);
    QTRY_COMPARE(m_settings->keys().value(u"goTo"_s).toString(), u"Ctrl+Shift+G"_s);
    QCOMPARE(help->property("recording").toString(), QString());

    // A key another action has, or one that would only type, is said and not taken.
    click(u"keys_newNote"_s);
    QTRY_COMPARE(help->property("recording").toString(), u"newNote"_s);
    QTest::keyClick(&window, Qt::Key_G, Qt::ControlModifier | Qt::ShiftModifier);
    QTRY_VERIFY2(message().contains(u"already Go to a note"_s), qPrintable(message()));
    QTest::keyClick(&window, Qt::Key_Q);
    QTRY_VERIFY2(message().contains(u"Ctrl or Alt"_s), qPrintable(message()));
    QVERIFY(!m_settings->keys().contains(u"newNote"_s));
    // Escape keeps the old shortcut and the popup open; Backspace leaves none.
    QTest::keyClick(&window, Qt::Key_Escape);
    QTRY_COMPARE(help->property("recording").toString(), QString());
    QVERIFY(help->property("opened").toBool());
    click(u"keys_newNote"_s);
    QTest::keyClick(&window, Qt::Key_Backspace);
    QTRY_VERIFY(m_settings->keys().contains(u"newNote"_s));
    QVERIFY(m_settings->keys().value(u"newNote"_s).toString().isEmpty());

    // Reset puts the built-in one back.
    click(u"reset_goTo"_s);
    QTRY_VERIFY(!m_settings->keys().contains(u"goTo"_s));
    m_settings->resetKeys(u"newNote"_s);
}

void TestDialogs::aChosenShortcutWorksInTheWindow()
{
    m_settings->setKeys(u"goTo"_s, u"Ctrl+Shift+G"_s);
    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.loadFromModule("Jot.Ui", "Main");
    std::unique_ptr<QObject> main(component.create());
    QVERIFY2(main, qPrintable(component.errorString()));
    auto *window = qobject_cast<QQuickWindow *>(main.get());
    QVERIFY(QTest::qWaitForWindowExposed(window));
    QObject *switcher = main->findChild<QObject *>(u"switcher"_s);
    QVERIFY(switcher);

    // The built-in key no longer opens it; the one chosen does.
    QTest::keyClick(window, Qt::Key_P, Qt::ControlModifier);
    QTest::qWait(200);
    QVERIFY(!switcher->property("opened").toBool());
    QTest::keyClick(window, Qt::Key_G, Qt::ControlModifier | Qt::ShiftModifier);
    QTRY_VERIFY(switcher->property("opened").toBool());
    m_settings->resetKeys(u"goTo"_s);
}

QTEST_MAIN(TestDialogs)
#include "tst_dialogs.moc"

void TestDialogs::narrowWindowOverlapsNothing()
{
    const QString folder = m_dir.filePath(u"A notes folder with a rather long name"_s);
    QVERIFY(m_vault->openPath(folder));
    QFile note(folder + u"/Narrow.md"_s);
    QVERIFY(note.open(QIODevice::WriteOnly));
    note.write("# A heading\n\nSome text.\n");
    note.close();
    QTRY_VERIFY_WITH_TIMEOUT(m_vault->index()->note(u"Narrow.md"_s), 3000);
    m_vault->setCurrent(u"Narrow.md"_s);

    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.loadFromModule("Jot.Ui", "Main");
    std::unique_ptr<QObject> main(component.create());
    QVERIFY2(main, qPrintable(component.errorString()));
    auto *window = qobject_cast<QQuickWindow *>(main.get());
    QVERIFY(QTest::qWaitForWindowExposed(window));
    QQuickItem *root = window->contentItem();
    const std::function<QQuickItem *(QQuickItem *, const QString &)> find =
        [&find](QQuickItem *at, const QString &name) -> QQuickItem * {
        for (QQuickItem *child : at->childItems()) {
            if (child->objectName() == name)
                return child;
            if (QQuickItem *found = find(child, name))
                return found;
        }
        return nullptr;
    };
    const auto scene = [](QQuickItem *item) {
        return item->mapRectToScene(QRectF(0, 0, item->width(), item->height()));
    };
    QQuickItem *forward = find(root, u"goForward"_s);
    QQuickItem *newButton = find(root, u"newButton"_s);
    QQuickItem *buttons = find(root, u"formatButtons"_s);
    QQuickItem *trailing = find(root, u"formatTrailing"_s);
    QVERIFY(forward && newButton && buttons && trailing);

    // The window's own minimum is the narrowest it gets.
    for (const int width : {1280, 1000, 760, 600}) {
        for (const bool reading : {false, true}) {
            m_settings->setReadMode(reading);
            window->resize(width, 700);
            QTRY_COMPARE(window->width(), width);
            QTest::qWait(50);
            const QString where = u"%1 wide%2"_s.arg(width).arg(reading ? u", reading"_s : u""_s);
            QVERIFY2(scene(forward).right() <= scene(newButton).left(), qPrintable(where));
            QVERIFY2(scene(trailing).left() >= 0 &&
                         scene(trailing).right() <= scene(trailing->parentItem()).right(),
                     qPrintable(where));
            if (reading)
                continue;
            // The formatting wraps onto a few lines at most, leaving the note its room.
            QVERIFY2(buttons->parentItem()->height() <= 200, qPrintable(where));
            QList<QQuickItem *> shown = buttons->childItems();
            shown.append(find(root, u"formatMore"_s));
            for (QQuickItem *button : std::as_const(shown)) {
                if (!button->isVisible())
                    continue;
                QVERIFY2(scene(button).right() <= scene(buttons->parentItem()).right(),
                         qPrintable(where));
                for (QQuickItem *other : trailing->childItems())
                    QVERIFY2(!scene(button).intersects(scene(other)), qPrintable(where));
            }
        }
    }
    m_settings->setReadMode(false);
    QVERIFY(m_vault->openPath(m_dir.filePath(u"Notes"_s)));
}

void TestDialogs::offersToRemoveATargetOnlyWhenThereIsOne()
{
    QQuickWindow window;
    window.resize(900, 700);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QVERIFY(QDir(m_dir.filePath(u"Notes"_s)).mkpath(u"Novel"_s));
    QVERIFY(m_vault->setNotebookTarget(u"Novel"_s, 0, {}));

    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.setData("import Jot.Ui\nNotebookTarget {}\n", QUrl(u"qrc:/test.qml"_s));
    std::unique_ptr<QObject> dialog(component.create());
    QVERIFY2(dialog, qPrintable(component.errorString()));
    dialog->setProperty("parent", QVariant::fromValue(window.contentItem()));
    const auto reopen = [&] {
        QMetaObject::invokeMethod(dialog.get(), "close");
        QTRY_VERIFY(!dialog->property("visible").toBool());
        QMetaObject::invokeMethod(dialog.get(), "openFor", Q_ARG(QVariant, u"Novel"_s));
        QTRY_VERIFY(dialog->property("opened").toBool());
    };
    reopen();
    QVERIFY(!dialog->property("hasTarget").toBool());
    // Set, then opened on the same notebook again, it can be taken away; then not again.
    QVERIFY(m_vault->setNotebookTarget(u"Novel"_s, 500, {}));
    reopen();
    QVERIFY(dialog->property("hasTarget").toBool());
    QVERIFY(m_vault->setNotebookTarget(u"Novel"_s, 0, {}));
    reopen();
    QVERIFY(!dialog->property("hasTarget").toBool());
}

void TestDialogs::columnsGiveWayToTheNote()
{
    m_settings->setColumnsHidden(false);
    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.loadFromModule("Jot.Ui", "Main");
    std::unique_ptr<QObject> main(component.create());
    QVERIFY2(main, qPrintable(component.errorString()));
    auto *window = qobject_cast<QQuickWindow *>(main.get());
    QVERIFY(QTest::qWaitForWindowExposed(window));
    const std::function<QQuickItem *(QQuickItem *, const QString &)> find =
        [&find](QQuickItem *at, const QString &name) -> QQuickItem * {
        for (QQuickItem *child : at->childItems()) {
            if (child->objectName() == name)
                return child;
            if (QQuickItem *found = find(child, name))
                return found;
        }
        return nullptr;
    };
    QQuickItem *sidebar = find(window->contentItem(), u"sidebar"_s);
    QQuickItem *list = find(window->contentItem(), u"noteList"_s);
    QQuickItem *toggle = find(window->contentItem(), u"columnsToggle"_s);
    QVERIFY(sidebar && list && toggle);
    const auto widen = [&](int width) {
        window->resize(width, 700);
        QTRY_COMPARE(window->contentItem()->width(), qreal(width));
    };
    const auto click = [&] {
        QTest::mouseClick(
            window, Qt::LeftButton, {},
            toggle->mapToScene({toggle->width() / 2, toggle->height() / 2}).toPoint());
    };

    // Dragging the list wider on a window just wide enough keeps both shown: the width it takes is
    // no more than leaves the note its room.
    widen(1100);
    QTRY_VERIFY(sidebar->isVisible() && list->isVisible());
    QQuickItem *splitter = find(window->contentItem(), u"listSplitter"_s);
    QVERIFY(splitter);
    QMetaObject::invokeMethod(splitter, "resized", Q_ARG(int, 420));
    QTRY_COMPARE(m_settings->noteListWidth(), 1100 - 240 - 480);
    QVERIFY(sidebar->isVisible() && list->isVisible());
    m_settings->resetNoteListWidth();
    // Brought back by hand on a narrow window, a drag sets the width it is dragged to.
    widen(800);
    QTRY_VERIFY(!list->isVisible());
    click();
    QTRY_VERIFY(list->isVisible());
    QMetaObject::invokeMethod(splitter, "resized", Q_ARG(int, 260));
    QTRY_COMPARE(m_settings->noteListWidth(), 260);
    click();
    QTRY_VERIFY(!list->isVisible());
    m_settings->setColumnsHidden(false);
    m_settings->resetNoteListWidth();

    // As the window narrows, both go together once the note would have less than its room
    // beside them, as the toggle puts both away; and they come back together.
    widen(1280);
    QTRY_VERIFY(sidebar->isVisible() && list->isVisible());
    widen(1060);
    QTRY_VERIFY(sidebar->isVisible() && list->isVisible());
    widen(1000);
    QTRY_VERIFY(!sidebar->isVisible() && !list->isVisible());
    widen(1060);
    QTRY_VERIFY(sidebar->isVisible() && list->isVisible());
    widen(600);
    QTRY_VERIFY(!sidebar->isVisible() && !list->isVisible());
    // Brought back by hand on a narrow window, both lie over the note rather than squeezing it,
    // and a click on the note's side puts them away again.
    QQuickItem *format = find(window->contentItem(), u"formatBar"_s);
    QVERIFY(format);
    const qreal noteWidth = format->width();
    click();
    QTRY_VERIFY(sidebar->isVisible() && list->isVisible());
    QCOMPARE(format->width(), noteWidth);
    QQuickItem *backdrop = find(window->contentItem(), u"columnsBackdrop"_s);
    QVERIFY(backdrop && backdrop->isVisible());
    QVERIFY(list->mapToScene({list->width(), 0}).x() < window->width());
    QTest::mouseClick(
        window, Qt::LeftButton, {},
        backdrop->mapToScene({backdrop->width() / 2, backdrop->height() / 2}).toPoint());
    QTRY_VERIFY(!sidebar->isVisible() && !list->isVisible());
    // Picking a note puts them away too, showing it.
    QFile pick(m_dir.filePath(u"Notes/Pick me.md"_s));
    QVERIFY(pick.open(QIODevice::WriteOnly));
    pick.write("Text.\n");
    pick.close();
    QTRY_VERIFY_WITH_TIMEOUT(m_vault->index()->note(u"Pick me.md"_s), 3000);
    click();
    const std::function<QQuickItem *(QQuickItem *)> rowOf =
        [&rowOf](QQuickItem *at) -> QQuickItem * {
        for (QQuickItem *child : at->childItems()) {
            if (child->property("path").toString() == u"Pick me.md"_s && child->height() > 0)
                return child;
            if (QQuickItem *found = rowOf(child))
                return found;
        }
        return nullptr;
    };
    QQuickItem *row = nullptr;
    QTRY_VERIFY((row = rowOf(list)) != nullptr && list->isVisible());
    QTest::mouseClick(window, Qt::LeftButton, {},
                      row->mapToScene({row->width() / 2, row->height() / 2}).toPoint());
    QTRY_VERIFY(!list->isVisible());
    QCOMPARE(m_vault->current(), u"Pick me.md"_s);
    // A double-click there renames the note in its row, the flyout staying for it.
    click();
    QTRY_VERIFY((row = rowOf(list)) != nullptr && list->isVisible());
    QTest::mouseDClick(window, Qt::LeftButton, {},
                       row->mapToScene({row->width() / 2, row->height() / 2}).toPoint());
    QTRY_COMPARE(list->property("renamingPath").toString(), u"Pick me.md"_s);
    QTest::qWait(QGuiApplication::styleHints()->mouseDoubleClickInterval() + 100);
    QVERIFY(list->isVisible());
    QTest::keyClick(window, Qt::Key_Escape);
    QTRY_COMPARE(list->property("renamingPath").toString(), QString());
    QVERIFY(sidebar->isVisible());
    // So does picking a to-do in Tasks, opening its note.
    QFile todo(m_dir.filePath(u"Notes/Errands.md"_s));
    QVERIFY(todo.open(QIODevice::WriteOnly));
    todo.write("- [ ] post the letter\n");
    todo.close();
    QTRY_VERIFY_WITH_TIMEOUT(m_vault->index()->note(u"Errands.md"_s), 3000);
    m_vault->tasks()->setView(u"all"_s);
    m_vault->setShowingTasks(true);
    const std::function<QQuickItem *(QQuickItem *)> taskRow =
        [&taskRow](QQuickItem *at) -> QQuickItem * {
        for (QQuickItem *child : at->childItems()) {
            if (child->property("path").toString() == u"Errands.md"_s &&
                child->property("wording").isValid() && child->isVisible())
                return child;
            if (QQuickItem *found = taskRow(child))
                return found;
        }
        return nullptr;
    };
    QQuickItem *task = nullptr;
    QTRY_VERIFY((task = taskRow(window->contentItem())) != nullptr);
    QTest::mouseClick(window, Qt::LeftButton, {},
                      task->mapToScene({task->width() / 2, task->height() / 2}).toPoint());
    QTRY_VERIFY(!sidebar->isVisible());
    QCOMPARE(m_vault->current(), u"Errands.md"_s);
    m_vault->setShowingTasks(false);
    // And picking a note in the trash, to preview it.
    QVERIFY(m_vault->trashNote(u"Errands.md"_s, false));
    m_vault->setShowingTrash(true);
    click();
    QTRY_VERIFY(sidebar->isVisible());
    const std::function<QQuickItem *(QQuickItem *)> trashedRow =
        [&trashedRow](QQuickItem *at) -> QQuickItem * {
        for (QQuickItem *child : at->childItems()) {
            if (child->property("title").toString() == u"Errands"_s &&
                !child->property("location").toString().isEmpty() && child->isVisible())
                return child;
            if (QQuickItem *found = trashedRow(child))
                return found;
        }
        return nullptr;
    };
    QQuickItem *trashed = nullptr;
    QTRY_VERIFY((trashed = trashedRow(window->contentItem())) != nullptr);
    QTest::mouseClick(window, Qt::LeftButton, {},
                      trashed->mapToScene({trashed->width() / 2, trashed->height() / 2}).toPoint());
    QTRY_VERIFY(!sidebar->isVisible());
    QVERIFY(!m_vault->chosenTrashed().isEmpty());
    m_vault->setShowingTrash(false);
    m_vault->setCurrent(u"Pick me.md"_s);
    click();
    QTRY_VERIFY(sidebar->isVisible());
    widen(1280);
    widen(600);
    QTRY_VERIFY(!sidebar->isVisible() && !list->isVisible());

    // Put away by hand, they stay away at any width, and that is remembered.
    widen(1280);
    QTRY_VERIFY(sidebar->isVisible());
    click();
    QTRY_VERIFY(!sidebar->isVisible() && !list->isVisible());
    QVERIFY(m_settings->columnsHidden());
    widen(600);
    widen(1280);
    QTRY_VERIFY(!sidebar->isVisible() && !list->isVisible());
    click();
    QTRY_VERIFY(sidebar->isVisible() && list->isVisible());
    QVERIFY(!m_settings->columnsHidden());

    // Searching brings the list back to search in, however it was put away.
    widen(600);
    QTRY_VERIFY(!list->isVisible());
    QMetaObject::invokeMethod(main.get(), "run", Q_ARG(QVariant, u"search"_s));
    QTRY_VERIFY(list->isVisible());
    widen(1280);
    click();
    QTRY_VERIFY(!list->isVisible());
    QMetaObject::invokeMethod(main.get(), "run", Q_ARG(QVariant, u"search"_s));
    QTRY_VERIFY(list->isVisible());
    QVERIFY(!m_settings->columnsHidden());

    // So does stepping through the notes, which then opens the next one.
    QVERIFY(QDir(m_dir.filePath(u"Notes"_s)).exists());
    for (const QString &name : {u"Step one.md"_s, u"Step two.md"_s}) {
        QFile note(m_dir.filePath(u"Notes/"_s + name));
        QVERIFY(note.open(QIODevice::WriteOnly));
        note.write("Text.\n");
    }
    QTRY_VERIFY_WITH_TIMEOUT(m_vault->index()->note(u"Step two.md"_s), 3000);
    click();
    QTRY_VERIFY(!list->isVisible());
    QMetaObject::invokeMethod(main.get(), "run", Q_ARG(QVariant, u"firstNote"_s));
    QTRY_VERIFY(list->isVisible());
    QTRY_VERIFY(!m_vault->current().isEmpty());
    const QString first = m_vault->current();
    QMetaObject::invokeMethod(main.get(), "run", Q_ARG(QVariant, u"nextNote"_s));
    QTRY_VERIFY(m_vault->current() != first);
}

void TestDialogs::showsTheSyntaxGuide()
{
    QQuickWindow window;
    window.resize(900, 700);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.setData("import Jot.Ui\nSyntaxGuide {}\n", QUrl(u"qrc:/test.qml"_s));
    std::unique_ptr<QObject> guide(component.create());
    QVERIFY2(guide, qPrintable(component.errorString()));
    guide->setProperty("parent", QVariant::fromValue(window.contentItem()));
    QMetaObject::invokeMethod(guide.get(), "open");
    QTRY_VERIFY(guide->property("opened").toBool());
    auto *text = guide->property("contentItem")
                     .value<QQuickItem *>()
                     ->findChild<QQuickItem *>(u"syntaxGuide"_s);
    QVERIFY(text);
    // Drawn from data/syntax.md as a note is read: headings, and examples as written.
    auto *doc = text->property("textDocument").value<QQuickTextDocument *>()->textDocument();
    QTRY_VERIFY(doc->toPlainText().contains(u"To-dos"_s));
    QVERIFY(doc->toPlainText().contains(u"@every 2 weeks on monday"_s));
    QVERIFY(!doc->toPlainText().contains(u"## To-dos"_s));
    // Code is drawn on a surface behind the text, not in frames, whose text Qt drew again and
    // again around a selection.
    for (QTextFrame *frame : doc->rootFrame()->childFrames())
        QVERIFY(qobject_cast<QTextTable *>(frame));
    const std::function<int(QQuickItem *)> backdrops = [&backdrops](QQuickItem *item) {
        int found = item->objectName() == u"codeBackdrop"_s && item->height() > 0;
        for (QQuickItem *child : item->childItems())
            found += backdrops(child);
        return found;
    };
    QTRY_VERIFY(backdrops(text) >= 5);
}

void TestDialogs::filtersToDosByPriority()
{
    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.loadFromModule("Jot.Ui", "Main");
    std::unique_ptr<QObject> main(component.create());
    QVERIFY2(main, qPrintable(component.errorString()));
    auto *window = qobject_cast<QQuickWindow *>(main.get());
    QVERIFY(QTest::qWaitForWindowExposed(window));
    window->resize(1200, 700);
    m_vault->setShowingTasks(true);
    const std::function<QQuickItem *(QQuickItem *, const QString &)> find =
        [&find](QQuickItem *at, const QString &name) -> QQuickItem * {
        for (QQuickItem *child : at->childItems()) {
            if (child->objectName() == name)
                return child;
            if (QQuickItem *found = find(child, name))
                return found;
        }
        return nullptr;
    };
    // The list's priority filter narrows it, and Any priority shows them all again.
    QQuickItem *filter = nullptr;
    QTRY_VERIFY((filter = find(window->contentItem(), u"priorityFilter"_s)) && filter->isVisible());
    QCOMPARE(filter->property("model").toList().size(), 6);
    QMetaObject::invokeMethod(filter, "activated", Q_ARG(QVariant, 2));
    QCOMPARE(m_vault->tasks()->priority(), 2);
    QMetaObject::invokeMethod(filter, "activated", Q_ARG(QVariant, -1));
    QCOMPARE(m_vault->tasks()->priority(), -1);
    m_vault->setShowingTasks(false);
}

void TestDialogs::viKeysMoveThroughTheListShownAndTheSidebar()
{
    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.loadFromModule("Jot.Ui", "Main");
    std::unique_ptr<QObject> main(component.create());
    QVERIFY2(main, qPrintable(component.errorString()));
    auto *window = qobject_cast<QQuickWindow *>(main.get());
    QVERIFY(QTest::qWaitForWindowExposed(window));
    window->resize(1200, 700);
    const std::function<QQuickItem *(QQuickItem *, const QString &)> find =
        [&find](QQuickItem *at, const QString &name) -> QQuickItem * {
        for (QQuickItem *child : at->childItems()) {
            if (child->objectName() == name)
                return child;
            if (QQuickItem *found = find(child, name))
                return found;
        }
        return nullptr;
    };
    const auto run = [&main](const QString &id) {
        QMetaObject::invokeMethod(main.get(), "run", Q_ARG(QVariant, id));
    };
    QQuickItem *sidebar = find(window->contentItem(), u"sidebar"_s);
    QVERIFY(sidebar);
    const auto sidebarHasKeys = [sidebar] {
        QVariant has;
        QMetaObject::invokeMethod(sidebar, "hasKeys", Q_RETURN_ARG(QVariant, has));
        return has.toBool();
    };

    // In Tasks, j and k go through the to-dos, opening each one's note.
    QFile todos(m_dir.filePath(u"Notes/Chores.md"_s));
    QVERIFY(todos.open(QIODevice::WriteOnly));
    todos.write("- [ ] sweep\n- [ ] dust\n- [ ] mop\n");
    todos.close();
    QTRY_VERIFY_WITH_TIMEOUT(m_vault->index()->note(u"Chores.md"_s), 3000);
    m_vault->tasks()->setView(u"all"_s);
    m_vault->setShowingTasks(true);
    QTRY_VERIFY(m_vault->tasks()->rowCount() > 1);
    QQuickItem *rows = nullptr;
    QTRY_VERIFY((rows = find(window->contentItem(), u"taskRows"_s)) && rows->isVisible());
    run(u"firstNote"_s);
    QTRY_COMPARE(rows->property("currentIndex").toInt(), 0);
    QTRY_VERIFY(!m_vault->current().isEmpty());
    run(u"nextNote"_s);
    QTRY_COMPARE(rows->property("currentIndex").toInt(), 1);
    run(u"lastNote"_s);
    QTRY_COMPARE(rows->property("currentIndex").toInt(), rows->property("count").toInt() - 1);

    // Esc from the list goes to the sidebar, where they go through its rows, and Enter opens one.
    QTest::keyClick(window, Qt::Key_Escape);
    QTRY_VERIFY(sidebarHasKeys());
    run(u"lastNote"_s);
    QTRY_COMPARE(window->activeFocusItem()->objectName(), u"trashEntry"_s);
    QTest::keyClick(window, Qt::Key_Return);
    QTRY_VERIFY(m_vault->showingTrash());
    QTRY_VERIFY(!sidebarHasKeys());

    // Up from the trash, back to Tasks, and into the notes.
    QTest::keyClick(window, Qt::Key_Escape);
    QTRY_VERIFY(sidebarHasKeys());
    run(u"firstNote"_s);
    QTest::keyClick(window, Qt::Key_Return);
    QTRY_VERIFY(m_vault->showingTasks());
    m_vault->setShowingTasks(false);
}

void TestDialogs::aTagShowsItsNotesAndToDos()
{
    QFile note(m_dir.filePath(u"Notes/Harbor.md"_s));
    QVERIFY(note.open(QIODevice::WriteOnly));
    note.write("The dawn run. #harbor\n\n- [ ] mend the rope #harbor\n");
    note.close();
    QTRY_VERIFY_WITH_TIMEOUT(m_vault->index()->note(u"Harbor.md"_s), 3000);
    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.loadFromModule("Jot.Ui", "Main");
    std::unique_ptr<QObject> main(component.create());
    QVERIFY2(main, qPrintable(component.errorString()));
    auto *window = qobject_cast<QQuickWindow *>(main.get());
    QVERIFY(QTest::qWaitForWindowExposed(window));
    window->resize(1200, 700);
    const std::function<QQuickItem *(QQuickItem *, const QString &, const QString &)> find =
        [&find](QQuickItem *at, const QString &name, const QString &label) -> QQuickItem * {
        for (QQuickItem *child : at->childItems()) {
            if ((!name.isEmpty() && child->objectName() == name) ||
                (!label.isEmpty() && child->property("isSidebarRow").toBool() &&
                 child->property("label").toString() == label))
                return child;
            if (QQuickItem *found = find(child, name, label))
                return found;
        }
        return nullptr;
    };
    QQuickItem *row = nullptr;
    QTRY_VERIFY((row = find(window->contentItem(), {}, u"#harbor"_s)) && row->isVisible());
    QMetaObject::invokeMethod(row, "clicked");
    QCOMPARE(m_vault->shownTag(), u"harbor"_s);
    QQuickItem *view = find(window->contentItem(), u"tagView"_s, {});
    QTRY_VERIFY(view && view->isVisible());
    QVERIFY(!find(window->contentItem(), u"noteList"_s, {})->isVisible());
    QTRY_VERIFY(find(view, u"tagNote"_s, {}));
    QTRY_VERIFY(find(view, u"tagTask"_s, {}));

    // The list keys go through its rows, opening each, and Esc goes out to the sidebar.
    m_vault->setCurrent({});
    QMetaObject::invokeMethod(main.get(), "run", Q_ARG(QVariant, u"firstNote"_s));
    QTRY_COMPARE(m_vault->current(), u"Harbor.md"_s);
    QTRY_COMPARE(window->activeFocusItem()->objectName(), u"tagNote"_s);
    QMetaObject::invokeMethod(main.get(), "run", Q_ARG(QVariant, u"nextNote"_s));
    QTRY_COMPARE(window->activeFocusItem()->objectName(), u"tagTask"_s);
    QTest::keyClick(window, Qt::Key_Escape);
    QTRY_VERIFY(window->activeFocusItem()->property("isSidebarRow").toBool());

    // Renaming there renames the note shown, in its title, the list being hidden.
    m_vault->setShownTag(u"harbor"_s);
    m_vault->setCurrent(u"Harbor.md"_s);
    QMetaObject::invokeMethod(main.get(), "run", Q_ARG(QVariant, u"rename"_s));
    QTRY_COMPARE(window->activeFocusItem()->objectName(), u"title"_s);
    // Searching is in the notes, so it leaves the tag for them.
    QMetaObject::invokeMethod(main.get(), "run", Q_ARG(QVariant, u"search"_s));
    QTRY_VERIFY(m_vault->shownTag().isEmpty());
    QTRY_VERIFY(find(window->contentItem(), u"noteList"_s, {})->isVisible());
    m_vault->setShownTag(u"harbor"_s);
    QTRY_VERIFY(view->isVisible());

    // Its chip goes back to the notes.
    QMetaObject::invokeMethod(find(view, u"tagChip"_s, {}), "clicked");
    QTRY_VERIFY(!view->isVisible());
    QVERIFY(m_vault->shownTag().isEmpty());
    QVERIFY(find(window->contentItem(), u"noteList"_s, {})->isVisible());
}

void TestDialogs::renamesANoteInTheWindow()
{
    QFile note(m_dir.filePath(u"Notes/Ferry log.md"_s));
    QVERIFY(note.open(QIODevice::WriteOnly));
    note.write("Some words.\n");
    note.close();
    QTRY_VERIFY_WITH_TIMEOUT(m_vault->index()->note(u"Ferry log.md"_s), 3000);
    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.loadFromModule("Jot.Ui", "Main");
    std::unique_ptr<QObject> main(component.create());
    QVERIFY2(main, qPrintable(component.errorString()));
    auto *window = qobject_cast<QQuickWindow *>(main.get());
    QVERIFY(QTest::qWaitForWindowExposed(window));
    window->resize(1280, 700);
    const std::function<QQuickItem *(QQuickItem *, const std::function<bool(QQuickItem *)> &)>
        find = [&find](QQuickItem *at,
                       const std::function<bool(QQuickItem *)> &match) -> QQuickItem * {
        for (QQuickItem *child : at->childItems()) {
            if (match(child))
                return child;
            if (QQuickItem *found = find(child, match))
                return found;
        }
        return nullptr;
    };
    QQuickItem *row = nullptr;
    QTRY_VERIFY(row = find(window->contentItem(), [](QQuickItem *item) {
                    return item->property("path").toString() == u"Ferry log.md"_s &&
                           item->isVisible() && item->height() > 0;
                }));
    // The note opens on the first click, and the editor does not take the typing from the row.
    QTest::mouseDClick(window, Qt::LeftButton, {},
                       row->mapToScene({row->width() / 2, row->height() / 2}).toPoint());
    QQuickItem *field = nullptr;
    QTRY_VERIFY(field = find(window->contentItem(),
                             [](QQuickItem *item) { return item->objectName() == u"noteName"_s; }));
    QTest::qWait(300);
    QVERIFY(field->hasActiveFocus());
    for (const char key : QByteArrayView("Crossing log"))
        QTest::keyClick(window, key);
    QTest::keyClick(window, Qt::Key_Return);
    QTRY_VERIFY(QFileInfo::exists(m_dir.filePath(u"Notes/Crossing log.md"_s)));
}
