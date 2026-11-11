#include "jot/MarkdownEdit.h"

#include <QTest>

using namespace jot;
using namespace jot::markdownedit;
using namespace Qt::StringLiterals;

/// Formatting as text: `|` marks the cursor, or `[` and `]` the selection, before and after.
class TestMarkdownEdit : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void wrapsAndUnwraps_data();
    void wrapsAndUnwraps();
    void stylesLines_data();
    void stylesLines();
    void makesLinks_data();
    void makesLinks();
    void fencesCode_data();
    void fencesCode();
    void continuesLists_data();
    void continuesLists();
    void indentsLists_data();
    void indentsLists();
    void linesUpTables_data();
    void linesUpTables();
    void linesUpASelectionAcrossRows();
    void movesThroughCells();
    void addsAndEndsRows();
    void insertsTables_data();
    void insertsTables();
    void findsTheWikilinkBeingTyped_data();
    void findsTheWikilinkBeingTyped();
    void completesWikilinks_data();
    void findsTheEmojiBeingTyped_data();
    void findsTheEmojiBeingTyped();
    void findsTheMarkerBeingTyped();
    void completesWikilinks();
    void setsFrontMatterKeepingTheSelection();
    void editsAreAsSmallAsTheyCanBe();
};

namespace {

struct Marked
{
    QString text;
    qsizetype from;
    qsizetype to;
};

Marked unmark(QString text)
{
    if (const qsizetype cursor = text.indexOf(u'|'); cursor >= 0) {
        text.remove(cursor, 1);
        return {text, cursor, cursor};
    }
    const qsizetype from = text.indexOf(u'[');
    text.remove(from, 1);
    const qsizetype to = text.indexOf(u']', from);
    text.remove(to, 1);
    return {text, from, to};
}

QString applied(const QString &text, const Edit &edit)
{
    QString out = text;
    out.replace(edit.start, edit.end - edit.start, edit.text);
    if (edit.selectionStart == edit.selectionEnd)
        return out.insert(edit.selectionStart, u'|');
    return out.insert(edit.selectionEnd, u']').insert(edit.selectionStart, u'[');
}

} // namespace

// Brackets would read as selection marks, so links are tested with their own markers.
void TestMarkdownEdit::wrapsAndUnwraps_data()
{
    QTest::addColumn<QString>("marker");
    QTest::addColumn<QString>("before");
    QTest::addColumn<QString>("after");
    QTest::newRow("bold a selection")
        << u"**"_s << u"say [hello] there"_s << u"say **[hello]** there"_s;
    QTest::newRow("unbold it") << u"**"_s << u"say **[hello]** there"_s << u"say [hello] there"_s;
    QTest::newRow("unbold with marks selected")
        << u"**"_s << u"say [**hello**] there"_s << u"say [hello] there"_s;
    QTest::newRow("spaces stay outside")
        << u"**"_s << u"say[ hello ]there"_s << u"say **[hello]** there"_s;
    QTest::newRow("the word at the cursor") << u"*"_s << u"say hel|lo"_s << u"say *hel|lo*"_s;
    QTest::newRow("unitalic the word") << u"*"_s << u"say *hel|lo*"_s << u"say hel|lo"_s;
    QTest::newRow("italic inside bold") << u"*"_s << u"**[hi]**"_s << u"***[hi]***"_s;
    QTest::newRow("bold is not italic") << u"*"_s << u"**hi|**"_s << u"***hi|***"_s;
    QTest::newRow("unbold keeps italic") << u"**"_s << u"***[hi]***"_s << u"*[hi]*"_s;
    QTest::newRow("a pair to type in") << u"`"_s << u"run | now"_s << u"run `|` now"_s;
    QTest::newRow("an empty pair goes") << u"`"_s << u"run `|` now"_s << u"run | now"_s;
    QTest::newRow("each line") << u"~~"_s << u"[one\n\ntwo]"_s << u"[~~one~~\n\n~~two~~]"_s;
    QTest::newRow("each line, off") << u"~~"_s << u"[~~one~~\n~~two~~]"_s << u"[one\ntwo]"_s;
}

void TestMarkdownEdit::wrapsAndUnwraps()
{
    QFETCH(QString, marker);
    QFETCH(QString, before);
    QFETCH(QString, after);
    const Marked in = unmark(before);
    QCOMPARE(applied(in.text, wrap(in.text, in.from, in.to, marker)), after);
}

void TestMarkdownEdit::stylesLines_data()
{
    QTest::addColumn<int>("style");
    QTest::addColumn<QString>("before");
    QTest::addColumn<QString>("after");
    const auto row = [](const char *name, LineStyle style, const QString &before,
                        const QString &after) {
        QTest::newRow(name) << int(style) << before << after;
    };
    row("a task on an empty line", LineStyle::Task, u"one\n|\ntwo"_s, u"one\n- [ ] |\ntwo"_s);
    row("a task keeps the cursor", LineStyle::Task, u"buy mi|lk"_s, u"- [ ] buy mi|lk"_s);
    row("a bullet becomes a task", LineStyle::Task, u"  - buy |milk"_s, u"  - [ ] buy |milk"_s);
    row("a task goes", LineStyle::Task, u"- [x] buy |milk"_s, u"buy |milk"_s);
    row("numbers count", LineStyle::Numbered, u"[a\n\nb\nc]"_s, u"[1. a\n\n2. b\n3. c]"_s);
    row("bullets off", LineStyle::Bullet, u"[- a\n* b]"_s, u"[a\nb]"_s);
    row("mixed lists all get it", LineStyle::Bullet, u"[- a\nb]"_s, u"[- a\n- b]"_s);
    row("the line after a selection is left", LineStyle::Quote, u"[a\n]b"_s, u"[> a\n]b"_s);
    row("quote keeps the list", LineStyle::Quote, u"- a|"_s, u"> - a|"_s);
    row("heading", LineStyle::Heading2, u"Ti|tle"_s, u"## Ti|tle"_s);
    row("heading level changes", LineStyle::Heading1, u"### Ti|tle"_s, u"# Ti|tle"_s);
    row("same heading goes", LineStyle::Heading2, u"## Ti|tle"_s, u"Ti|tle"_s);
    row("cursor in the marks goes to the text", LineStyle::Heading1, u"#|# Title"_s, u"# |Title"_s);
}

void TestMarkdownEdit::stylesLines()
{
    QFETCH(int, style);
    QFETCH(QString, before);
    QFETCH(QString, after);
    const Marked in = unmark(before);
    QCOMPARE(applied(in.text, styleLines(in.text, in.from, in.to, LineStyle(style))), after);
}

void TestMarkdownEdit::makesLinks_data()
{
    QTest::addColumn<QString>("text");
    QTest::addColumn<int>("from");
    QTest::addColumn<int>("to");
    QTest::addColumn<QString>("after");
    QTest::addColumn<int>("cursor");
    QTest::newRow("a label") << u"see docs now"_s << 4 << 8 << u"see [docs]() now"_s << 11;
    QTest::newRow("a url") << u"at https://x.org"_s << 3 << 16 << u"at [](https://x.org)"_s << 4;
    QTest::newRow("nothing") << u"ab"_s << 1 << 1 << u"a[]()b"_s << 2;
}

void TestMarkdownEdit::makesLinks()
{
    QFETCH(QString, text);
    QFETCH(int, from);
    QFETCH(int, to);
    QFETCH(QString, after);
    QFETCH(int, cursor);
    const Edit edit = link(text, from, to);
    QCOMPARE(QString(text).replace(edit.start, edit.end - edit.start, edit.text), after);
    QCOMPARE(edit.selectionStart, cursor);
    QCOMPARE(edit.selectionEnd, cursor);
}

void TestMarkdownEdit::fencesCode_data()
{
    QTest::addColumn<QString>("before");
    QTest::addColumn<QString>("after");
    QTest::newRow("fence") << u"a\nx = |1\nb"_s << u"a\n```\nx = |1\n```\nb"_s;
    QTest::newRow("unfence") << u"a\n```\nx = |1\n```\nb"_s << u"a\nx = |1\nb"_s;
    QTest::newRow("unfence a tagged fence") << u"```cpp\n[x\ny]\n```"_s << u"[x\ny]"_s;
    QTest::newRow("unfence from the middle") << u"```\na\n|b\nc\n```"_s << u"a\n|b\nc"_s;
    QTest::newRow("unfence from a fence line") << u"|```\na\n```"_s << u"|a"_s;
    QTest::newRow("between two blocks")
        << u"```\na\n```\n|b\n```\nc\n```"_s << u"```\na\n```\n```\n|b\n```\n```\nc\n```"_s;
    QTest::newRow("under a closing fence")
        << u"```\na\n```\n|b"_s << u"```\na\n```\n```\n|b\n```"_s;
    QTest::newRow("an unclosed block") << u"```\na\n|b"_s << u"a\n|b"_s;
}

void TestMarkdownEdit::fencesCode()
{
    QFETCH(QString, before);
    QFETCH(QString, after);
    const Marked in = unmark(before);
    QCOMPARE(applied(in.text, codeBlock(in.text, in.from, in.to)), after);
}

// An empty `after` means Return is left to break the line as usual.
void TestMarkdownEdit::continuesLists_data()
{
    QTest::addColumn<QString>("before");
    QTest::addColumn<QString>("after");
    QTest::newRow("bullet") << u"- milk|"_s << u"- milk\n- |"_s;
    QTest::newRow("star bullet keeps its star") << u"* milk|"_s << u"* milk\n* |"_s;
    QTest::newRow("number counts on") << u"9. milk|"_s << u"9. milk\n10. |"_s;
    QTest::newRow("paren numbers") << u"2) milk|"_s << u"2) milk\n3) |"_s;
    QTest::newRow("a ticked task gives an open one")
        << u"- [x] milk|"_s << u"- [x] milk\n- [ ] |"_s;
    QTest::newRow("splits the item") << u"- mi|lk"_s << u"- mi\n- |lk"_s;
    QTest::newRow("nested keeps its indent") << u"  - milk|"_s << u"  - milk\n  - |"_s;
    QTest::newRow("quote") << u"> said|"_s << u"> said\n> |"_s;
    QTest::newRow("quoted list") << u"> - a|"_s << u"> - a\n> - |"_s;
    QTest::newRow("empty item ends the list") << u"- a\n- |"_s << u"- a\n|"_s;
    QTest::newRow("empty task ends the list") << u"- a\n- [ ] |"_s << u"- a\n|"_s;
    QTest::newRow("empty nested item steps out") << u"- a\n  - |"_s << u"- a\n- |"_s;
    QTest::newRow("empty quote ends it") << u"> a\n> |"_s << u"> a\n|"_s;
    QTest::newRow("quoted heading carries on as a quote") << u"> # Title|"_s << u"> # Title\n> |"_s;
    QTest::newRow("empty nested number steps out as far as Tab nested it")
        << u"1. a\n   2. |"_s << u"1. a\n2. |"_s;
    QTest::newRow("in a code block") << u"```\n- a|\n```"_s << QString();
    QTest::newRow("on a fence") << u"```- a|"_s << QString();
    QTest::newRow("plain line") << u"milk|"_s << QString();
    QTest::newRow("heading") << u"# Title|"_s << QString();
    QTest::newRow("cursor in the marker") << u"-| milk"_s << QString();
    QTest::newRow("selection over lines") << u"- [a\n- b]"_s << QString();
}

void TestMarkdownEdit::continuesLists()
{
    QFETCH(QString, before);
    QFETCH(QString, after);
    const Marked in = unmark(before);
    const std::optional<Edit> edit = continueList(in.text, in.from, in.to);
    QCOMPARE(edit ? applied(in.text, *edit) : QString(), after);
}

void TestMarkdownEdit::indentsLists_data()
{
    QTest::addColumn<bool>("outdent");
    QTest::addColumn<QString>("before");
    QTest::addColumn<QString>("after");
    QTest::newRow("bullet") << false << u"- a\n- b|"_s << u"- a\n  - b|"_s;
    QTest::newRow("number lines up with the text")
        << false << u"1. a\n2. b|"_s << u"1. a\n   2. b|"_s;
    QTest::newRow("several lines") << false << u"[- a\n- b]"_s << u"[  - a\n  - b]"_s;
    QTest::newRow("out") << true << u"- a\n  - b|"_s << u"- a\n- b|"_s;
    QTest::newRow("out of a tab") << true << u"\t- b|"_s << u"- b|"_s;
    QTest::newRow("out at the margin stays") << true << u"- b|"_s << u"- b|"_s;
    QTest::newRow("cursor in the indent") << true << u" | - b"_s << u"|- b"_s;
    QTest::newRow("quoted item") << false << u"> - a|"_s << QString();
    QTest::newRow("in a code block") << false << u"```\n- a|\n```"_s << QString();
    QTest::newRow("not a list") << false << u"plain|"_s << QString();
    QTest::newRow("partly a list") << false << u"[- a\nb]"_s << QString();
}

void TestMarkdownEdit::indentsLists()
{
    QFETCH(bool, outdent);
    QFETCH(QString, before);
    QFETCH(QString, after);
    const Marked in = unmark(before);
    const std::optional<Edit> edit = indentList(in.text, in.from, in.to, outdent);
    QCOMPARE(edit ? applied(in.text, *edit) : QString(), after);
}

// Tables mark the cursor with ^, since their cells are full of pipes. A cursor before a
// row's first pipe lands at the start of its first cell.
void TestMarkdownEdit::linesUpTables_data()
{
    QTest::addColumn<QString>("before");
    QTest::addColumn<QString>("after");
    QTest::newRow("pads to the widest cell")
        << u"^|a|bee|\n|-|-|\n|cc|d|"_s << u"| ^a   | bee |\n| --- | --- |\n| cc  | d   |"_s;
    QTest::newRow("keeps the cursor in its cell")
        << u"| name | x |\n|--|--|\n| Ma^ra | 1 |"_s
        << u"| name | x   |\n| ---- | --- |\n| Ma^ra | 1   |"_s;
    QTest::newRow("alignment from the delimiter")
        << u"^| a | b | c |\n|:-|:-:|-:|\n| one | two | three |"_s
        << u"| ^a   |  b  |     c |\n| :-- | :-: | ----: |\n| one | two | three |"_s;
    QTest::newRow("without outer pipes")
        << u"a | b\n- | -\n1 | 2^"_s << u"| a   | b   |\n| --- | --- |\n| 1   | 2^   |"_s;
    QTest::newRow("escaped and code pipes stay in the cell")
        << u"^| a \\| b | `c|d` |\n|-|-|"_s << u"| ^a \\| b | `c|d` |\n| ------ | ----- |"_s;
    QTest::newRow("an escaped pipe ends the last cell")
        << u"^| a | b \\|\n|-|-|"_s << u"| ^a   | b \\| |\n| --- | ---- |"_s;
    QTest::newRow("short rows get their cells")
        << u"^| a | b |\n|-|-|\n| 1 |"_s << u"| ^a   | b   |\n| --- | --- |\n| 1   |     |"_s;
}

void TestMarkdownEdit::linesUpTables()
{
    QFETCH(QString, before);
    QFETCH(QString, after);
    const qsizetype cursor = before.indexOf(u'^');
    const QString text = QString(before).remove(cursor, 1);
    const std::optional<Edit> edit = formatTable(text, cursor, cursor);
    QVERIFY(edit);
    QString out = text;
    out.replace(edit->start, edit->end - edit->start, edit->text);
    QCOMPARE(out.insert(edit->selectionStart, u'^'), after);
}

void TestMarkdownEdit::linesUpASelectionAcrossRows()
{
    // Two rows selected: the button lines the table up rather than adding another.
    const QString text = u"| a | b |\n|-|-|\n| c | d |"_s;
    const std::optional<Edit> edit = formatTable(text, 2, text.indexOf(u'd') + 1);
    QVERIFY(edit);
    QString out = text;
    out.replace(edit->start, edit->end - edit->start, edit->text);
    QCOMPARE(out, u"| a   | b   |\n| --- | --- |\n| c   | d   |"_s);
    QCOMPARE(out.mid(edit->selectionStart, edit->selectionEnd - edit->selectionStart),
             u"a   | b   |\n| --- | --- |\n| c   | d"_s);
}

void TestMarkdownEdit::movesThroughCells()
{
    const QString text = u"| a | b |\n|-|-|\n| c | d |"_s;
    // From "a", Tab selects "b", then skips the delimiter row to "c".
    std::optional<Edit> edit = moveInTable(text, 2, false);
    QVERIFY(edit);
    QString out = text;
    out.replace(edit->start, edit->end - edit->start, edit->text);
    QCOMPARE(out.mid(edit->selectionStart, edit->selectionEnd - edit->selectionStart), u"b"_s);
    edit = moveInTable(out, edit->selectionStart, false);
    QString next = out;
    next.replace(edit->start, edit->end - edit->start, edit->text);
    QCOMPARE(next.mid(edit->selectionStart, edit->selectionEnd - edit->selectionStart), u"c"_s);

    // Shift+Tab from "c" goes back over the delimiter to "b".
    edit = moveInTable(next, edit->selectionStart, true);
    QString back = next;
    back.replace(edit->start, edit->end - edit->start, edit->text);
    QCOMPARE(back.mid(edit->selectionStart, edit->selectionEnd - edit->selectionStart), u"b"_s);

    // Tab in the last cell adds a row and lands in its first cell.
    const qsizetype last = back.lastIndexOf(u'd');
    edit = moveInTable(back, last, false);
    QString added = back;
    added.replace(edit->start, edit->end - edit->start, edit->text);
    QCOMPARE(added, u"| a   | b   |\n| --- | --- |\n| c   | d   |\n|     |     |"_s);
    QCOMPARE(edit->selectionStart, added.lastIndexOf(u'\n') + 3);

    QVERIFY(!moveInTable(u"no table | here"_s, 3, false));
}

void TestMarkdownEdit::addsAndEndsRows()
{
    const QString text = u"| a | b |\n|-|-|\n| c | d |\n\nAfter."_s;
    const std::optional<Edit> row = newTableRow(text, 18);
    QVERIFY(row);
    QString added = text;
    added.replace(row->start, row->end - row->start, row->text);
    QCOMPARE(added, u"| a   | b   |\n| --- | --- |\n| c   | d   |\n|     |     |\n\nAfter."_s);

    // Return on that empty row ends the table, leaving the cursor on the blank line.
    const std::optional<Edit> ended = newTableRow(added, row->selectionStart);
    QVERIFY(ended);
    QString done = added;
    done.replace(ended->start, ended->end - ended->start, ended->text);
    QCOMPARE(done, u"| a   | b   |\n| --- | --- |\n| c   | d   |\n\n\nAfter."_s);
    QCOMPARE(ended->selectionStart, done.indexOf(u"\n\n"_s) + 1);

    // Return on the heading adds the row under the delimiter.
    const std::optional<Edit> top = newTableRow(text, 2);
    QString under = text;
    under.replace(top->start, top->end - top->start, top->text);
    QVERIFY(under.startsWith(u"| a   | b   |\n| --- | --- |\n|     |     |\n| c   | d   |"_s));

    QVERIFY(!newTableRow(u"a | b"_s, 1));
    QVERIFY(!newTableRow(u"```\n| a | b |\n|-|-|\n```"_s, 6));
}

void TestMarkdownEdit::insertsTables_data()
{
    QTest::addColumn<QString>("before");
    QTest::addColumn<QString>("after");
    const QString table = u"| [Column] | Column |\n| ------ | ------ |\n|        |        |"_s;
    QTest::newRow("on an empty note") << u"|"_s << table;
    QTest::newRow("after a line of text")
        << u"Some te|xt\nMore"_s << u"Some text\n\n"_s + table + u"\n\nMore"_s;
    QTest::newRow("a selection from a blank line keeps its text")
        << u"[\nfoo\nbar]"_s
        << u"\nfoo\nbar\n\n"_s +
               u"| [Column] | Column |\n| ------ | ------ |\n|        |        |"_s;
    QTest::newRow("on a blank line between paragraphs")
        << u"One\n|\nTwo"_s << u"One\n\n"_s + table + u"\n\nTwo"_s;
}

void TestMarkdownEdit::insertsTables()
{
    QFETCH(QString, before);
    QFETCH(QString, after);
    const Marked in = unmark(before);
    const QString &text = in.text;
    const Edit edit = insertTable(text, in.from, in.to);
    QString out = text;
    out.replace(edit.start, edit.end - edit.start, edit.text);
    out.insert(edit.selectionEnd, u']').insert(edit.selectionStart, u'[');
    QCOMPARE(out, after);
}

void TestMarkdownEdit::findsTheWikilinkBeingTyped_data()
{
    QTest::addColumn<QString>("before");
    QTest::addColumn<QString>("typed");
    QTest::newRow("just opened") << u"see [[|"_s << QString();
    QTest::newRow("some typed") << u"see [[Mara V|"_s << u"Mara V"_s;
    QTest::newRow("a path") << u"[[Ashfall/Pl|"_s << u"Ashfall/Pl"_s;
    QTest::newRow("inside a closed link") << u"[[Ma|ra]]"_s << u"Ma"_s;
    QTest::newRow("after the link") << u"[[Mara]] and |"_s << u"-"_s;
    QTest::newRow("in the label") << u"[[Mara|the cap|"_s << u"-"_s;
    QTest::newRow("in the heading") << u"[[Mara#Ear|"_s << u"-"_s;
    QTest::newRow("on another line") << u"[[Mara\nnext|"_s << u"-"_s;
    QTest::newRow("in inline code") << u"`[[Ma|"_s << u"-"_s;
    QTest::newRow("in a code block") << u"```\n[[Ma|\n```"_s << u"-"_s;
}

void TestMarkdownEdit::findsTheWikilinkBeingTyped()
{
    QFETCH(QString, before);
    QFETCH(QString, typed);
    // The cursor is the last bar, since a label's bar comes before it.
    const qsizetype cursor = before.lastIndexOf(u'|');
    const QString text = QString(before).remove(cursor, 1);
    const std::optional<WikilinkQuery> query = wikilinkAt(text, cursor);
    QCOMPARE(query ? query->text : u"-"_s, typed);
    if (query)
        QCOMPARE(text.mid(query->start - 2, 2), u"[["_s);
}

void TestMarkdownEdit::findsTheEmojiBeingTyped_data()
{
    QTest::addColumn<QString>("before");
    QTest::addColumn<QString>("typed");
    QTest::newRow("at the start") << u":thu|"_s << u"thu"_s;
    QTest::newRow("after a word") << u"well done :thumbs_up|"_s << u"thumbs up"_s;
    QTest::newRow("in brackets") << u"(:fox|"_s << u"fox"_s;
    QTest::newRow("plus one") << u":+1|"_s << u"+1"_s;
    QTest::newRow("one letter is too few") << u":t|"_s << u"-"_s;
    QTest::newRow("a time") << u"at 10:30|"_s << u"-"_s;
    QTest::newRow("a link") << u"https://x|"_s << u"-"_s;
    QTest::newRow("a label") << u"Note: bring|"_s << u"-"_s;
    QTest::newRow("in inline code") << u"`:fox|"_s << u"-"_s;
    QTest::newRow("in a code block") << u"```\n:fox|\n```"_s << u"-"_s;
}

void TestMarkdownEdit::findsTheEmojiBeingTyped()
{
    QFETCH(QString, before);
    QFETCH(QString, typed);
    const qsizetype cursor = before.indexOf(u'|');
    const QString text = QString(before).remove(cursor, 1);
    const std::optional<EmojiQuery> query = emojiAt(text, cursor);
    QCOMPARE(query ? query->text : u"-"_s, typed);
    if (query)
        QCOMPARE(text.at(query->start), u':');
}

void TestMarkdownEdit::findsTheMarkerBeingTyped()
{
    const auto typed = [](const QString &before) {
        const qsizetype cursor = before.indexOf(u'|');
        const QString text = QString(before).remove(cursor, 1);
        const std::optional<EmojiQuery> query = markerAt(text, cursor);
        return query ? text.mid(query->start, cursor - query->start) : u"-"_s;
    };
    // After an @ on a to-do's line, even before any letter.
    QCOMPARE(typed(u"- [ ] water @we|"_s), u"@we"_s);
    QCOMPARE(typed(u"- [ ] call @p1|"_s), u"@p1"_s);
    QCOMPARE(typed(u"  1. [x] water @|"_s), u"@"_s);
    // Not in an address, the box itself, other lines or code.
    QCOMPARE(typed(u"- [ ] mail me@ex|"_s), u"-"_s);
    QCOMPARE(typed(u"Water @we|"_s), u"-"_s);
    QCOMPARE(typed(u"- [ ] call `x @we|"_s), u"-"_s);
    QCOMPARE(typed(u"- [ ] water @due 2026|"_s), u"-"_s);
}

void TestMarkdownEdit::completesWikilinks_data()
{
    QTest::addColumn<QString>("before");
    QTest::addColumn<QString>("after");
    QTest::newRow("closes it") << u"see [[Ma| now"_s << u"see [[Mara Vey]]| now"_s;
    QTest::newRow("fills an open pair") << u"see [[Ma|]] now"_s << u"see [[Mara Vey]]| now"_s;
    QTest::newRow("replaces the rest of the target") << u"[[Ma|rx]]"_s << u"[[Mara Vey]]|"_s;
    QTest::newRow("keeps a label")
        << u"[[Ma|ra|the captain]] now"_s << u"[[Mara Vey|the captain]]| now"_s;
    QTest::newRow("keeps a heading") << u"[[Ma|ra#Early life]]"_s << u"[[Mara Vey#Early life]]|"_s;
    QTest::newRow("keeps a later link")
        << u"[[Ma| and [[Soren]]"_s << u"[[Mara Vey]]| and [[Soren]]"_s;
}

void TestMarkdownEdit::completesWikilinks()
{
    QFETCH(QString, before);
    QFETCH(QString, after);
    // The cursor is the first bar; a label's bar comes after it.
    const qsizetype cursor = before.indexOf(u'|');
    const QString text = QString(before).remove(cursor, 1);
    const std::optional<Edit> edit = completeWikilink(text, cursor, u"Mara Vey"_s);
    QVERIFY(edit);
    QString out = text;
    out.replace(edit->start, edit->end - edit->start, edit->text);
    QCOMPARE(out.insert(edit->selectionStart, u'|'), after);
    QVERIFY(!completeWikilink(u"no link here"_s, 3, u"Mara"_s));
}

void TestMarkdownEdit::setsFrontMatterKeepingTheSelection()
{
    const QString note = u"Buy milk.\n"_s;
    const Edit made = setKey(note, 4, 8, u"task"_s, u"open"_s);
    QCOMPARE(applied(note, made), u"---\ntask: open\n---\nBuy [milk].\n"_s);

    const QString task = u"---\ntask: open\ndue: 2026-10-20\n---\nBuy milk.\n"_s;
    const Edit dropped = setKey(task, 37, 37, u"due"_s, QVariant());
    QCOMPARE(applied(task, dropped), u"---\ntask: open\n---\nBu|y milk.\n"_s);
}

void TestMarkdownEdit::editsAreAsSmallAsTheyCanBe()
{
    const Edit edit = wrap(u"a long line with one word"_s, 7, 11, u"**"_s);
    QCOMPARE(edit.start, 7);
    QCOMPARE(edit.end, 11);
    QCOMPARE(edit.text, u"**line**"_s);
}

QTEST_GUILESS_MAIN(TestMarkdownEdit)
#include "tst_markdownedit.moc"
