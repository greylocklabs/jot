#include "JoplinDatabaseBuilder.h"
#include "SettingsForeign.h"
#include "ThemeController.h"
#include "VaultController.h"

#include "jot/Settings.h"

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
    void themeColorsFinishHoweverClosed();
    void showsAndRestoresPastVersions();
    void importsFromTheJoplinOnThisComputer();

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
}

void TestDialogs::everyModalHasAClose_data()
{
    QTest::addColumn<QString>("dialog");
    for (const QString &dialog :
         {u"SettingsDialog {}"_s, u"KeyHelp { actions: [] }"_s, u"AboutDialog {}"_s,
          u"WhatsNewDialog {}"_s, u"UpdateDialog {}"_s, u"ThemeEditor {}"_s, u"TrashConfirm {}"_s,
          u"QuickSwitcher {}"_s, u"CommandPalette {}"_s, u"EmojiPicker {}"_s, u"QuickAdd {}"_s,
          u"JoplinImport {}"_s, u"HistoryDialog {}"_s})
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
    const QPointF centre = close->mapToScene(QPointF(close->width() / 2, close->height() / 2));
    QTest::mouseClick(&window, Qt::LeftButton, {}, centre.toPoint());
    QTRY_VERIFY(!popup->property("visible").toBool());
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

QTEST_MAIN(TestDialogs)
#include "tst_dialogs.moc"
