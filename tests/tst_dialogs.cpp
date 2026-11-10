#include "JoplinDatabaseBuilder.h"
#include "SettingsForeign.h"
#include "ThemeController.h"
#include "VaultController.h"

#include "jot/Settings.h"

#include <QDir>
#include <QFile>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QRegularExpression>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
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
    void themeColorsFinishHoweverClosed();
    void showsAndRestoresPastVersions();
    void importsFromTheJoplinOnThisComputer();
    void asksBeforeTrashingSharedAttachments();
    void narrowWindowOverlapsNothing();
    void offersToRemoveATargetOnlyWhenThereIsOne();

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
          u"JoplinImport {}"_s, u"HistoryDialog {}"_s, u"NotebookTarget {}"_s})
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
    for (const QString &tab : {u"general"_s, u"writing"_s, u"reminders"_s}) {
        settings->setProperty("tab", tab);
        QTRY_VERIFY2(pane->property("contentHeight").toReal() <= pane->height(),
                     qPrintable(u"%1: %2 tall in %3"_s.arg(tab)
                                    .arg(pane->property("contentHeight").toReal())
                                    .arg(pane->height())));
    }
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
