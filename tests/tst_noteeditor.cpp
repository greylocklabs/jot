#include "DataStyler.h"
#include "DataTableModel.h"
#include "MarkdownStyler.h"
#include "SettingsForeign.h"
#include "ThemeController.h"
#include "VaultController.h"

#include "jot/Settings.h"

#include <QDir>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QInputMethodEvent>
#include <QLocale>
#include <QPointer>
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
#include <QTextBlock>
#include <QTextDocument>
#include <QTextLayout>
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
    void showsAndResolvesAConflictCopy();
    void goesBackToWhereTheCursorWas();
    void typingIsDrawnAndSaved();
    void typingAfterAMoveIsSaved();
    void typingKeepsWindowsLineEndings();
    void anOutsideEditKeepsTheCursorWhereItWas();
    void showsImagesInPlaceOfTheirLinks();
    void drawsDiagramsInPlaceOfTheirText();
    void switchingAwayFromAPictureLeavesNoStalePlace();
    void formatsAndUndoesInOneStep();
    void formatsFromTheKeyboard();
    void setsAToDosDayAndRepeat();
    void makesTheNoteATaskWithADueDate();
    void continuesAndNestsLists();
    void fillsInTablesWithTheKeys();
    void completesWikilinksFromTheKeyboard();
    void togglesSourceMode();
    void writesInTheChosenFont();
    void editsDataFilesAsPlainText();
    void colorsNumbersAndChecksDataFiles();
    void editsAListOfRecordsAsATable();
    void showsEachListOfAFileAsATab();
    void checksATableAgainstItsSchema();
    void scrollsAWideTableAcross();
    void removingARowOrColumnAsksWithATrashCan();
    void readsTheNoteWithLinksAndBoxesToTick();
    void readsDiagramsAsPictures();
    void tickingARepeatingToDoWritesTheNext();
    void editsInViModes();
    void showsWhereViIsAlongTheFoot();
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
    // What tests delete goes to the vault's own trash, never the desktop's.
    m_vault->setSystemTrash(false);
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

void TestNoteEditor::typingAfterAMoveIsSaved()
{
    m_vault->setCurrent(u"Welcome.md"_s);
    QVERIFY(m_vault->moveNoteTo(u"Welcome.md"_s, u"Inbox"_s));
    QCOMPARE(m_vault->current(), u"Inbox/Welcome.md"_s);
    QQuickItem *body = find(u"body"_s);
    body->forceActiveFocus();
    body->setProperty("cursorPosition", body->property("length"));
    QTest::keyClick(m_window.get(), 'Z');
    QTRY_VERIFY_WITH_TIMEOUT(m_vault->currentText().endsWith(u'Z'), 2000);
    QTRY_VERIFY_WITH_TIMEOUT(!m_vault->unsaved(), 2000);
    QString error;
    QVERIFY(notefile::read(m_root + u"/Inbox/Welcome.md"_s, &error)->endsWith(u'Z'));
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

void TestNoteEditor::showsAndResolvesAConflictCopy()
{
    QString error;
    QVERIFY(notefile::write(m_root + u"/Welcome.md"_s, u"# Hello\n\nkept line\nmine\n"_s, &error));
    QVERIFY(notefile::write(m_root + u"/Welcome (conflict desk 2026-11-03 0930).md"_s,
                            u"# Hello\n\nkept line\ntheirs\nand more\n"_s, &error));
    m_vault->setCurrent(u"Welcome.md"_s);
    QQuickItem *banner = find(u"conflictBanner"_s);
    QTRY_VERIFY(banner->isVisible());

    QQuickItem *compare = find(u"compareConflict"_s);
    QMetaObject::invokeMethod(compare, "clicked");
    auto *lines = m_editor->findChild<QQuickItem *>(u"conflictLines"_s);
    QVERIFY(lines);
    QTRY_VERIFY(lines->isVisible());
    QTRY_COMPARE(lines->property("count").toInt(), 5);
    auto *keep = m_editor->findChild<QQuickItem *>(u"keepThisVersion"_s);
    QMetaObject::invokeMethod(keep, "clicked");
    QTRY_VERIFY(!lines->isVisible());
    QTRY_VERIFY(!banner->isVisible());
    QVERIFY(!QFileInfo::exists(m_root + u"/Welcome (conflict desk 2026-11-03 0930).md"_s));
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

void TestNoteEditor::drawsDiagramsInPlaceOfTheirText()
{
    m_vault->setCurrent(u"Welcome.md"_s);
    QQuickItem *body = find(u"body"_s);
    auto *styler = m_editor->findChild<QObject *>(u"styler"_s);
    QVERIFY(styler);
    const auto diagrams = [styler] {
        QVariantList found;
        for (const QVariant &each : styler->property("images").toList()) {
            if (each.toMap().value(u"source"_s).toUrl().path().endsWith(u".svg"_s))
                found.append(each);
        }
        return found;
    };
    const QString text =
        u"# Plan\n\n```mermaid\nflowchart LR\n  A[Write] --> B[Read]\n```\n\nafter\n"_s;
    body->forceActiveFocus();
    body->setProperty("text", text);
    body->setProperty("cursorPosition", int(text.size()) - 2);
    QTRY_COMPARE(diagrams().size(), 1);
    const QVariantMap shown = diagrams().first().toMap();
    QCOMPARE(shown.value(u"position"_s).toInt(), int(text.indexOf(u"```"_s)));
    QVERIFY(shown.value(u"height"_s).toInt() > 20);

    // In its fence it is the text again, to edit.
    body->setProperty("cursorPosition", int(text.indexOf(u"Write"_s)));
    QTRY_COMPARE(diagrams().size(), 0);
    body->setProperty("cursorPosition", int(text.size()) - 2);
    QTRY_COMPARE(diagrams().size(), 1);

    // A diagram with a mistake in it stays as its text.
    QString wrong = text;
    wrong.replace(u"A[Write]"_s, u"A[Write"_s);
    body->setProperty("text", wrong);
    body->setProperty("cursorPosition", int(wrong.size()) - 2);
    QTRY_COMPARE(diagrams().size(), 0);
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

void TestNoteEditor::setsAToDosDayAndRepeat()
{
    m_vault->setCurrent(u"Welcome.md"_s);
    QQuickItem *body = find(u"body"_s);
    body->forceActiveFocus();
    body->setProperty("text", u"# Chores\n- [ ] water the plants every week\nsweep\n"_s);
    const auto text = [body] { return body->property("text").toString(); };
    const QString today = QDate::currentDate().toString(Qt::ISODate);
    const auto schedule = [this] {
        QTest::keyClick(m_window.get(), Qt::Key_D, Qt::ControlModifier | Qt::ShiftModifier);
    };

    // Words that say when become the markers, in one step to undo.
    body->setProperty("cursorPosition", 12);
    schedule();
    QCOMPARE(text(), u"# Chores\n- [ ] water the plants 🔁 every week 📅 %1\nsweep\n"_s.arg(today));
    QMetaObject::invokeMethod(body, "undo");
    QCOMPARE(text(), u"# Chores\n- [ ] water the plants every week\nsweep\n"_s);

    // A line that is no to-do becomes one, and with nothing said, the popup asks when.
    const qsizetype sweep = text().indexOf(u"sweep"_s);
    body->setProperty("cursorPosition", int(sweep) + 2);
    schedule();
    QVERIFY(text().contains(u"- [ ] sweep\n"_s));
    auto *popup = m_editor->findChild<QObject *>(u"scheduleInput"_s);
    QVERIFY(popup);
    QTRY_VERIFY(popup->property("activeFocus").toBool());
    for (const QChar c : u"2026-12-24 every year"_s)
        QTest::keyClick(m_window.get(), c.toLatin1());
    QTest::keyClick(m_window.get(), Qt::Key_Return);
    QTRY_VERIFY(text().contains(u"- [ ] sweep 🔁 every year 📅 2026-12-24\n"_s));

    // Asked again, it offers what is there; emptied, it takes the day and repeat away.
    body->setProperty("cursorPosition", int(text().indexOf(u"sweep"_s)) + 2);
    schedule();
    QTRY_VERIFY(popup->property("activeFocus").toBool());
    QCOMPARE(popup->property("text").toString(), u"2026-12-24 every year"_s);
    QTest::keyClick(m_window.get(), Qt::Key_Backspace);
    QTest::keyClick(m_window.get(), Qt::Key_Return);
    QTRY_VERIFY(text().contains(u"- [ ] sweep\n"_s));

    // A to-do of nothing but its day is cleared too.
    body->setProperty("text", u"- [ ] 📅 2026-12-24"_s);
    body->setProperty("cursorPosition", 3);
    schedule();
    QTRY_VERIFY(popup->property("activeFocus").toBool());
    QTest::keyClick(m_window.get(), Qt::Key_A, Qt::ControlModifier);
    QTest::keyClick(m_window.get(), Qt::Key_Backspace);
    QTest::keyClick(m_window.get(), Qt::Key_Return);
    QTRY_COMPARE(text(), u"- [ ] "_s);
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

void TestNoteEditor::readsDiagramsAsPictures()
{
    QString error;
    QVERIFY(notefile::write(m_root + u"/Diagrams.md"_s,
                            u"# Plan\n\n```mermaid\nflowchart LR\n  A[Write] --> B[Read]\n```\n\n"
                            "```mermaid\nflowchart LR\n  C[Broken --> D\n```\n"_s,
                            &error));
    QTRY_VERIFY_WITH_TIMEOUT(m_vault->index()->note(u"Diagrams.md"_s), 3000);
    m_vault->setCurrent(u"Diagrams.md"_s);
    m_settings->setReadMode(true);
    QQuickItem *reader = find(u"reader"_s);
    QTRY_VERIFY(reader->isVisible());
    auto *doc = reader->property("textDocument").value<QQuickTextDocument *>()->textDocument();
    QTRY_VERIFY(doc->toPlainText().contains(u"Plan"_s));

    QList<QTextImageFormat> pictures;
    for (QTextBlock block = doc->begin(); block.isValid(); block = block.next()) {
        for (auto it = block.begin(); !it.atEnd(); ++it) {
            if (it.fragment().charFormat().isImageFormat())
                pictures.append(it.fragment().charFormat().toImageFormat());
        }
    }
    QCOMPARE(pictures.size(), 1);
    QVERIFY(pictures.first().name().endsWith(u".svg"_s));
    QVERIFY(pictures.first().width() > 20);
    const QString shown = doc->toPlainText();
    QVERIFY2(!shown.contains(u"A[Write]"_s), qPrintable(shown));
    // One with a mistake in it is shown as its code.
    QVERIFY2(shown.contains(u"C[Broken --> D"_s), qPrintable(shown));

    // A picture beside the note is found from its relative link, as before.
    m_vault->setCurrent(u"Ashfall/Places/The Ferry.md"_s);
    QTRY_VERIFY([doc] {
        for (QTextBlock block = doc->begin(); block.isValid(); block = block.next()) {
            for (auto it = block.begin(); !it.atEnd(); ++it) {
                const QTextCharFormat format = it.fragment().charFormat();
                if (format.isImageFormat() && format.toImageFormat().width() > 20)
                    return true;
            }
        }
        return false;
    }());
    m_settings->setReadMode(false);
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

void TestNoteEditor::editsDataFilesAsPlainText()
{
    QFile file(m_root + u"/cast.json"_s);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("[{\"name\": \"**Mara**\"}]\n");
    file.close();
    m_settings->setReadMode(true);
    m_vault->setCurrent(u"cast.json"_s);
    // A list of records opens as a table; this is about its text.
    m_editor->setProperty("showText", true);
    // Switching fonts on the way in sends nothing of the last note into this one.
    QCOMPARE(m_vault->currentText(), u"[{\"name\": \"**Mara**\"}]\n"_s);
    QVERIFY(!m_vault->unsaved());
    // Its text as it is, in the mono font, even in read mode, with no Markdown bar or styling.
    QQuickItem *body = find(u"body"_s);
    QVERIFY(body->isVisible());
    QVERIFY(!find(u"reader"_s)->isVisible());
    QVERIFY(!find(u"formatBar"_s)->isVisible());
    QCOMPARE(body->property("text").toString(), u"[{\"name\": \"**Mara**\"}]\n"_s);
    QCOMPARE(body->property("font").value<QFont>().family(), u"monospace"_s);
    QVERIFY(!body->findChild<MarkdownStyler *>()->document());

    // A note after it is styled again.
    m_settings->setReadMode(false);
    m_vault->setCurrent(u"Welcome.md"_s);
    QVERIFY(body->findChild<MarkdownStyler *>()->document());
    QVERIFY(find(u"formatBar"_s)->isVisible());
}

void TestNoteEditor::colorsNumbersAndChecksDataFiles()
{
    QFile file(m_root + u"/cast.json"_s);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("[\n  {\"name\": \"Mara [the ferrywoman]\"},\n  {\"name\": 3}\n]\n");
    file.close();
    m_vault->setCurrent(u"cast.json"_s);
    m_editor->setProperty("showText", true);
    QQuickItem *body = find(u"body"_s);
    auto *styler = body->findChild<DataStyler *>();
    QVERIFY(styler);

    // Its words are colored by their kind.
    QTextDocument *document = styler->document()->textDocument();
    const QTextBlock second = document->findBlockByNumber(1);
    QVERIFY(std::ranges::any_of(second.layout()->formats(), [](const QTextLayout::FormatRange &f) {
        return f.format.foreground().style() != Qt::NoBrush;
    }));

    // A number for each line in view, and none for lines that are not there.
    QCOMPARE(styler->lineTops(0, 100000).size(), 5);
    QVERIFY(find(u"gutter"_s)->isVisible());

    // The brackets around a record pair up, while one inside a string is passed over.
    const QString text = body->property("text").toString();
    const int open = int(text.indexOf(u'{'));
    QCOMPARE(styler->brackets(open), (QVariantList{open, int(text.indexOf(u'}'))}));
    QCOMPARE(styler->brackets(0), (QVariantList{0, int(text.lastIndexOf(u']'))}));
    QVERIFY(styler->brackets(int(text.indexOf(u"[the"_s))).isEmpty());

    // Breaking it says where, and clicking there takes the cursor to it.
    QVERIFY(!find(u"dataProblem"_s)->isVisible());
    body->setProperty("text", u"[\n  {\"name\": }\n]\n"_s);
    styler->checkNow();
    QQuickItem *problem = find(u"dataProblem"_s);
    QVERIFY(problem->isVisible());
    QVERIFY2(problem->property("text").toString().startsWith(u"Line 2"_s),
             qPrintable(problem->property("text").toString()));
    QTRY_VERIFY(problem->height() > 0);
    find(u"scroll"_s)->setProperty("contentY", 0);
    QPointF placed(-1, -1);
    QTRY_VERIFY([&] {
        const QPointF now = problem->mapToScene(QPointF(0, 0));
        const bool settled = now == placed;
        placed = now;
        return settled;
    }());
    const QPointF middle = problem->mapToScene(QPointF(20, problem->height() / 2));
    QTest::mouseClick(m_window.get(), Qt::LeftButton, {}, middle.toPoint());
    QTRY_COMPARE(body->property("cursorPosition").toInt(), styler->problemPosition());
    QVERIFY(styler->problemPosition() > 0);
}

void TestNoteEditor::showsEachListOfAFileAsATab()
{
    const QByteArray text = "# The ferry\ntitle: Ashfall\ncast:\n  - name: Mara\n    age: 31\n"
                            "crew:\n  - name: Ox\n    job: cook\n";
    QFile file(m_root + u"/ensemble.yaml"_s);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(text);
    file.close();
    m_vault->setCurrent(u"ensemble.yaml"_s);

    // Several lists open as a table too, a tab for each, the first one shown.
    QQuickItem *view = find(u"tableView"_s);
    QTRY_VERIFY(view->isVisible());
    auto *records = view->property("records").value<DataTableModel *>();
    QCOMPARE(records->partNames(), (QStringList{u"cast"_s, u"crew"_s}));
    QVERIFY(view->findChild<QQuickItem *>(u"partTabs"_s)->isVisible());
    QCOMPARE(records->columns(), (QStringList{u"name"_s, u"age"_s}));

    // Another tab is another list; an edit there rewrites its part alone, and the tab stays.
    records->setPart(1);
    QCOMPARE(records->columns(), (QStringList{u"name"_s, u"job"_s}));
    QVERIFY(records->setCell(0, 1, u"captain"_s));
    QTRY_COMPARE(m_vault->currentText(),
                 QString::fromUtf8(text).replace(u"job: cook"_s, u"job: captain"_s));
    QCOMPARE(records->part(), 1);
    records->undo();
    QCOMPARE(records->part(), 1);
    QTRY_COMPARE(m_vault->currentText(), QString::fromUtf8(text));

    // A schema describes the lists under their keys, and marks the tab's own cells.
    QFile schema(m_root + u"/ensemble.schema.json"_s);
    QVERIFY(schema.open(QIODevice::WriteOnly));
    schema.write(R"({"properties": {"crew": {"items": {"properties": {
        "job": {"enum": ["cook", "pilot"]}}}}}})");
    schema.close();
    QVERIFY(records->setCell(0, 1, u"captain"_s));
    QCOMPARE(records->schemaFile(), m_root + u"/ensemble.schema.json"_s);
    QCOMPARE(records->data(records->index(0, 1), DataTableModel::ProblemRole).toString(),
             u"should be one of \"cook\", \"pilot\""_s);
    QCOMPARE(records->choices(1), (QStringList{u"cook"_s, u"pilot"_s}));
}

void TestNoteEditor::editsAListOfRecordsAsATable()
{
    QFile file(m_root + u"/cast.json"_s);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(
        "[\n  {\"name\": \"Mara\", \"age\": 31},\n  {\"name\": \"Soren\", \"age\": 40}\n]\n");
    file.close();
    m_vault->setCurrent(u"cast.json"_s);

    // A list of records opens as a table, in place of its text.
    QQuickItem *view = find(u"tableView"_s);
    QVERIFY(view->isVisible());
    QVERIFY(!find(u"body"_s)->isVisible());
    auto *records = view->property("records").value<DataTableModel *>();
    QVERIFY(records);
    QCOMPARE(records->rowCount(), 2);
    QCOMPARE(records->columns(), (QStringList{u"name"_s, u"age"_s}));

    // Typing over a cell keeps its type, and the file changes there alone.
    auto *table = view->findChild<QQuickItem *>(u"table"_s);
    QVERIFY(table);
    table->forceActiveFocus();
    view->setProperty("row", 1);
    view->setProperty("column", 1);
    QTest::keyClick(m_window.get(), '4');
    QTest::keyClick(m_window.get(), '1');
    QTRY_VERIFY(view->findChild<QQuickItem *>(u"cellEditor"_s)->isVisible());
    QTest::keyClick(m_window.get(), Qt::Key_Return);
    QTRY_COMPARE(
        m_vault->currentText(),
        u"[\n  {\"name\": \"Mara\", \"age\": 31},\n  {\"name\": \"Soren\", \"age\": 41}\n]\n"_s);
    QTRY_VERIFY_WITH_TIMEOUT(!m_vault->unsaved(), 2000);

    // Sorting shows the rows in another order without touching the file.
    records->sortBy(0);
    records->sortBy(0);
    QCOMPARE(records->data(records->index(0, 0)).toString(), u"Soren"_s);
    QVERIFY(m_vault->currentText().indexOf(u"Mara"_s) < m_vault->currentText().indexOf(u"Soren"_s));

    // A row added under a sort is picked where the sort shows it, and stays empty until typed in.
    const int added = records->addRowAfter(0);
    QVERIFY(added >= 0);
    QCOMPARE(records->data(records->index(added, 0), DataTableModel::TypeRole).toString(),
             u"missing"_s);
    QCOMPARE(records->rowCount(), 3);
    records->undo();
    QTRY_COMPARE(records->rowCount(), 2);

    // The keys redo and undo too, with the table in focus.
    find(u"table"_s)->forceActiveFocus();
    QTest::keyClick(m_window.get(), Qt::Key_Z, Qt::ControlModifier | Qt::ShiftModifier);
    QTRY_COMPARE(records->rowCount(), 3);
    QTest::keyClick(m_window.get(), Qt::Key_Z, Qt::ControlModifier);
    QTRY_COMPARE(records->rowCount(), 2);
    QTest::keyClick(m_window.get(), Qt::Key_Y, Qt::ControlModifier);
    QTRY_COMPARE(records->rowCount(), 3);
    QTest::keyClick(m_window.get(), Qt::Key_Z, Qt::ControlModifier);
    QTRY_COMPARE(records->rowCount(), 2);

    // Undo puts the file back; the text view shows what the table wrote.
    records->undo();
    QTRY_VERIFY(m_vault->currentText().contains(u"\"age\": 40"_s));
    find(u"viewSwitch"_s)->setProperty("checked", false);
    QMetaObject::invokeMethod(find(u"viewSwitch"_s), "clicked");
    QVERIFY(find(u"body"_s)->isVisible());
    QVERIFY(find(u"body"_s)->property("text").toString().contains(u"\"age\": 40"_s));

    // A YAML list of records opens as a table too.
    QFile yaml(m_root + u"/cast.yaml"_s);
    QVERIFY(yaml.open(QIODevice::WriteOnly));
    yaml.write("# cast\n- name: Mara\n  age: 31\n");
    yaml.close();
    m_vault->setCurrent(u"cast.yaml"_s);
    QVERIFY(find(u"tableView"_s)->isVisible());
    QCOMPARE(records->rowCount(), 1);
    QVERIFY(records->setCell(0, 1, u"32"_s));
    QTRY_COMPARE(m_vault->currentText(), u"# cast\n- name: Mara\n  age: 32\n"_s);
    m_vault->setCurrent(u"cast.json"_s);
    QMetaObject::invokeMethod(find(u"viewSwitch"_s), "clicked");

    // Text that is no list of records cannot be asked to be a table, and typing one in
    // leaves the text showing until a table is asked for.
    QQuickItem *body = find(u"body"_s);
    body->setProperty("text", u"{\"not\": \"records\"}"_s);
    QTRY_VERIFY(!find(u"viewSwitch"_s)->isEnabled());
    body->setProperty("text", u"[{\"name\": \"Mo\"}]"_s);
    QTRY_VERIFY(find(u"viewSwitch"_s)->isEnabled());
    QVERIFY(body->isVisible());
    QMetaObject::invokeMethod(find(u"viewSwitch"_s), "clicked");
    QVERIFY(find(u"tableView"_s)->isVisible());

    // A file that is no table opens as its text, with no table to ask for.
    QFile schema(m_root + u"/cast.schema.json"_s);
    QVERIFY(schema.open(QIODevice::WriteOnly));
    schema.write("{\"type\": \"array\"}\n");
    schema.close();
    QTRY_VERIFY_WITH_TIMEOUT(m_vault->index()->note(u"cast.schema.json"_s), 3000);
    m_vault->setCurrent(u"cast.schema.json"_s);
    QVERIFY(body->isVisible());
    QVERIFY(!find(u"viewSwitch"_s)->isEnabled());
}

void TestNoteEditor::removingARowOrColumnAsksWithATrashCan()
{
    QFile file(m_root + u"/pair.json"_s);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("[\n  {\"name\": \"Mara\", \"age\": 31, \"town\": \"Vey\"},\n"
               "  {\"name\": \"Soren\", \"age\": 40, \"town\": \"Ost\"}\n]\n");
    file.close();
    QTRY_VERIFY_WITH_TIMEOUT(m_vault->index()->note(u"pair.json"_s), 3000);
    m_vault->setCurrent(u"pair.json"_s);
    QQuickItem *view = find(u"tableView"_s);
    QTRY_VERIFY(view->isVisible());
    auto *records = view->property("records").value<DataTableModel *>();
    QQuickItem *rows = find(u"rowHeader"_s);
    // Views hang their delegates on the item tree, not the object tree.
    const std::function<QList<QQuickItem *>(QQuickItem *, const QString &)> named =
        [&named](QQuickItem *from, const QString &name) {
            QList<QQuickItem *> found;
            for (QQuickItem *child : from->childItems()) {
                if (child->objectName() == name)
                    found << child;
                found << named(child, name);
            }
            return found;
        };
    // The button in a header's first cell as it is laid out now. Views remake their delegates
    // after a change, so it is looked up again for every use.
    const auto button = [&named](QQuickItem *header, const QString &name) {
        QQuickItem *cell = nullptr;
        QMetaObject::invokeMethod(header, "itemAtCell", qReturnArg(cell), QPoint(0, 0));
        const QList<QQuickItem *> found = cell ? named(cell, name) : QList<QQuickItem *>();
        return QPointer<QQuickItem>(!found.isEmpty() && found.first()->isVisible() ? found.first()
                                                                                   : nullptr);
    };
    const auto said = [&button](QQuickItem *header, const QString &name, const char *property) {
        const QPointer<QQuickItem> item = button(header, name);
        return item ? item->property(property) : QVariant();
    };
    // Points at the button first, as a mouse would, so it knows it is hovered, then clicks.
    const auto clickOn = [this, &button](QQuickItem *header, const QString &name,
                                         bool twice = false) {
        QPointer<QQuickItem> item;
        QTRY_VERIFY((item = button(header, name)));
        const QPoint at =
            item->mapToScene(QPointF(item->width() / 2, item->height() / 2)).toPoint();
        QTest::mouseMove(m_window.get(), at);
        QTRY_VERIFY((item = button(header, name)) && item->property("hovered").toBool());
        if (twice)
            QTest::mouseDClick(m_window.get(), Qt::LeftButton, {}, at);
        else
            QTest::mouseClick(m_window.get(), Qt::LeftButton, {}, at);
    };
    const QString removeRow = u"removeRow"_s;
    const QString removeColumn = u"removeColumn"_s;

    // Over the first row, its X; a click turns it into a trash can and takes nothing yet.
    QTRY_VERIFY(!named(m_window->contentItem(), removeRow).isEmpty());
    const QPoint first = rows->mapToScene(QPointF(rows->width() / 2, 10)).toPoint();
    QTest::mouseMove(m_window.get(), first + QPoint(0, 300));
    QTest::mouseMove(m_window.get(), first);
    QTRY_COMPARE(said(rows, removeRow, "glyph").toString(), u"close"_s);
    clickOn(rows, removeRow);
    QTRY_COMPARE(said(rows, removeRow, "glyph").toString(), u"trash"_s);
    QCOMPARE(records->rowCount(), 2);
    // Moving away puts the X back.
    QTest::mouseMove(m_window.get(), first + QPoint(400, 200));
    QTRY_VERIFY(!button(rows, removeRow));
    QTest::mouseMove(m_window.get(), first);
    QTRY_COMPARE(said(rows, removeRow, "armed").toBool(), false);
    // A second click on the can takes the row.
    clickOn(rows, removeRow);
    QTRY_COMPARE(said(rows, removeRow, "armed").toBool(), true);
    clickOn(rows, removeRow);
    QTRY_COMPARE(records->rowCount(), 1);
    QTRY_VERIFY(m_vault->currentText().contains(u"Soren"_s) &&
                !m_vault->currentText().contains(u"Mara"_s));

    // A column asks the same way.
    QQuickItem *header = find(u"tableHeader"_s);
    QVERIFY(header);
    const QPoint heading = header->mapToScene(QPointF(30, header->height() / 2)).toPoint();
    QTest::mouseMove(m_window.get(), heading);
    clickOn(header, removeColumn);
    QTRY_COMPARE(said(header, removeColumn, "armed").toBool(), true);
    QCOMPARE(records->columns().size(), 3);
    // The click is the button's alone: the heading under it neither sorts nor renames.
    QCOMPARE(records->sortColumn(), -1);
    clickOn(header, removeColumn);
    QTRY_COMPARE(records->columns().size(), 2);

    // Two quick clicks, which come as a double click, take it too.
    QTest::mouseMove(m_window.get(), heading + QPoint(0, 300));
    QTest::mouseMove(m_window.get(), heading);
    clickOn(header, removeColumn, true);
    QTRY_COMPARE(records->columns().size(), 1);
    QCOMPARE(records->sortColumn(), -1);

    // Beside the X, a double click on the heading renames it without sorting the table, so the
    // field stays where it opened.
    const auto renaming = [header] {
        QQuickItem *cell = nullptr;
        QMetaObject::invokeMethod(header, "itemAtCell", qReturnArg(cell), QPoint(0, 0));
        return cell && cell->property("renaming").toBool();
    };
    // The table reloads once the removals are saved, remaking its headings; that is let pass.
    QSignalSpy reloaded(records, &QAbstractItemModel::modelReset);
    for (int quiet = 0; quiet < 20; ++quiet) {
        reloaded.clear();
        QTest::qWait(300);
        if (reloaded.isEmpty())
            break;
    }
    QTest::mouseMove(m_window.get(), heading);
    QTest::mouseDClick(m_window.get(), Qt::LeftButton, {}, heading);
    QTRY_VERIFY(renaming());
    QTest::qWait(QGuiApplication::styleHints()->mouseDoubleClickInterval() + 100);
    QVERIFY(renaming());
    QCOMPARE(records->sortColumn(), -1);
    QTest::keyClick(m_window.get(), Qt::Key_Escape);
    QTRY_VERIFY(!renaming());

    // And a single click sorts.
    QTest::mouseClick(m_window.get(), Qt::LeftButton, {}, heading);
    QTRY_COMPARE(records->sortColumn(), 0);
}

void TestNoteEditor::scrollsAWideTableAcross()
{
    QFile file(m_root + u"/wide.json"_s);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QStringList fields;
    for (int i = 0; i < 16; ++i)
        fields << u"\"field%1\": %1"_s.arg(i);
    file.write((u"[{"_s + fields.join(u", "_s) + u"}]\n"_s).toUtf8());
    file.close();
    QTRY_VERIFY_WITH_TIMEOUT(m_vault->index()->note(u"wide.json"_s), 3000);
    m_vault->setCurrent(u"wide.json"_s);
    QQuickItem *table = find(u"table"_s);
    QTRY_VERIFY(table->isVisible() && table->property("contentWidth").toReal() > table->width());
    QCOMPARE(table->property("contentX").toReal(), 0.0);

    // The wheel with Shift held scrolls across, over the cells or either header.
    for (const QString &name : {u"table"_s, u"tableHeader"_s, u"rowHeader"_s}) {
        table->setProperty("contentX", 0.0);
        QQuickItem *over = find(name);
        const QPointF at = over->mapToScene(QPointF(std::min(over->width() / 2, 40.0), 10));
        QWheelEvent wheel(at, m_window->mapToGlobal(at), QPoint(), QPoint(0, -120), Qt::NoButton,
                          Qt::ShiftModifier, Qt::NoScrollPhase, false);
        QGuiApplication::sendEvent(m_window.get(), &wheel);
        QTRY_VERIFY2(table->property("contentX").toReal() > 0, qPrintable(name));
    }
    // And the bar for it shows all along, not only while scrolling.
    QQuickItem *across = nullptr;
    for (QQuickItem *child : table->findChildren<QQuickItem *>()) {
        if (QByteArray(child->metaObject()->className()).contains("ScrollBar") &&
            child->property("orientation").toInt() == Qt::Horizontal)
            across = child;
    }
    QVERIFY(across);
    QCOMPARE(across->property("policy").toInt(), 2); // ScrollBar.AlwaysOn
    // Right under the one row, not down at the bottom of the window.
    const qreal rowBottom =
        table->mapToScene(QPointF(0, table->property("contentHeight").toReal())).y();
    const qreal barTop = across->mapToScene(QPointF(0, 0)).y();
    QVERIFY2(barTop >= rowBottom - 1 && barTop - rowBottom < 4,
             qPrintable(u"row ends at %1, bar starts at %2"_s.arg(rowBottom).arg(barTop)));
    QVERIFY(table->mapToScene(QPointF(0, table->height())).y() < m_window->height() / 2.0);
}

void TestNoteEditor::checksATableAgainstItsSchema()
{
    const auto write = [this](const QString &name, const QByteArray &text) {
        QFile file(m_root + u'/' + name);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(text);
    };
    write(u"cast.schema.json"_s,
          "{\"type\": \"array\", \"items\": {\"properties\": {\"role\": {\"enum\": [\"lead\", "
          "\"chorus\"]}, \"age\": {\"type\": \"integer\"}}}}");
    write(u"cast.json"_s, "[{\"name\": \"Mara\", \"role\": \"extra\", \"age\": 31}]\n");
    m_vault->setCurrent(u"cast.json"_s);
    auto *records = find(u"tableView"_s)->property("records").value<DataTableModel *>();
    QVERIFY(records);

    // The value that breaks it is marked with why, and the line says so.
    QCOMPARE(records->schemaFile(), m_root + u"/cast.schema.json"_s);
    QCOMPARE(records->schemaProblems().size(), 1);
    QCOMPARE(records->data(records->index(0, 1), DataTableModel::ProblemRole).toString(),
             u"should be one of \"lead\", \"chorus\""_s);
    QVERIFY(records->data(records->index(0, 0), DataTableModel::ProblemRole).toString().isEmpty());
    QVERIFY(find(u"schemaLine"_s)->isVisible());
    // Only that cell is drawn as wrong.
    QQuickItem *table = find(u"table"_s);
    const auto cellColor = [table](int column) {
        QQuickItem *item = nullptr;
        QMetaObject::invokeMethod(table, "itemAtCell", Q_RETURN_ARG(QQuickItem *, item),
                                  Q_ARG(QPoint, QPoint(column, 0)));
        return item ? item->property("color").value<QColor>() : QColor();
    };
    QTRY_VERIFY(cellColor(0).isValid() && cellColor(1).isValid());
    QVERIFY(cellColor(0) != cellColor(1));
    // The schema's values are offered for the column, and taking one clears the mark.
    QCOMPARE(records->choices(1), (QStringList{u"lead"_s, u"chorus"_s}));
    QVERIFY(records->choices(0).isEmpty());
    QVERIFY(records->setCell(0, 1, u"lead"_s));
    QVERIFY(records->schemaProblems().isEmpty());
    QVERIFY(!find(u"schemaLine"_s)->isVisible());
    // A schema that does not read says why, rather than counting as a value that breaks it.
    write(u"cast.schema.json"_s, "{\"type\": ");
    const QString path = records->path();
    records->setPath(QString());
    records->setPath(path);
    QVERIFY(records->schemaProblems().isEmpty());
    QVERIFY2(records->schemaSummary().startsWith(u"cast.schema.json is not JSON"_s),
             qPrintable(records->schemaSummary()));
    QTRY_VERIFY(find(u"schemaLine"_s)->isVisible());
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

void TestNoteEditor::showsWhereViIsAlongTheFoot()
{
    QQuickItem *line = find(u"viStatusLine"_s);
    QVERIFY(line);
    QVERIFY(!line->isVisible());
    m_settings->setViMode(true);
    m_vault->setCurrent(u"Ashfall/Characters/Mara Vey.md"_s);
    QTRY_VERIFY(line->isVisible());
    QQuickItem *body = find(u"body"_s);
    body->forceActiveFocus();
    body->setProperty("text", u"alpha\nbeta gamma"_s);
    body->setProperty("cursorPosition", 8);
    // What a part of the line says, all its words together.
    const std::function<QString(QQuickItem *)> words = [&words](QQuickItem *item) {
        if (item->inherits("QQuickText"))
            return item->isVisible() ? item->property("text").toString() : QString();
        QStringList found;
        for (QQuickItem *child : item->childItems()) {
            if (!child->isVisible())
                continue;
            if (child->inherits("QQuickText"))
                found << child->property("text").toString();
            else if (const QString inner = words(child); !inner.isEmpty())
                found << inner;
        }
        return found.join(u' ');
    };
    const auto part = [line](const char *name) {
        return line->findChild<QQuickItem *>(QString::fromLatin1(name));
    };

    QCOMPARE(words(part("viMode")), u"NORMAL"_s);
    QCOMPARE(words(part("viFile")), u"Ashfall / Characters / Mara Vey.md"_s);
    QCOMPARE(words(part("viCursor")), u"2:3"_s);
    body->setProperty("text", u"\nalpha\nbeta gamma"_s);
    body->setProperty("cursorPosition", 0);
    QCOMPARE(words(part("viCursor")), u"1:1"_s);
    body->setProperty("text", u"alpha\nbeta gamma"_s);
    body->setProperty("cursorPosition", 8);
    QCOMPARE(words(part("viPlace")), u"Top"_s);
    // The day and time as the locale writes them.
    const auto clock = [] {
        const QDateTime now = QDateTime::currentDateTime();
        const QLocale locale;
        return locale.dayName(now.date().dayOfWeek(), QLocale::ShortFormat) + u' ' +
               locale.toString(now.time(), QLocale::ShortFormat);
    };
    QTRY_COMPARE(words(part("viClock")), clock());
    const QColor normal = part("viMode")->property("fill").value<QColor>();

    // Each mode in its own color, the clock's too.
    QTest::keyClick(m_window.get(), 'i');
    QCOMPARE(words(part("viMode")), u"INSERT"_s);
    const QColor insert = part("viMode")->property("fill").value<QColor>();
    QVERIFY(insert != normal);
    QCOMPARE(part("viClock")->property("fill").value<QColor>(), insert);
    QTest::keyClick(m_window.get(), Qt::Key_Escape);

    // Visual counts the lines it picks.
    body->setProperty("cursorPosition", 0);
    QTest::keyClick(m_window.get(), 'v');
    QTest::keyClick(m_window.get(), 'j');
    QCOMPARE(words(part("viMode")), u"VISUAL"_s);
    QVERIFY(part("viMode")->property("fill").value<QColor>() != normal);
    QTRY_COMPARE(words(part("viPicked")), u"2 lines"_s);
    QTest::keyClick(m_window.get(), Qt::Key_Escape);

    m_settings->setViMode(false);
    QTRY_VERIFY(!line->isVisible());
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
