#include "SettingsForeign.h"
#include "ThemeController.h"
#include "VaultController.h"

#include "jot/Settings.h"
#include "jot/Vault.h"

#include <QDirIterator>
#include <QMimeData>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QTest>
#include <QtQml/qqmlextensionplugin.h>

#include <functional>

Q_IMPORT_QML_PLUGIN(Jot_UiPlugin)

using namespace jot;
using namespace Qt::StringLiterals;

/// Drives the sidebar and the note list in a window: making and renaming notebooks by hand,
/// and the menus on notes and notebooks.
class TestLists : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase() { QQuickStyle::setStyle(u"Basic"_s); }
    void init();
    void cleanup();

    void leavesRoomUnderTheTitleBar();
    void renamesANotebookByDoubleClick();
    void escapeKeepsTheOldName();
    void makesANotebookReadyToName();
    void dropsNotesAndNotebooksOnNotebooks();
    void noteMenuMakesATask();
    void notebookMenuMakesANotebookInside();
    void trashAsksFirst();
    void columnBordersDragToResize();
    void deleteAsksFirstForNotesAndNotebooks();
    void searchHandsTheKeysBackToTheList();
    void taskViewsLineUpWithTheAddButton();
    void taskViewsFitANarrowColumn();
    void untickingInDoneOpensItAgain();

private:
    /// The visible menu entry reading `text`, anywhere in the window.
    QQuickItem *menuEntry(const QString &text) const;
    void click(QQuickItem *item, Qt::MouseButton button = Qt::LeftButton);
    /// The sidebar entry labeled `label`, among those shown.
    QQuickItem *entry(const QString &label) const;
    QQuickItem *findItem(const std::function<bool(QQuickItem *)> &match,
                         QQuickItem *from = nullptr) const;
    void type(const QString &text);
    /// Drags `format` holding `data` onto `target`, as the desktop delivers a drag and drop.
    void drop(QQuickItem *target, const QString &format, const QString &data);

    std::unique_ptr<QTemporaryDir> m_dir;
    std::unique_ptr<Settings> m_settings;
    std::unique_ptr<VaultController> m_vault;
    std::unique_ptr<QQmlEngine> m_engine;
    std::unique_ptr<QQuickWindow> m_window;
    std::unique_ptr<QObject> m_sidebar;
    std::unique_ptr<QObject> m_noteList;
    QString m_root;
};

void TestLists::init()
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
    SettingsForeign::s_instance = m_settings.get();
    m_vault = std::make_unique<VaultController>(*m_settings);
    m_vault->setIndexPath(m_dir->filePath(u"index.sqlite"_s));
    m_vault->setSystemTrash(false);
    VaultController::setInstance(m_vault.get());
    QVERIFY(m_vault->openPath(m_root));

    m_engine = std::make_unique<QQmlEngine>();
    QQmlComponent component(m_engine.get());
    component.loadFromModule("Jot.Ui", "Sidebar");
    m_sidebar.reset(component.create());
    QVERIFY2(m_sidebar, qPrintable(component.errorString()));
    QQmlComponent list(m_engine.get());
    list.loadFromModule("Jot.Ui", "NoteList");
    m_noteList.reset(list.create());
    QVERIFY2(m_noteList, qPrintable(list.errorString()));
    m_window = std::make_unique<QQuickWindow>();
    m_window->resize(640, 700);
    auto *item = qobject_cast<QQuickItem *>(m_sidebar.get());
    item->setParentItem(m_window->contentItem());
    item->setSize({320, 700});
    auto *notes = qobject_cast<QQuickItem *>(m_noteList.get());
    notes->setParentItem(m_window->contentItem());
    notes->setPosition({320, 0});
    notes->setSize({320, 700});
    m_window->show();
    QVERIFY(QTest::qWaitForWindowExposed(m_window.get()));
}

void TestLists::taskViewsLineUpWithTheAddButton()
{
    QQmlComponent component(m_engine.get());
    component.loadFromModule("Jot.Ui", "TaskList");
    std::unique_ptr<QObject> tasks(component.create());
    QVERIFY2(tasks, qPrintable(component.errorString()));
    auto *item = qobject_cast<QQuickItem *>(tasks.get());
    item->setParentItem(m_window->contentItem());
    item->setSize({320, 700});
    auto *views = item->findChild<QQuickItem *>(u"taskViews"_s);
    auto *add = item->findChild<QQuickItem *>(u"addTask"_s);
    QVERIFY(views && add);
    // The switch is shorter than the button beside it, so both sit on the row's middle.
    const auto middle = [](QQuickItem *of) { return of->mapToScene({0, of->height() / 2}).y(); };
    QTRY_COMPARE(middle(views), middle(add));
}

void TestLists::taskViewsFitANarrowColumn()
{
    QQmlComponent component(m_engine.get());
    component.loadFromModule("Jot.Ui", "TaskList");
    std::unique_ptr<QObject> tasks(component.create());
    QVERIFY2(tasks, qPrintable(component.errorString()));
    auto *item = qobject_cast<QQuickItem *>(tasks.get());
    item->setParentItem(m_window->contentItem());
    auto *full = item->findChild<QQuickItem *>(u"taskViews"_s);
    auto *brief = item->findChild<QQuickItem *>(u"taskViewsShort"_s);
    auto *menu = item->findChild<QQuickItem *>(u"taskViewsMenu"_s);
    auto *add = item->findChild<QQuickItem *>(u"addTask"_s);
    QVERIFY(full && brief && menu && add);
    // Which of the three shows, with the + still in the column beside it.
    const auto shown = [&] {
        QList<QQuickItem *> visible;
        for (QQuickItem *views : {full, brief, menu}) {
            if (views->isVisible())
                visible.append(views);
        }
        if (visible.size() != 1 || !add->isVisible())
            return static_cast<QQuickItem *>(nullptr);
        const qreal right = add->mapToItem(item, {add->width(), 0}).x();
        const qreal views = visible.first()->mapToItem(item, {visible.first()->width(), 0}).x();
        return right <= item->width() && views < add->mapToItem(item, {0, 0}).x() ? visible.first()
                                                                                  : nullptr;
    };
    const qreal margins = 2 * 16 + 8 + add->width();

    item->setSize({full->implicitWidth() + margins, 700});
    QTRY_COMPARE(shown(), full);
    item->setSize({brief->implicitWidth() + margins, 700});
    QTRY_COMPARE(shown(), brief);
    item->setSize({brief->implicitWidth() + margins - 1, 700});
    QTRY_COMPARE(shown(), menu);
    item->setSize({140, 700});
    QTRY_COMPARE(shown(), menu);

    // The menu shows and sets the same view as the switches.
    m_vault->tasks()->setProperty("view", u"anytime"_s);
    QCOMPARE(menu->property("currentValue").toString(), u"anytime"_s);
    QCOMPARE(brief->property("current").toInt(), 2);
}

void TestLists::untickingInDoneOpensItAgain()
{
    QVERIFY(m_vault->setTaskDone(u"Ashfall/Characters/Soren.md"_s, 7, true));
    m_vault->tasks()->setView(u"done"_s);
    QQmlComponent component(m_engine.get());
    component.loadFromModule("Jot.Ui", "TaskList");
    std::unique_ptr<QObject> tasks(component.create());
    QVERIFY2(tasks, qPrintable(component.errorString()));
    auto *item = qobject_cast<QQuickItem *>(tasks.get());
    item->setParentItem(m_window->contentItem());
    item->setSize({480, 700});
    QQuickItem *box = nullptr;
    QTRY_VERIFY((box = findItem(
                     [](QQuickItem *each) {
                         const QQuickItem *row =
                             each->parentItem() ? each->parentItem()->parentItem() : nullptr;
                         return each->isVisible() && each->objectName() == u"taskBox"_s && row &&
                                row->property("path").toString() ==
                                    u"Ashfall/Characters/Soren.md"_s;
                     },
                     item)));
    const int count = m_vault->tasks()->rowCount();
    QVERIFY(QMetaObject::invokeMethod(box, "clicked"));
    QTRY_COMPARE(m_vault->tasks()->rowCount(), count - 1);
    QString error;
    QVERIFY(notefile::read(m_root + u"/Ashfall/Characters/Soren.md"_s, &error)
                ->contains(u"- [ ] decide what he wants"_s));
}

void TestLists::cleanup()
{
    m_sidebar.reset();
    m_noteList.reset();
    m_window.reset();
    m_engine.reset();
    VaultController::setInstance(nullptr);
    m_vault.reset();
    SettingsForeign::s_instance = nullptr;
}

QQuickItem *TestLists::findItem(const std::function<bool(QQuickItem *)> &match,
                                QQuickItem *from) const
{
    if (!from)
        from = qobject_cast<QQuickItem *>(m_sidebar.get());
    for (QQuickItem *child : from->childItems()) {
        if (match(child))
            return child;
        if (QQuickItem *found = findItem(match, child))
            return found;
    }
    return nullptr;
}

QQuickItem *TestLists::entry(const QString &label) const
{
    return findItem([&label](QQuickItem *item) {
        return item->isVisible() && item->property("label").toString() == label;
    });
}

QQuickItem *TestLists::menuEntry(const QString &text) const
{
    return findItem(
        [&text](QQuickItem *item) {
            return item->isVisible() && item->inherits("QQuickMenuItem") &&
                   item->property("text").toString() == text;
        },
        m_window->contentItem());
}

void TestLists::click(QQuickItem *item, Qt::MouseButton button)
{
    QTest::mouseClick(m_window.get(), button, {},
                      item->mapToScene({item->width() / 2, item->height() / 2}).toPoint());
}

void TestLists::type(const QString &text)
{
    for (const QChar c : text)
        QTest::keyClick(m_window.get(), c.toLatin1());
}

void TestLists::leavesRoomUnderTheTitleBar()
{
    QQuickItem *tasks = entry(u"Tasks"_s);
    QVERIFY(tasks);
    QVERIFY(tasks->mapToScene({0, 0}).y() >= ThemeController::instance()->space3());
}

void TestLists::renamesANotebookByDoubleClick()
{
    QQuickItem *places = entry(u"Places"_s);
    QVERIFY(places);
    const QPoint center = places->mapToScene({places->width() / 2, places->height() / 2}).toPoint();
    QTest::mouseDClick(m_window.get(), Qt::LeftButton, {}, center);
    // The name is selected, so typing replaces it.
    QTRY_VERIFY(m_sidebar->property("renamingPath").toString() == u"Ashfall/Places"_s);
    type(u"Harbor"_s);
    QTest::keyClick(m_window.get(), Qt::Key_Return);

    QTRY_VERIFY(QFileInfo(m_root + u"/Ashfall/Harbor"_s).isDir());
    QVERIFY(!QFileInfo::exists(m_root + u"/Ashfall/Places"_s));
    QCOMPARE(m_sidebar->property("renamingPath").toString(), QString());
    QTRY_VERIFY(entry(u"Harbor"_s));
}

void TestLists::escapeKeepsTheOldName()
{
    QQuickItem *places = entry(u"Places"_s);
    const QPoint center = places->mapToScene({places->width() / 2, places->height() / 2}).toPoint();
    QTest::mouseDClick(m_window.get(), Qt::LeftButton, {}, center);
    QTRY_VERIFY(!m_sidebar->property("renamingPath").toString().isEmpty());
    type(u"Nope"_s);
    QTest::keyClick(m_window.get(), Qt::Key_Escape);
    QCOMPARE(m_sidebar->property("renamingPath").toString(), QString());
    QVERIFY(QFileInfo(m_root + u"/Ashfall/Places"_s).isDir());
}

void TestLists::makesANotebookReadyToName()
{
    QQuickItem *add =
        findItem([](QQuickItem *item) { return item->objectName() == u"addNotebook"_s; });
    QVERIFY(add);
    QTest::mouseClick(m_window.get(), Qt::LeftButton, {},
                      add->mapToScene({add->width() / 2, add->height() / 2}).toPoint());
    QTRY_COMPARE(m_sidebar->property("renamingPath").toString(), u"New notebook"_s);
    type(u"Ideas"_s);
    QTest::keyClick(m_window.get(), Qt::Key_Return);
    QTRY_VERIFY(QFileInfo(m_root + u"/Ideas"_s).isDir());
    QVERIFY(!QFileInfo::exists(m_root + u"/New notebook"_s));
}

void TestLists::drop(QQuickItem *target, const QString &format, const QString &data)
{
    // Rows made again after a move are placed a moment later; every row sits below Tasks.
    QTRY_VERIFY(target->mapToScene({0, 0}).y() > 0);
    QMimeData mime;
    mime.setData(format, data.toUtf8());
    const QPointF at = target->mapToScene({target->width() / 2, target->height() / 2});
    QDragEnterEvent enter(at.toPoint(), Qt::MoveAction, &mime, Qt::LeftButton, {});
    QCoreApplication::sendEvent(m_window.get(), &enter);
    QDragMoveEvent move(at.toPoint(), Qt::MoveAction, &mime, Qt::LeftButton, {});
    QCoreApplication::sendEvent(m_window.get(), &move);
    QDropEvent dropped(at, Qt::MoveAction, &mime, Qt::LeftButton, {});
    QCoreApplication::sendEvent(m_window.get(), &dropped);
}

void TestLists::dropsNotesAndNotebooksOnNotebooks()
{
    QTest::failOnWarning(QRegularExpression(u"\\.qml:"_s));
    drop(entry(u"Inbox"_s), u"application/x-jot-note"_s, u"Recipes/Pancakes.md"_s);
    QTRY_VERIFY(QFileInfo::exists(m_root + u"/Inbox/Pancakes.md"_s));

    QTRY_VERIFY(entry(u"Ashfall"_s));
    drop(entry(u"Ashfall"_s), u"application/x-jot-notebook"_s, u"Recipes"_s);
    QTRY_VERIFY(QFileInfo(m_root + u"/Ashfall/Recipes"_s).isDir());

    // All notes is the top of the vault.
    drop(entry(u"All notes"_s), u"application/x-jot-notebook"_s, u"Ashfall/Recipes"_s);
    QTRY_VERIFY(QFileInfo(m_root + u"/Recipes"_s).isDir());
}

void TestLists::noteMenuMakesATask()
{
    // Looked up each time, since the list makes its rows again when a note changes.
    const auto pancakes = [this] {
        return findItem(
            [](QQuickItem *item) {
                return item->isVisible() &&
                       item->property("path").toString() == u"Recipes/Pancakes.md"_s;
            },
            qobject_cast<QQuickItem *>(m_noteList.get()));
    };
    QVERIFY(pancakes());
    click(pancakes(), Qt::RightButton);
    QTRY_VERIFY(menuEntry(u"Make a task"_s));
    click(menuEntry(u"Make a task"_s));
    QTRY_VERIFY(m_vault->isTask(u"Recipes/Pancakes.md"_s));

    // Opened again, it offers the other way.
    QTRY_VERIFY(pancakes());
    click(pancakes(), Qt::RightButton);
    QTRY_VERIFY(menuEntry(u"Not a task"_s));
}

void TestLists::notebookMenuMakesANotebookInside()
{
    click(entry(u"Recipes"_s), Qt::RightButton);
    QTRY_VERIFY(menuEntry(u"New notebook inside"_s));
    click(menuEntry(u"New notebook inside"_s));
    QTRY_COMPARE(m_sidebar->property("renamingPath").toString(), u"Recipes/New notebook"_s);
    type(u"Breads"_s);
    QTest::keyClick(m_window.get(), Qt::Key_Return);
    QTRY_VERIFY(QFileInfo(m_root + u"/Recipes/Breads"_s).isDir());
}

void TestLists::trashAsksFirst()
{
    const auto sourdough = [this] {
        return findItem(
            [](QQuickItem *item) {
                return item->isVisible() &&
                       item->property("path").toString() == u"Recipes/Sourdough.md"_s;
            },
            qobject_cast<QQuickItem *>(m_noteList.get()));
    };
    click(sourdough(), Qt::RightButton);
    QTRY_VERIFY(menuEntry(u"Move to trash..."_s));
    click(menuEntry(u"Move to trash..."_s));
    // Nothing goes until the question is answered.
    QVERIFY(QFileInfo::exists(m_root + u"/Recipes/Sourdough.md"_s));
    QQuickItem *confirm = nullptr;
    QTRY_VERIFY((confirm = findItem(
                     [](QQuickItem *item) {
                         return item->isVisible() && item->objectName() == u"confirmTrash"_s;
                     },
                     m_window->contentItem())));
    click(confirm);
    QTRY_VERIFY(!QFileInfo::exists(m_root + u"/Recipes/Sourdough.md"_s));
}

void TestLists::columnBordersDragToResize()
{
    QQmlComponent component(m_engine.get());
    // A row under the border that drags too, as the note list's rows do.
    component.setData("import Jot.Ui\nimport QtQuick\n"
                      "Item { property int asked: -1; property bool reset: false\n"
                      "  property bool rowDragged: false\n"
                      "  Item { width: 400; height: 200\n"
                      "    DragHandler { target: null\n"
                      "      onActiveChanged: if (active) parent.parent.rowDragged = true } }\n"
                      "  ColumnSplitter { objectName: 'splitter'; x: 100; height: 200\n"
                      "    columnWidth: 100; onResized: width => parent.asked = width\n"
                      "    onReset: parent.reset = true } }\n",
                      QUrl(u"qrc:/splitter.qml"_s));
    std::unique_ptr<QObject> holder(component.create());
    QVERIFY2(holder, qPrintable(component.errorString()));
    auto *item = qobject_cast<QQuickItem *>(holder.get());
    item->setParentItem(m_window->contentItem());
    item->setZ(100);
    item->setSize(m_window->size());
    QTest::qWait(50);

    const QPoint from(104, 100);
    QTest::mousePress(m_window.get(), Qt::LeftButton, {}, from);
    // Over the rows of the list beneath, whose own drags must not take over.
    for (int step = 1; step <= 6; ++step)
        QTest::mouseMove(m_window.get(), from + QPoint(10 * step, 0), 20);
    QTest::mouseRelease(m_window.get(), Qt::LeftButton, {}, from + QPoint(60, 0));
    QCOMPARE(holder->property("asked").toInt(), 160);
    QVERIFY(!holder->property("rowDragged").toBool());

    // A double click asks for the theme's width again.
    QTest::mouseDClick(m_window.get(), Qt::LeftButton, {}, from);
    QVERIFY(holder->property("reset").toBool());
}

void TestLists::deleteAsksFirstForNotesAndNotebooks()
{
    const auto confirm = [this] {
        return findItem(
            [](QQuickItem *item) {
                return item->isVisible() && item->objectName() == u"confirmTrash"_s;
            },
            m_window->contentItem());
    };
    // A note: chosen in the list, then Delete.
    QQuickItem *pancakes = findItem(
        [](QQuickItem *item) {
            return item->isVisible() &&
                   item->property("path").toString() == u"Recipes/Pancakes.md"_s;
        },
        qobject_cast<QQuickItem *>(m_noteList.get()));
    click(pancakes);
    QTest::keyClick(m_window.get(), Qt::Key_Delete);
    QTRY_VERIFY(confirm());
    QVERIFY(QFileInfo::exists(m_root + u"/Recipes/Pancakes.md"_s));
    click(confirm());
    QTRY_VERIFY(!QFileInfo::exists(m_root + u"/Recipes/Pancakes.md"_s));

    // A notebook: chosen in the sidebar, then Delete.
    QTRY_VERIFY(!confirm());
    click(entry(u"Places"_s));
    QTest::keyClick(m_window.get(), Qt::Key_Delete);
    QTRY_VERIFY(confirm());
    QVERIFY(QFileInfo(m_root + u"/Ashfall/Places"_s).isDir());
    click(confirm());
    QTRY_VERIFY(!QFileInfo::exists(m_root + u"/Ashfall/Places"_s));
}

void TestLists::searchHandsTheKeysBackToTheList()
{
    QQuickItem *list = findItem([](QQuickItem *item) { return item->inherits("QQuickListView"); },
                                qobject_cast<QQuickItem *>(m_noteList.get()));
    QVERIFY(list);
    const auto search = [this] {
        QMetaObject::invokeMethod(m_noteList.get(), "focusSearch");
        type(u"pan"_s);
        QCOMPARE(m_vault->notes()->property("query").toString(), u"pan"_s);
    };

    // Return keeps what was found and goes to the list.
    search();
    QTest::keyClick(m_window.get(), Qt::Key_Return);
    QVERIFY(list->hasActiveFocus());
    QCOMPARE(m_vault->notes()->property("query").toString(), u"pan"_s);

    // Escape clears it; in vi mode it leaves the box too, so j and k work at once.
    search();
    QTest::keyClick(m_window.get(), Qt::Key_Escape);
    QCOMPARE(m_vault->notes()->property("query").toString(), QString());
    QVERIFY(!list->hasActiveFocus());
    m_settings->setViMode(true);
    search();
    QTest::keyClick(m_window.get(), Qt::Key_Escape);
    QCOMPARE(m_vault->notes()->property("query").toString(), QString());
    QVERIFY(list->hasActiveFocus());
}

QTEST_MAIN(TestLists)
#include "tst_lists.moc"
