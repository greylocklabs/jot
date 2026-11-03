#include "MarkdownStyler.h"
#include "SettingsForeign.h"
#include "ThemeController.h"
#include "VaultController.h"

#include "jot/Settings.h"

#include <QDir>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QInputMethodEvent>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickTextDocument>
#include <QQuickWindow>
#include <QRegularExpression>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QTextDocument>
#include <QtQml/qqmlextensionplugin.h>

#include <algorithm>

Q_IMPORT_QML_PLUGIN(Jot_UiPlugin)

using namespace jot;
using namespace Qt::StringLiterals;

/// Drives the real editor in a window, since what matters is the text being drawn and
/// saved, which the models alone cannot show.
class TestNoteEditor : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase() { QQuickStyle::setStyle(u"Basic"_s); }
    void init();
    void cleanup();

    void drawsTheNoteAndKeepsDrawingItAcrossSwitches();
    void opensALongNoteAtTheTop();
    void theTitleFollowsAnEditedTitleKey();
    void aTickFromTheTaskViewKeepsTheEditorsUndo();
    void undoesATickOnTheShownNote();
    void goesBackToWhereTheCursorWas();
    void typingIsDrawnAndSaved();
    void typingKeepsWindowsLineEndings();
    void anOutsideEditKeepsTheCursorWhereItWas();
    void showsImagesInPlaceOfTheirLinks();
    void switchingAwayFromAPictureLeavesNoStalePlace();
    void formatsAndUndoesInOneStep();
    void formatsFromTheKeyboard();
    void makesTheNoteATaskWithADueDate();
    void continuesAndNestsLists();
    void fillsInTablesWithTheKeys();
    void completesWikilinksFromTheKeyboard();
    void togglesSourceMode();
    void writesInTheChosenFont();
    void readsTheNoteWithLinksAndBoxesToTick();
    void tickingARepeatingToDoWritesTheNext();
    void editsInViModes();
    void jumpsAlongALineInVi();
    void takesTextInputOnlyInInsertMode();
    void staysQuickAndStillInALongNote();
    void composesWithAnInputMethod();

private:
    /// Pixels in the body's area that are not the background, as a measure of text drawn.
    int inkBelowTitle();
    /// Pixels of the sample picture's lower half, a blue nothing else in the editor has.
    int blueInk();
    QQuickItem *find(const QString &name, QQuickItem *from = nullptr) const;

    std::unique_ptr<QTemporaryDir> m_dir;
    std::unique_ptr<Settings> m_settings;
    std::unique_ptr<VaultController> m_vault;
    std::unique_ptr<QQmlEngine> m_engine;
    std::unique_ptr<QQuickWindow> m_window;
    std::unique_ptr<QObject> m_editor;
    QString m_root;
};

void TestNoteEditor::init()
{
    // A binding that breaks in the editor or its bars prints a warning naming the QML file.
    QTest::failOnWarning(QRegularExpression(u"\\.qml:"_s));
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
    // The editor reads the Settings singleton, which would otherwise be the user's own file.
    SettingsForeign::s_instance = m_settings.get();
    m_vault = std::make_unique<VaultController>(*m_settings);
    m_vault->setIndexPath(m_dir->filePath(u"index.sqlite"_s));
    m_vault->setSaveDelay(50);
    VaultController::setInstance(m_vault.get());
    QVERIFY(m_vault->openPath(m_root));
    m_vault->setCurrent(u"Ashfall/Characters/Mara Vey.md"_s);

    m_engine = std::make_unique<QQmlEngine>();
    QQmlComponent component(m_engine.get());
    component.loadFromModule("Jot.Ui", "NoteEditor");
    m_editor.reset(component.create());
    QVERIFY2(m_editor, qPrintable(component.errorString()));
    m_window = std::make_unique<QQuickWindow>();
    m_window->setColor(ThemeController::instance()->bg());
    m_window->resize(900, 700);
    auto *item = qobject_cast<QQuickItem *>(m_editor.get());
    item->setParentItem(m_window->contentItem());
    item->setSize(m_window->size());
    m_window->show();
    QVERIFY(QTest::qWaitForWindowExposed(m_window.get()));
}

void TestNoteEditor::cleanup()
{
    m_editor.reset();
    m_window.reset();
    m_engine.reset();
    VaultController::setInstance(nullptr);
    m_vault.reset();
    SettingsForeign::s_instance = nullptr;
}

int TestNoteEditor::inkBelowTitle()
{
    const QImage image = m_window->grabWindow();
    const QColor background = ThemeController::instance()->bg();
    int ink = 0;
    for (int y = 140; y < image.height(); y += 2) {
        for (int x = 0; x < image.width(); x += 2) {
            const QColor pixel = image.pixelColor(x, y);
            if (std::abs(pixel.red() - background.red()) +
                    std::abs(pixel.green() - background.green()) +
                    std::abs(pixel.blue() - background.blue()) >
                90)
                ++ink;
        }
    }
    return ink;
}

QQuickItem *TestNoteEditor::find(const QString &name, QQuickItem *from) const
{
    if (!from)
        from = qobject_cast<QQuickItem *>(m_editor.get());
    for (QQuickItem *child : from->childItems()) {
        if (child->property("objectName").toString() == name)
            return child;
        if (QQuickItem *found = find(name, child))
            return found;
    }
    return nullptr;
}

void TestNoteEditor::drawsTheNoteAndKeepsDrawingItAcrossSwitches()
{
    QTRY_VERIFY_WITH_TIMEOUT(inkBelowTitle() > 500, 3000);
    m_vault->setCurrent(u"Ashfall/Pitch.md"_s);
    QTRY_VERIFY_WITH_TIMEOUT(inkBelowTitle() > 200, 3000);
    m_vault->setCurrent(u"Recipes/Pancakes.md"_s);
    QTRY_VERIFY_WITH_TIMEOUT(inkBelowTitle() > 100, 3000);
}

void TestNoteEditor::opensALongNoteAtTheTop()
{
    // Mara Vey, with her backlinks and mentions, runs past this window, and the editor is
    // laid out with no height before the window gives it one.
    QQuickItem *scroll = find(u"scroll"_s);
    QVERIFY(scroll);
    QTest::qWait(200);
    QVERIFY(scroll->property("contentHeight").toReal() > scroll->height());
    QCOMPARE(scroll->property("contentY").toReal(), 0.0);

    // Another long note, opened from part way down this one, starts at its top.
    QFile file(m_root + u"/Long.md"_s);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(QByteArray("A line.\n\n").repeated(80));
    file.close();
    QTRY_VERIFY_WITH_TIMEOUT(m_vault->index()->note(u"Long.md"_s), 3000);
    scroll->setProperty("contentY", 150.0);
    m_vault->setCurrent(u"Long.md"_s);
    QTest::qWait(200);
    QCOMPARE(scroll->property("contentY").toReal(), 0.0);

    // The same note changed on disk keeps its place.
    scroll->setProperty("contentY", 300.0);
    QVERIFY(file.open(QIODevice::Append));
    file.write("One more.\n");
    file.close();
    QTRY_VERIFY_WITH_TIMEOUT(m_vault->currentText().endsWith(u"One more.\n"_s), 3000);
    QTest::qWait(200);
    QCOMPARE(scroll->property("contentY").toReal(), 300.0);
}

void TestNoteEditor::typingIsDrawnAndSaved()
{
    m_vault->setCurrent(u"Welcome.md"_s);
    QQuickItem *body = find(u"body"_s);
    QVERIFY(body);
    body->forceActiveFocus();
    body->setProperty("cursorPosition", body->property("length"));
    for (const QChar c : u"\n# A heading typed here\nAnd **bold** words."_s) {
        if (c == u'\n')
            QTest::keyClick(m_window.get(), Qt::Key_Return);
        else
            QTest::keyClick(m_window.get(), c.toLatin1());
    }
    QTRY_VERIFY_WITH_TIMEOUT(m_vault->currentText().contains(u"**bold** words."_s), 2000);
    QTRY_VERIFY_WITH_TIMEOUT(!m_vault->unsaved(), 2000);
    QTRY_VERIFY_WITH_TIMEOUT(inkBelowTitle() > 400, 3000);

    QString error;
    QVERIFY(notefile::read(m_root + u"/Welcome.md"_s, &error)->endsWith(u"And **bold** words."_s));
}

void TestNoteEditor::typingKeepsWindowsLineEndings()
{
    QFile file(m_root + u"/Windows.md"_s);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("one\r\ntwo\r\n");
    file.close();
    // Written before the window opened, so the watcher may not have told the index yet.
    QTRY_VERIFY_WITH_TIMEOUT(m_vault->index()->note(u"Windows.md"_s), 3000);
    m_vault->setCurrent(u"Windows.md"_s);
    QQuickItem *body = find(u"body"_s);
    body->forceActiveFocus();
    body->setProperty("cursorPosition", 3);
    QTest::keyClick(m_window.get(), '!');
    QTRY_VERIFY_WITH_TIMEOUT(!m_vault->unsaved(), 2000);
    QVERIFY(file.open(QIODevice::ReadOnly));
    QCOMPARE(file.readAll(), QByteArray("one!\r\ntwo\r\n"));
}

void TestNoteEditor::aTickFromTheTaskViewKeepsTheEditorsUndo()
{
    m_vault->setCurrent(u"Inbox/Groceries.md"_s);
    QQuickItem *body = find(u"body"_s);
    body->forceActiveFocus();
    body->setProperty("cursorPosition", body->property("text").toString().size());
    for (const char c : QByteArrayView("- [ ] eggs"))
        QTest::keyClick(m_window.get(), c);
    QTRY_VERIFY_WITH_TIMEOUT(!m_vault->unsaved(), 2000);
    const QString typed = body->property("text").toString();

    // Ticked from the task view, the editor makes the same change as an edit of its own.
    // Lines count from 1.
    const int line = int(typed.left(typed.indexOf(u"- [ ] oat milk"_s)).count(u'\n')) + 1;
    QVERIFY(m_vault->setTaskDone(u"Inbox/Groceries.md"_s, line, true));
    QVERIFY(body->property("text").toString().contains(u"- [x] oat milk"_s));
    QVERIFY(body->property("text").toString().contains(u"- [ ] eggs"_s));

    // Its undo takes back the tick, then what was typed before it.
    QMetaObject::invokeMethod(body, "undo");
    QCOMPARE(body->property("text").toString(), typed);
    QMetaObject::invokeMethod(body, "undo");
    QVERIFY(!body->property("text").toString().contains(u"eggs"_s));
}

void TestNoteEditor::undoesATickOnTheShownNote()
{
    // Windows line endings, which the editor shows as plain lines, and a byte order mark.
    QFile file(m_root + u"/Windows.md"_s);
    QVERIFY(file.open(QIODevice::WriteOnly));
    const QByteArray untouched = "\xef\xbb\xbfone\r\n- [ ] two\r\nthree\r\n";
    file.write(untouched);
    file.close();
    QTRY_VERIFY_WITH_TIMEOUT(m_vault->index()->note(u"Windows.md"_s), 3000);
    m_vault->setCurrent(u"Windows.md"_s);
    QQuickItem *body = find(u"body"_s);
    const auto text = [body] { return body->property("text").toString(); };
    const auto saved = [&file] {
        file.open(QIODevice::ReadOnly);
        const QByteArray bytes = file.readAll();
        file.close();
        return bytes;
    };
    QCOMPARE(text(), u"\ufeffone\n- [ ] two\nthree\n"_s);
    QVERIFY(!body->property("canUndo").toBool());
    const QString ticked =
        u"\ufeffone\n- [x] two ✅ %1\nthree\n"_s.arg(QDate::currentDate().toString(Qt::ISODate));

    // Each step is an edit in the editor, not a reload, so the editor's own undo keeps it.
    QVERIFY(m_vault->setTaskDone(u"Windows.md"_s, 2, true));
    QCOMPARE(text(), ticked);
    QVERIFY(body->property("canUndo").toBool());
    QVERIFY(m_vault->undoTask());
    QCOMPARE(text(), u"\ufeffone\n- [ ] two\nthree\n"_s);
    QTRY_VERIFY_WITH_TIMEOUT(!m_vault->unsaved(), 2000);
    QCOMPARE(saved(), untouched);
    QVERIFY(m_vault->redoTask());
    QCOMPARE(text(), ticked);
    QTRY_VERIFY_WITH_TIMEOUT(!m_vault->unsaved(), 2000);
    QCOMPARE(saved(), QString(ticked).replace(u"\n"_s, u"\r\n"_s).toUtf8());
}

void TestNoteEditor::goesBackToWhereTheCursorWas()
{
    m_vault->setCurrent(u"Ashfall/Characters/Mara Vey.md"_s);
    QQuickItem *body = find(u"body"_s);
    body->forceActiveFocus();
    body->setProperty("cursorPosition", 40);
    m_vault->setCurrent(u"Welcome.md"_s);
    QCOMPARE(body->property("cursorPosition").toInt(), 0);
    body->setProperty("cursorPosition", 12);

    QVERIFY(m_vault->goBack());
    QCOMPARE(body->property("cursorPosition").toInt(), 40);
    QVERIFY(m_vault->goForward());
    QCOMPARE(body->property("cursorPosition").toInt(), 12);
    // Left untouched since going back to it, it is still where it was.
    QVERIFY(m_vault->goBack());
    QCOMPARE(body->property("cursorPosition").toInt(), 40);
}

void TestNoteEditor::theTitleFollowsAnEditedTitleKey()
{
    QQuickItem *title = find(u"title"_s);
    QQuickItem *body = find(u"body"_s);
    QCOMPARE(title->property("text").toString(), u"Mara Vey"_s);
    // Typed into the body's front matter, as a person would.
    body->forceActiveFocus();
    const QString text = body->property("text").toString();
    body->setProperty("cursorPosition", text.indexOf(u"type: character"_s));
    for (const QChar c : u"title: The Ferrywoman"_s)
        QTest::keyClick(m_window.get(), c.toLatin1());
    QTest::keyClick(m_window.get(), Qt::Key_Return);
    QTRY_VERIFY_WITH_TIMEOUT(!m_vault->unsaved(), 2000);
    QTRY_COMPARE(title->property("text").toString(), u"The Ferrywoman"_s);

    // Clicking into the title and away leaves the new title, not the old one, in the file.
    title->forceActiveFocus();
    body->forceActiveFocus();
    QTRY_VERIFY_WITH_TIMEOUT(!m_vault->unsaved(), 2000);
    QCOMPARE(m_vault->currentTitle(), u"The Ferrywoman"_s);
    QVERIFY(m_vault->currentText().contains(u"title: The Ferrywoman"_s));

    // Changed on disk, as by a sync or another editor, it follows too, even while the title is
    // clicked into, and leaving it then keeps the new title.
    title->forceActiveFocus();
    QFile file(m_root + u"/Ashfall/Characters/Mara Vey.md"_s);
    QVERIFY(file.open(QIODevice::ReadOnly));
    const QByteArray saved = file.readAll();
    file.close();
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write(QByteArray(saved).replace("title: The Ferrywoman", "title: Mara of the Ferry"));
    file.close();
    QTRY_COMPARE_WITH_TIMEOUT(title->property("text").toString(), u"Mara of the Ferry"_s, 3000);
    body->forceActiveFocus();
    QCOMPARE(m_vault->currentTitle(), u"Mara of the Ferry"_s);
    QCOMPARE(m_vault->current(), u"Ashfall/Characters/Mara Vey.md"_s);
}

void TestNoteEditor::anOutsideEditKeepsTheCursorWhereItWas()
{
    QString error;
    QVERIFY(notefile::write(m_root + u"/Draft.md"_s, u"one\ntwo\nthree\n"_s, &error));
    QTRY_VERIFY_WITH_TIMEOUT(m_vault->index()->note(u"Draft.md"_s), 3000);
    m_vault->setSaveDelay(60000);
    m_vault->setCurrent(u"Draft.md"_s);
    QQuickItem *body = find(u"body"_s);
    body->forceActiveFocus();
    // Typing at the end of "three", unsaved, while a line goes in at the top elsewhere.
    body->setProperty("cursorPosition", 13);
    QTest::keyClick(m_window.get(), '!');
    QVERIFY(notefile::write(m_root + u"/Draft.md"_s, u"zero\none\ntwo\nthree\n"_s, &error));
    QTRY_COMPARE_WITH_TIMEOUT(body->property("text").toString(), u"zero\none\ntwo\nthree!\n"_s,
                              3000);
    QCOMPARE(body->property("cursorPosition").toInt(), 19);

    // The same in a note with Windows line breaks, which the editor shows as plain ones.
    QVERIFY(notefile::write(m_root + u"/Windows.md"_s, u"one\r\ntwo\r\nthree\r\n"_s, &error));
    QTRY_VERIFY_WITH_TIMEOUT(m_vault->index()->note(u"Windows.md"_s), 3000);
    m_vault->setCurrent(u"Windows.md"_s);
    body->forceActiveFocus();
    body->setProperty("cursorPosition", 13);
    QTest::keyClick(m_window.get(), '!');
    QVERIFY(
        notefile::write(m_root + u"/Windows.md"_s, u"zero\r\none\r\ntwo\r\nthree\r\n"_s, &error));
    QTRY_VERIFY_WITH_TIMEOUT(body->property("text").toString().contains(u"zero"_s), 3000);
    QCOMPARE(body->property("cursorPosition").toInt(), 19);
}

void TestNoteEditor::showsImagesInPlaceOfTheirLinks()
{
    m_vault->setCurrent(u"Ashfall/Places/The Ferry.md"_s);
    QTRY_VERIFY_WITH_TIMEOUT(blueInk() > 1000, 3000);
}

int TestNoteEditor::blueInk()
{
    const QImage image = m_window->grabWindow();
    int count = 0;
    for (int y = 0; y < image.height(); y += 2) {
        for (int x = 0; x < image.width(); x += 2) {
            const QColor c = image.pixelColor(x, y);
            count += c.blue() > 200 && c.red() < 140 && c.green() > 110 ? 1 : 0;
        }
    }
    return count;
}

void TestNoteEditor::switchingAwayFromAPictureLeavesNoStalePlace()
{
    // The picture's place is from The Ferry; a shorter note must not be asked for it.
    QTest::failOnWarning(QRegularExpression(u"out of range"_s));
    m_vault->setCurrent(u"Ashfall/Places/The Ferry.md"_s);
    QTRY_VERIFY_WITH_TIMEOUT(blueInk() > 1000, 3000);
    m_vault->setCurrent(u"Welcome.md"_s);
    QTRY_VERIFY_WITH_TIMEOUT(blueInk() < 50, 3000);
}

void TestNoteEditor::formatsAndUndoesInOneStep()
{
    m_vault->setCurrent(u"Welcome.md"_s);
    QQuickItem *body = find(u"body"_s);
    const QString before = body->property("text").toString();
    const int word = int(before.indexOf(u"plain"_s));
    QVERIFY(word >= 0);
    QMetaObject::invokeMethod(body, "select", Q_ARG(int, word), Q_ARG(int, word + 5));

    QMetaObject::invokeMethod(m_editor.get(), "format", Q_ARG(QVariant, u"bold"_s),
                              Q_ARG(QVariant, QString()));

    QCOMPARE(body->property("text").toString().mid(word, 9), u"**plain**"_s);
    QCOMPARE(body->property("selectedText").toString(), u"plain"_s);
    QTRY_VERIFY_WITH_TIMEOUT(!m_vault->unsaved(), 2000);
    QString error;
    QVERIFY(notefile::read(m_root + u"/Welcome.md"_s, &error)->contains(u"**plain**"_s));

    QMetaObject::invokeMethod(body, "undo");
    QCOMPARE(body->property("text").toString(), before);
}

void TestNoteEditor::formatsFromTheKeyboard()
{
    m_vault->setCurrent(u"Welcome.md"_s);
    QQuickItem *body = find(u"body"_s);
    body->forceActiveFocus();
    const QString before = body->property("text").toString();
    const int word = int(before.indexOf(u"plain"_s));
    body->setProperty("cursorPosition", word + 1);

    QTest::keyClick(m_window.get(), Qt::Key_I, Qt::ControlModifier);
    QCOMPARE(body->property("text").toString().mid(word, 7), u"*plain*"_s);
    QTest::keyClick(m_window.get(), Qt::Key_Return, Qt::ControlModifier);
    QVERIFY(body->property("text").toString().contains(u"- [ ] "_s));
}

void TestNoteEditor::makesTheNoteATaskWithADueDate()
{
    m_vault->setCurrent(u"Recipes/Pancakes.md"_s);
    QMetaObject::invokeMethod(m_editor.get(), "format", Q_ARG(QVariant, u"taskNote"_s),
                              Q_ARG(QVariant, QString()));
    QMetaObject::invokeMethod(m_editor.get(), "format", Q_ARG(QVariant, u"due"_s),
                              Q_ARG(QVariant, u"2026-11-02"_s));
    QTRY_VERIFY_WITH_TIMEOUT(!m_vault->unsaved(), 2000);

    QString error;
    const QString saved = *notefile::read(m_root + u"/Recipes/Pancakes.md"_s, &error);
    QVERIFY2(saved.contains(u"task: open\n"_s), qPrintable(saved));
    QVERIFY(saved.contains(u"due: 2026-11-02\n"_s));
    QTRY_VERIFY_WITH_TIMEOUT(m_vault->index()->openTaskNotes().size() == 2, 3000);

    QMetaObject::invokeMethod(m_editor.get(), "format", Q_ARG(QVariant, u"taskNote"_s),
                              Q_ARG(QVariant, QString()));
    QTRY_VERIFY_WITH_TIMEOUT(!m_vault->unsaved(), 2000);
    const QString undone = *notefile::read(m_root + u"/Recipes/Pancakes.md"_s, &error);
    // The due date is the user's own, so it stays for when it is a task again.
    QVERIFY(!undone.contains(u"task:"_s));
    QVERIFY(undone.contains(u"due: 2026-11-02"_s));
}

void TestNoteEditor::continuesAndNestsLists()
{
    m_vault->setCurrent(u"Welcome.md"_s);
    QQuickItem *body = find(u"body"_s);
    body->forceActiveFocus();
    body->setProperty("cursorPosition", body->property("length"));
    const QString before = body->property("text").toString();
    const auto type = [this](const QString &text) {
        for (const QChar c : text)
            QTest::keyClick(m_window.get(), c.toLatin1());
    };

    QTest::keyClick(m_window.get(), Qt::Key_Return);
    type(u"- eggs"_s);
    QTest::keyClick(m_window.get(), Qt::Key_Return);
    QTest::keyClick(m_window.get(), Qt::Key_Tab);
    type(u"brown"_s);
    QTest::keyClick(m_window.get(), Qt::Key_Return);
    QTest::keyClick(m_window.get(), Qt::Key_Backtab);
    type(u"milk"_s);
    QTest::keyClick(m_window.get(), Qt::Key_Return);
    QTest::keyClick(m_window.get(), Qt::Key_Return);
    type(u"done"_s);
    QCOMPARE(body->property("text").toString().mid(before.size()),
             u"\n- eggs\n  - brown\n- milk\ndone"_s);

    // Shift+Return is a plain line break, even in a list.
    QTest::keyClick(m_window.get(), Qt::Key_Return, Qt::ShiftModifier);
    QVERIFY(body->property("text").toString().endsWith(u"done\n"_s));
}

void TestNoteEditor::fillsInTablesWithTheKeys()
{
    m_vault->setCurrent(u"Welcome.md"_s);
    QQuickItem *body = find(u"body"_s);
    body->forceActiveFocus();
    body->setProperty("cursorPosition", body->property("length"));
    const QString before = body->property("text").toString();
    const auto type = [this](const QString &text) {
        for (const QChar c : text)
            QTest::keyClick(m_window.get(), c.toLatin1());
    };

    // The note ends in a line break, so one more leaves a blank line above the table. The
    // table button selects the first heading, so typing replaces it.
    QMetaObject::invokeMethod(m_editor.get(), "format", Q_ARG(QVariant, u"table"_s),
                              Q_ARG(QVariant, QString()));
    type(u"Name"_s);
    QTest::keyClick(m_window.get(), Qt::Key_Tab);
    type(u"Role"_s);
    QTest::keyClick(m_window.get(), Qt::Key_Tab);
    type(u"Mara"_s);
    QTest::keyClick(m_window.get(), Qt::Key_Tab);
    type(u"Captain"_s);
    QTest::keyClick(m_window.get(), Qt::Key_Return);
    QTest::keyClick(m_window.get(), Qt::Key_Return);
    type(u"After"_s);

    QCOMPARE(body->property("text").toString().mid(before.size()),
             u"\n| Name | Role    |\n| ---- | ------- |\n| Mara | Captain |\nAfter"_s);
}

void TestNoteEditor::completesWikilinksFromTheKeyboard()
{
    m_vault->setCurrent(u"Welcome.md"_s);
    QQuickItem *body = find(u"body"_s);
    body->forceActiveFocus();
    body->setProperty("cursorPosition", body->property("length"));
    const QString before = body->property("text").toString();
    const auto type = [this](const QString &text) {
        for (const QChar c : text)
            QTest::keyClick(m_window.get(), c.toLatin1());
    };

    type(u"See [[sourd"_s);
    QTest::keyClick(m_window.get(), Qt::Key_Return);
    type(u" and [[ferry"_s);
    // Escape puts the notes away, so Return breaks the line instead.
    QTest::keyClick(m_window.get(), Qt::Key_Escape);
    QTest::keyClick(m_window.get(), Qt::Key_Return);

    QCOMPARE(body->property("text").toString().mid(before.size()),
             u"See [[Sourdough]] and [[ferry\n"_s);

    // Moving into a finished link offers nothing, so Return still breaks the line.
    const int inside = int(body->property("text").toString().indexOf(u"Sourd"_s)) + 2;
    body->setProperty("cursorPosition", inside);
    QTest::keyClick(m_window.get(), Qt::Key_Return);
    QVERIFY(body->property("text").toString().contains(u"[[So\nurdough]]"_s));
}

void TestNoteEditor::togglesSourceMode()
{
    m_vault->setCurrent(u"Ashfall/Places/The Ferry.md"_s);
    find(u"body"_s)->forceActiveFocus();
    QTRY_VERIFY_WITH_TIMEOUT(blueInk() > 1000, 3000);

    // As written, the picture is its link again; Ctrl+/ again brings it back.
    QTest::keyClick(m_window.get(), Qt::Key_Slash, Qt::ControlModifier);
    QVERIFY(m_settings->sourceMode());
    QTRY_VERIFY_WITH_TIMEOUT(blueInk() < 50, 3000);
    QTest::keyClick(m_window.get(), Qt::Key_Slash, Qt::ControlModifier);
    QVERIFY(!m_settings->sourceMode());
    QTRY_VERIFY_WITH_TIMEOUT(blueInk() > 1000, 3000);

    // The format row's switch does the same, its thumb sliding under the choice.
    QQuickItem *modes = find(u"modes"_s);
    QVERIFY(modes);
    QQuickItem *thumb = find(u"thumb"_s, modes);
    // At rest under Formatted, once back from the Ctrl+/ above.
    const qreal start = ThemeController::instance()->space1();
    QTRY_COMPARE(thumb->x(), start);
    const auto click = [this, modes](int choice) {
        QQuickItem *item = find(u"choice%1"_s.arg(choice), modes);
        QTest::mouseClick(
            m_window.get(), Qt::LeftButton, {},
            item->mapToScene(QPointF(item->width() / 2, item->height() / 2)).toPoint());
        return item;
    };
    QQuickItem *source = click(1);
    QVERIFY(m_settings->sourceMode());
    QTRY_COMPARE(thumb->x(), start + source->x());
    QTRY_COMPARE(thumb->width(), source->width());
    click(0);
    QVERIFY(!m_settings->sourceMode());
    QTRY_COMPARE(thumb->x(), start);
    // Read draws the note to read, and Formatted goes back to writing.
    QQuickItem *read = click(2);
    QVERIFY(m_settings->readMode());
    QTRY_VERIFY(!find(u"body"_s)->isVisible());
    QTRY_COMPARE(thumb->x(), start + read->x());
    click(0);
    QVERIFY(!m_settings->readMode());
    QVERIFY(!m_settings->sourceMode());
    // Slimmer than the row, so it does not touch its edges.
    const int slim = ThemeController::instance()->switchHeight();
    QCOMPARE(modes->height(), qreal(slim));
    QVERIFY(find(u"formatBar"_s)->height() >= slim + 2 * ThemeController::instance()->space2());
}

void TestNoteEditor::readsTheNoteWithLinksAndBoxesToTick()
{
    QString error;
    QVERIFY(notefile::write(
        m_root + u"/Reading.md"_s,
        u"---\ntags: [demo]\n---\n# Today\n\n- [ ] Buy rope\n- [x] Patch the hull\n\nSee [[Mara Vey]] and #ferry.\n\n> A quote.\n\n| Crossing | Fare |\n|---|---|\n| Dawn | 2 |\n\n```\nlet fare = 2\n```\n"_s,
        &error));
    QTRY_VERIFY_WITH_TIMEOUT(m_vault->index()->note(u"Reading.md"_s), 3000);
    m_vault->setCurrent(u"Reading.md"_s);
    m_settings->setReadMode(true);
    QQuickItem *reader = find(u"reader"_s);
    QVERIFY(reader);
    QTRY_VERIFY(reader->isVisible());
    QVERIFY(!find(u"body"_s)->isVisible());
    // Drawn, not written: no front matter, marks or brackets.
    auto *doc = reader->property("textDocument").value<QQuickTextDocument *>()->textDocument();
    QTRY_VERIFY(doc->toPlainText().contains(u"Buy rope"_s));
    const QString shown = doc->toPlainText();
    QVERIFY2(!shown.contains(u"tags:"_s) && !shown.contains(u"[["_s) && !shown.contains(u"# "_s),
             qPrintable(shown));

    // A click left of the task's text ticks it in the file. The view lays itself out again as it
    // settles into the editor's place, so the click is aimed afresh until it lands.
    const auto ticked = [&] {
        return notefile::read(m_root + u"/Reading.md"_s, &error)->contains(u"- [x] Buy rope"_s);
    };
    for (int attempt = 0; attempt < 10 && !ticked(); ++attempt) {
        QRectF line;
        QMetaObject::invokeMethod(reader, "positionToRectangle", Q_RETURN_ARG(QRectF, line),
                                  Q_ARG(int, int(doc->toPlainText().indexOf(u"Buy rope"_s))));
        QTest::mouseClick(m_window.get(), Qt::LeftButton, {},
                          reader->mapToScene(QPointF(line.x() - 6, line.center().y())).toPoint());
        if (QTest::qWaitFor(ticked, 300))
            break;
    }
    QVERIFY(ticked());

    // Its links lead to the note, and to the tag's notes.
    QMetaObject::invokeMethod(reader, "linkActivated", Q_ARG(QString, u"jot-tag:ferry"_s));
    QCOMPARE(m_vault->notes()->tag(), u"ferry"_s);
    QMetaObject::invokeMethod(reader, "linkActivated", Q_ARG(QString, u"jot-note:Mara%20Vey"_s));
    QCOMPARE(m_vault->current(), u"Ashfall/Characters/Mara Vey.md"_s);
    // Ctrl+/ there goes back to writing, in Source.
    m_vault->setCurrent(u"Reading.md"_s);
    reader->forceActiveFocus();
    QTest::keyClick(m_window.get(), Qt::Key_Slash, Qt::ControlModifier);
    QVERIFY(!m_settings->readMode());
    QVERIFY(m_settings->sourceMode());
    QTRY_VERIFY(find(u"body"_s)->isVisible());
    m_settings->setSourceMode(false);
}

void TestNoteEditor::tickingARepeatingToDoWritesTheNext()
{
    const QString today = QDate::currentDate().toString(Qt::ISODate);
    QString error;
    QVERIFY(notefile::write(m_root + u"/Chores.md"_s,
                            u"- [ ] Water 🔁 every day 📅 %1\n"_s.arg(today), &error));
    QTRY_VERIFY_WITH_TIMEOUT(m_vault->index()->note(u"Chores.md"_s), 3000);
    m_vault->setCurrent(u"Chores.md"_s);
    QQuickItem *body = find(u"body"_s);
    QPointF placed(-1, -1);
    QTRY_VERIFY([&] {
        const QPointF now = body->mapToScene(QPointF(0, 0));
        const bool settled = now == placed;
        placed = now;
        return settled;
    }());
    // A click on the box, where the mark between the brackets is.
    QRectF mark;
    QMetaObject::invokeMethod(body, "positionToRectangle", Q_RETURN_ARG(QRectF, mark),
                              Q_ARG(int, 3));
    QTest::mouseClick(m_window.get(), Qt::LeftButton, {},
                      body->mapToScene(mark.center()).toPoint());
    const QString tomorrow = QDate::currentDate().addDays(1).toString(Qt::ISODate);
    QTRY_COMPARE_WITH_TIMEOUT(
        body->property("text").toString(),
        u"- [ ] Water 🔁 every day 📅 %1\n- [x] Water 🔁 every day 📅 %2 ✅ %2\n"_s.arg(tomorrow,
                                                                                        today),
        3000);
}

void TestNoteEditor::writesInTheChosenFont()
{
    QQuickItem *body = find(u"body"_s);
    auto *styler = body->findChild<MarkdownStyler *>();
    QVERIFY(styler);
    const auto family = [body] { return body->property("font").value<QFont>().family(); };
    const ThemeController &theme = *ThemeController::instance();
    // The default theme leaves its mono family empty, so mono must not come out empty.
    QVERIFY(theme.monoFontFamily().isEmpty());
    QCOMPARE(family(), theme.editorFontFamily());

    m_settings->setNoteFont(u"mono"_s);
    QCOMPARE(family(), u"monospace"_s);
    QCOMPARE(styler->fontFamily(), u"monospace"_s);
    m_settings->setNoteFont(u"serif"_s);
    QCOMPARE(family(), theme.noteFontFamily(u"serif"_s));
    QVERIFY(family() != theme.editorFontFamily());
    m_settings->setNoteFont({});
    QCOMPARE(family(), theme.editorFontFamily());
    QCOMPARE(styler->fontFamily(), theme.editorFontFamily());
}

void TestNoteEditor::editsInViModes()
{
    m_settings->setViMode(true);
    m_vault->setCurrent(u"Welcome.md"_s);
    QQuickItem *body = find(u"body"_s);
    auto *vi = body->findChild<QObject *>(u"vi"_s);
    QVERIFY(vi);
    body->forceActiveFocus();
    body->setProperty("text", u"alpha beta\ngamma"_s);
    body->setProperty("cursorPosition", 0);
    const auto text = [body] { return body->property("text").toString(); };
    const auto type = [this](const QString &keys) {
        for (const QChar c : keys)
            QTest::keyClick(m_window.get(), c.toLatin1());
    };
    QSignalSpy left(m_editor.get(), SIGNAL(leftNote()));

    // A note opens in normal mode, where letters move and edit rather than type.
    QCOMPARE(vi->property("mode").toString(), u"normal"_s);
    type(u"xw"_s);
    QCOMPARE(text(), u"lpha beta\ngamma"_s);
    QCOMPARE(body->property("cursorPosition").toInt(), 5);
    type(u"i"_s);
    QCOMPARE(vi->property("mode").toString(), u"insert"_s);
    type(u"big "_s);
    QTest::keyClick(m_window.get(), Qt::Key_Escape);
    QCOMPARE(vi->property("mode").toString(), u"normal"_s);
    QCOMPARE(text(), u"lpha big beta\ngamma"_s);

    // Lines are cut and pasted whole, and undone.
    type(u"jdd"_s);
    QCOMPARE(text(), u"lpha big beta"_s);
    type(u"p"_s);
    QCOMPARE(text(), u"lpha big beta\ngamma"_s);
    type(u"u"_s);
    QCOMPARE(text(), u"lpha big beta"_s);

    // Visual mode selects with the same moves, and cuts what it selected.
    type(u"0vll"_s);
    QCOMPARE(vi->property("mode").toString(), u"visual"_s);
    type(u"d"_s);
    QCOMPARE(text(), u"ha big beta"_s);
    QCOMPARE(vi->property("mode").toString(), u"normal"_s);

    type(u"oz"_s);
    QTest::keyClick(m_window.get(), Qt::Key_Escape);
    QCOMPARE(text(), u"ha big beta\nz"_s);
    // With no router, the leader is only swallowed.
    type(u","_s);
    QCOMPARE(text(), u"ha big beta\nz"_s);
    // Ctrl and a letter vi does not use still formats.
    type(u"0"_s);
    QTest::keyClick(m_window.get(), Qt::Key_B, Qt::ControlModifier);
    QCOMPARE(text(), u"ha big beta\n**z**"_s);

    // Escape in normal mode hands the keyboard back; off, keys type as ever.
    QTest::keyClick(m_window.get(), Qt::Key_Escape);
    QCOMPARE(left.size(), 1);
    m_settings->setViMode(false);
    body->forceActiveFocus();
    body->setProperty("cursorPosition", 0);
    type(u"j"_s);
    QCOMPARE(text(), u"jha big beta\n**z**"_s);
}

void TestNoteEditor::takesTextInputOnlyInInsertMode()
{
    m_settings->setViMode(true);
    m_vault->setCurrent(u"Welcome.md"_s);
    QQuickItem *body = find(u"body"_s);
    auto *vi = body->findChild<QObject *>(u"vi"_s);
    QVERIFY(vi);
    body->forceActiveFocus();
    const auto takesInput = [body] {
        QInputMethodQueryEvent query(Qt::ImEnabled | Qt::ImCursorRectangle);
        QCoreApplication::sendEvent(body, &query);
        // The editor still says where its cursor is.
        if (!query.value(Qt::ImCursorRectangle).toRectF().isValid())
            return QVariant();
        return query.value(Qt::ImEnabled);
    };

    // A held key repeats in normal and visual mode rather than offering accented letters.
    QCOMPARE(takesInput(), QVariant(false));
    QTest::keyClick(m_window.get(), 'v');
    QCOMPARE(takesInput(), QVariant(false));
    QTest::keyClick(m_window.get(), Qt::Key_Escape);
    QTest::keyClick(m_window.get(), 'i');
    QCOMPARE(takesInput(), QVariant(true));
    QTest::keyClick(m_window.get(), Qt::Key_Escape);
    QCOMPARE(takesInput(), QVariant(false));
    m_settings->setViMode(false);
    QCOMPARE(takesInput(), QVariant(true));
}

void TestNoteEditor::jumpsAlongALineInVi()
{
    m_settings->setViMode(true);
    m_vault->setCurrent(u"Welcome.md"_s);
    QQuickItem *body = find(u"body"_s);
    auto *vi = body->findChild<QObject *>(u"vi"_s);
    QVERIFY(vi);
    body->forceActiveFocus();
    body->setProperty("text", u"one, two, three\nfour"_s);
    body->setProperty("cursorPosition", 0);
    const auto text = [body] { return body->property("text").toString(); };
    const auto at = [body] { return body->property("cursorPosition").toInt(); };
    const auto type = [this](const QString &keys) {
        for (const QChar c : keys)
            QTest::keyClick(m_window.get(), c.toLatin1());
    };

    // f and t go along the line to a character, F and T back; ; and , repeat either way.
    type(u"ft"_s);
    QCOMPARE(at(), 5);
    type(u";"_s);
    QCOMPARE(at(), 10);
    type(u","_s);
    QCOMPARE(at(), 5);
    type(u";"_s);
    QCOMPARE(at(), 10);
    type(u"Fo"_s);
    QCOMPARE(at(), 7);
    type(u"t,"_s);
    QCOMPARE(at(), 7);
    type(u";"_s);
    QCOMPARE(at(), 7);
    type(u"0te"_s);
    QCOMPARE(at(), 1);
    type(u";"_s);
    QCOMPARE(at(), 12);
    type(u"Tn"_s);
    QCOMPARE(at(), 2);
    // Nothing to find on the line, or past its end, leaves the cursor where it was.
    type(u"fz"_s);
    QCOMPARE(at(), 2);
    type(u"$fr"_s);
    QCOMPARE(at(), 15);
    QCOMPARE(text(), u"one, two, three\nfour"_s);

    // d and c cut up to it, the character too going forward; c then types.
    type(u"0dt,"_s);
    QCOMPARE(text(), u", two, three\nfour"_s);
    // $ is past the line's last character, so back from there takes all of "three".
    type(u"$dFt"_s);
    QCOMPARE(text(), u", two, \nfour"_s);
    type(u"0cfwnew"_s);
    QCOMPARE(vi->property("mode").toString(), u"insert"_s);
    QCOMPARE(text(), u"newo, \nfour"_s);
    QTest::keyClick(m_window.get(), Qt::Key_Escape);

    // In visual mode, they select as they go.
    type(u"0vf,"_s);
    QCOMPARE(body->property("selectedText").toString(), u"newo"_s);
    type(u"d"_s);
    QCOMPARE(text(), u", \nfour"_s);
    m_settings->setViMode(false);
}

void TestNoteEditor::staysQuickAndStillInALongNote()
{
    // Some 2,400 lines with every kind of Markdown the editor styles.
    QString text = u"# A long note\n\n"_s;
    for (int i = 1; i <= 150; ++i) {
        text += u"## Section %1\n\n"_s.arg(i);
        text += u"A paragraph with **bold**, *italic*, `code`, a [[Mara Vey]] link and a "
                "[site](https://example.com), long enough to wrap across the editor's width "
                "at least once.\n\n"_s;
        text += u"- An item\n  - A nested item\n- [ ] A to-do @minutes #calls\n\n"_s;
        text +=
            u"```\nint x = %1;\n```\n\n| Name | Value |\n| --- | --- |\n| x | %1 |\n\n"_s.arg(i);
    }
    QFile file(m_root + u"/Long.md"_s);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(text.toUtf8());
    file.close();
    QTRY_VERIFY_WITH_TIMEOUT(m_vault->index()->note(u"Long.md"_s), 3000);
    m_vault->setCurrent(u"Long.md"_s);
    QQuickItem *body = find(u"body"_s);
    QQuickItem *scroll = find(u"scroll"_s);
    body->forceActiveFocus();
    const QString marker = u"x = 75;"_s;
    const int at = int(m_vault->currentText().indexOf(marker) + marker.size());
    body->setProperty("cursorPosition", at);
    QTest::qWait(300);
    const qreal top = scroll->property("contentY").toReal();
    QVERIFY(top > 0);

    // Typed in the middle, keys are taken and drawn without a lag one would feel, and the view
    // holds still. The middle key is timed, so one slow moment on a busy machine passes.
    const QString typed = u" // typed in the middle"_s;
    QSignalSpy scrolled(scroll, SIGNAL(contentYChanged()));
    QList<qint64> times;
    QElapsedTimer timer;
    for (const QChar c : typed) {
        timer.start();
        QTest::keyClick(m_window.get(), c.toLatin1());
        QCoreApplication::processEvents();
        times.append(timer.elapsed());
    }
    std::ranges::sort(times);
    QVERIFY2(times.at(times.size() / 2) < 50,
             qPrintable(u"%1 ms a key"_s.arg(times.at(times.size() / 2))));
    QCOMPARE(scrolled.count(), 0);
    QCOMPARE(scroll->property("contentY").toReal(), top);
    QCOMPARE(body->property("cursorPosition").toInt(), at + typed.size());
    QCOMPARE(m_vault->currentText().mid(at - marker.size(), marker.size() + typed.size()),
             marker + typed);
    QTRY_VERIFY_WITH_TIMEOUT(!m_vault->unsaved(), 2000);
}

void TestNoteEditor::composesWithAnInputMethod()
{
    m_vault->setCurrent(u"Welcome.md"_s);
    QQuickItem *body = find(u"body"_s);
    body->forceActiveFocus();
    body->setProperty("cursorPosition", body->property("length"));
    QTest::keyClick(m_window.get(), Qt::Key_Return);
    for (const char c : {'-', ' '})
        QTest::keyClick(m_window.get(), c);
    const QString before = m_vault->currentText();
    const auto compose = [body](const QString &preedit, const QString &commit = {}) {
        QInputMethodEvent event(preedit, {});
        if (!commit.isEmpty())
            event.setCommitString(commit);
        QCoreApplication::sendEvent(body, &event);
    };

    // While composing, what is shown is not yet the note's.
    compose(u"に"_s);
    compose(u"にほん"_s);
    QVERIFY(body->property("inputMethodComposing").toBool());
    QCOMPARE(body->property("preeditText").toString(), u"にほん"_s);
    QCOMPARE(m_vault->currentText(), before);

    // Committed, it goes in once, and composing again in Hangul adds to it.
    compose({}, u"日本"_s);
    for (const QString &step : {u"ㅎ"_s, u"하"_s, u"한"_s})
        compose(step);
    compose(u"ㄱ"_s, u"한"_s);
    compose(u"그"_s);
    compose(u"글"_s);
    compose({}, u"글"_s);
    QVERIFY(!body->property("inputMethodComposing").toBool());
    QCOMPARE(m_vault->currentText(), before + u"日本한글"_s);
    QCOMPARE(body->property("cursorPosition").toInt(), body->property("length").toInt());

    // The list carries on from the composed item, and the note is saved as typed.
    QTest::keyClick(m_window.get(), Qt::Key_Return);
    QCOMPARE(m_vault->currentText(), before + u"日本한글\n- "_s);
    QTRY_VERIFY_WITH_TIMEOUT(!m_vault->unsaved(), 2000);
    QString error;
    QVERIFY(notefile::read(m_root + u"/Welcome.md"_s, &error)->contains(u"- 日本한글"_s));
}

QTEST_MAIN(TestNoteEditor)
#include "tst_noteeditor.moc"
