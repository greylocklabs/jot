#include "MarkdownStyler.h"
#include "SettingsForeign.h"
#include "ThemeController.h"
#include "VaultController.h"

#include "jot/Settings.h"

#include <QDir>
#include <QDirIterator>
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
    void editsInViModes();

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

    // A click left of the task's text ticks it in the file, once the view has settled where
    // the editor was.
    QPointF placed(-1, -1);
    QTRY_VERIFY([&] {
        const QPointF now = reader->mapToScene(QPointF(0, 0));
        const bool settled = now == placed;
        placed = now;
        return settled;
    }());
    QRectF line;
    QMetaObject::invokeMethod(reader, "positionToRectangle", Q_RETURN_ARG(QRectF, line),
                              Q_ARG(int, int(shown.indexOf(u"Buy rope"_s))));
    QTest::mouseClick(m_window.get(), Qt::LeftButton, {},
                      reader->mapToScene(QPointF(line.x() - 6, line.center().y())).toPoint());
    QTRY_VERIFY_WITH_TIMEOUT(
        notefile::read(m_root + u"/Reading.md"_s, &error)->contains(u"- [x] Buy rope"_s), 3000);

    // Its links lead to the note, and to the tag's notes.
    QMetaObject::invokeMethod(reader, "linkActivated", Q_ARG(QString, u"jot-tag:ferry"_s));
    QCOMPARE(m_vault->notes()->tag(), u"ferry"_s);
    QMetaObject::invokeMethod(reader, "linkActivated", Q_ARG(QString, u"jot-note:Mara%20Vey"_s));
    QCOMPARE(m_vault->current(), u"Ashfall/Characters/Mara Vey.md"_s);
    // Writing again leaves reading.
    m_settings->setReadMode(false);
    QTRY_VERIFY(find(u"body"_s)->isVisible());
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

QTEST_MAIN(TestNoteEditor)
#include "tst_noteeditor.moc"
