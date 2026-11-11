#include "jot/FrontMatter.h"
#include "jot/NoteScan.h"

#include <QLocale>
#include <QTest>

using namespace jot;
using namespace Qt::StringLiterals;

class TestNoteScan : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void tickingARepeatingTaskWritesTheNext();
    void readsAsMarkdownWithLinks();
    void findsHeadingsForAContentsList();
    void readsCriticMarkupWithItsEditsMade();
    void findsMathAsPandocDoes();
    void findsEveryKindOfLink();
    void leavesUrlsAndAnchorsOut();
    void findsNestedTagsButNotHeadingsOrNumbers();
    void skipsCode();
    void findsTasks();
    void countsLinesFromTheTopOfTheFile();
    void ticksATaskAndNothingElse();
    void findsATaskThatMoved();
    void findsWhereLinesStart();
    void findsAToDosWords();
};

void TestNoteScan::findsEveryKindOfLink()
{
    const NoteScan scan = scanNote(uR"(See [[Mara]], [[Cast/Soren|her brother]] and [[Mara#Voice]].
![[map.png]] and [the ferry](Places/The%20Ferry.md "title") and ![sketch](Mara.assets/sketch.png)
[odd](<With spaces.md>) and [[ spaced ]]
)"_s);
    const QList<NoteScan::Link> expected{
        {u"Mara"_s, u"wiki"_s, 1},
        {u"Cast/Soren"_s, u"wiki"_s, 1},
        {u"Mara#Voice"_s, u"wiki"_s, 1},
        {u"map.png"_s, u"embed"_s, 2},
        {u"Places/The%20Ferry.md"_s, u"link"_s, 2},
        {u"Mara.assets/sketch.png"_s, u"embed"_s, 2},
        {u"With spaces.md"_s, u"link"_s, 3},
        {u"spaced"_s, u"wiki"_s, 3},
    };
    QCOMPARE(scan.links, expected);
}

void TestNoteScan::leavesUrlsAndAnchorsOut()
{
    const NoteScan scan = scanNote(
        u"[site](https://callieapp.org) [mail](mailto:a@b.c) [up](#top) <https://x.y>\n"_s);
    QVERIFY(scan.links.isEmpty());
}

void TestNoteScan::findsNestedTagsButNotHeadingsOrNumbers()
{
    const NoteScan scan = scanNote(uR"(# Heading
## Another
Tagged #ashfall and #cast/lead, (#idea), #2026 is a number, #v2 is fine.
Not a tag: a#b, [[#Heading]], issue #12, [link](#anchor)
#Ashfall again
)"_s);
    QCOMPARE(scan.tags, (QStringList{u"ashfall"_s, u"cast/lead"_s, u"idea"_s, u"v2"_s}));
}

void TestNoteScan::skipsCode()
{
    const NoteScan scan = scanNote(uR"(```cpp
#include <x> [[NotALink]]
```
~~~
- [ ] not a task
~~~
Inline `#notatag [[NotALink]]` but #real and [[Real]].
    indented #code [[NotALink]]
)"_s);
    QCOMPARE(scan.tags, QStringList{u"real"_s});
    QCOMPARE(scan.links, (QList<NoteScan::Link>{{u"Real"_s, u"wiki"_s, 7}}));
    QVERIFY(scan.tasks.isEmpty());
}

void TestNoteScan::findsTasks()
{
    const NoteScan scan = scanNote(uR"(- [ ] buy paper
- [x] find shop
  * [X] nested and done
1. [ ] numbered
- [] not a task
- plain
)"_s);
    const QList<NoteScan::Task> expected{{1, false, u"buy paper"_s},
                                         {2, true, u"find shop"_s},
                                         {3, true, u"nested and done"_s},
                                         {4, false, u"numbered"_s}};
    QCOMPARE(scan.tasks, expected);
}

void TestNoteScan::countsLinesFromTheTopOfTheFile()
{
    const QString note = u"---\ntitle: x\n---\nline four [[A]]\r\n- [ ] five\r\n"_s;
    const NoteParts parts = splitFrontMatter(note);
    const int first = bodyFirstLine(note, parts.body);
    QCOMPARE(first, 4);
    const NoteScan scan = scanNote(parts.body, first);
    QCOMPARE(scan.links.first().line, 4);
    QCOMPARE(scan.tasks.first().line, 5);
    QCOMPARE(scan.tasks.first().text, u"five"_s);
}

void TestNoteScan::ticksATaskAndNothingElse()
{
    const QString note =
        u"---\na: 1\n---\n- [ ] one\r\n  * [X] two @done 2026-10-01\nplain\n3. [ ] three"_s;
    const QDate today(2026, 10, 28);
    // Ticked, a box says the day; unticked, it no longer does.
    QCOMPARE(setTaskDone(note, 4, true, {}, today),
             std::optional(
                 u"---\na: 1\n---\n- [x] one @done 2026-10-28\r\n  * [X] two @done 2026-10-01\n"
                 "plain\n3. [ ] three"_s));
    QCOMPARE(setTaskDone(note, 5, false, {}, today),
             std::optional(u"---\na: 1\n---\n- [ ] one\r\n  * [ ] two\nplain\n3. [ ] three"_s));
    QCOMPARE(setTaskDone(note, 7, true, {}, today),
             std::optional(u"---\na: 1\n---\n- [ ] one\r\n  * [X] two @done 2026-10-01\nplain\n"
                           "3. [x] three @done 2026-10-28"_s));
    QVERIFY(!setTaskDone(note, 6, true));
    QVERIFY(!setTaskDone(note, 99, true));
}

void TestNoteScan::findsATaskThatMoved()
{
    // Read when "- [ ] buy" was line 1; a line has since gone in above it.
    const QString note = u"new line\n- [ ] buy\n- [ ] sell\n"_s;
    const QDate today(2026, 10, 28);
    QCOMPARE(setTaskDone(note, 1, true, u"buy"_s, today),
             std::optional(u"new line\n- [x] buy @done 2026-10-28\n- [ ] sell\n"_s));
    // The line it names says something else, and nothing says "buy": nothing changes.
    QVERIFY(!setTaskDone(u"- [ ] sell\n"_s, 1, true, u"buy"_s));
    // Two tasks say it, so which one moved cannot be told.
    QVERIFY(!setTaskDone(u"x\n- [ ] buy\n- [ ] buy\n"_s, 1, true, u"buy"_s));
    QCOMPARE(setTaskDone(u"- [ ] buy\n"_s, 1, true, u"buy"_s, today),
             std::optional(u"- [x] buy @done 2026-10-28\n"_s));
}

void TestNoteScan::findsAToDosWords()
{
    const QString note = u"# Chores\n  * [x]  water @weekly  \nsweep\n- [ ] "_s;
    const std::optional<TaskWords> water = taskWordsAt(note, note.indexOf(u"weekly"_s));
    QVERIFY(water);
    QCOMPARE(note.sliced(water->start, water->end - water->start), u"water @weekly"_s);
    // Anywhere on the line, even its start.
    QCOMPARE(taskWordsAt(note, note.indexOf(u"  * [x]"_s))->start, water->start);
    QVERIFY(!taskWordsAt(note, note.indexOf(u"sweep"_s)));
    QVERIFY(!taskWordsAt(note, 0));
    // A box with no words yet has them at the end of the line.
    const std::optional<TaskWords> empty = taskWordsAt(note, note.size());
    QVERIFY(empty);
    QCOMPARE(empty->start, note.size());
    QCOMPARE(empty->end, note.size());
}

void TestNoteScan::findsWhereLinesStart()
{
    QCOMPARE(lineStart(u"a\nbb\nccc", 1), 0);
    QCOMPARE(lineStart(u"a\nbb\nccc", 3), 5);
    QCOMPARE(lineStart(u"a\nbb\nccc", 9), 8);
}

void TestNoteScan::readsAsMarkdownWithLinks()
{
    const auto embed = [](const QString &target) {
        return target == u"map.png"_s ? u"file:///v/map.png"_s : QString();
    };
    // A to-do's markers are said as a reader would; elsewhere they are words.
    const int year = QDate::currentDate().year();
    QCOMPARE(readingMarkdown(u"- [ ] Water @weekly @due %1-11-02 #home\n- [x] Pay @done %1-11-01\n"
                             u"Sent @due %1-11-02\n"_s.arg(year),
                             embed),
             u"- [ ] Water 🔁 weekly 📅 %1 [#home](jot-tag:home)\n- [x] Pay ✅ %2\n"
             u"Sent @due %3-11-02\n"_s
                 .arg(QLocale().toString(QDate(year, 11, 2), u"MMM d"_s),
                      QLocale().toString(QDate(year, 11, 1), u"MMM d"_s))
                 .arg(year));
    QCOMPARE(readingMarkdown(u"- [ ] Call @every 2 weeks @due 2000-01-05 14:00\n"_s, embed),
             u"- [ ] Call 🔁 every 2 weeks 📅 %1\n"_s.arg(
                 QLocale().toString(QDate(2000, 1, 5), u"MMM d, yyyy"_s) + u", "_s +
                 QLocale().toString(QTime(14, 0), QLocale::ShortFormat)));
    QCOMPARE(readingMarkdown(u"See [[Mara Vey]] and [[Cast/Soren|her brother]].\n"_s, embed),
             u"See [Mara Vey](jot-note:Mara%20Vey) and [her brother](jot-note:Cast/Soren).\n"_s);
    QCOMPARE(readingMarkdown(u"![[map.png]] ![[gone.png]]\n"_s, embed),
             u"![map.png](<file:///v/map.png>) [gone.png](jot-note:gone.png)\n"_s);
    QCOMPARE(readingMarkdown(u"#ferry and #cast/lead, not [x](#heading)\n"_s, embed),
             u"[#ferry](jot-tag:ferry) and [#cast/lead](jot-tag:cast/lead), not [x](#heading)\n"_s);
    // An image from the web is only linked to; one in the vault stays an image.
    QCOMPARE(readingMarkdown(u"![pixel](https://tracker.example/p.png) ![map](map.png)\n"_s, embed),
             u"[pixel](https://tracker.example/p.png) ![map](map.png)\n"_s);
    // A target that is only an opening bracket reads as written.
    QCOMPARE(readingMarkdown(u"[x](<) ![](<)\n"_s, embed), u"[x](<) ![](<)\n"_s);
    // An image without words is named for its file, which Qt needs to draw it.
    QCOMPARE(readingMarkdown(u"![](Note.assets/my%20photo.png) ![kept](map.png)\n"_s, embed),
             u"![my photo.png](Note.assets/my%20photo.png) ![kept](map.png)\n"_s);
    // A sound is a link the reader can play, with its target as written; a link to one is not.
    QCOMPARE(readingMarkdown(u"![](Notes.assets/Take%201.ogg) ![[My Take.wav]] ![Mara](<m 1.mp3>) "
                             u"[plain](m.mp3)\n"_s,
                             embed),
             u"[Take 1.ogg](jot-sound:Notes.assets/Take%25201.ogg) "
             u"[My Take.wav](jot-wiki-sound:My%20Take.wav) [Mara](jot-sound:m%201.mp3) "
             u"[plain](m.mp3)\n"_s);
    // Code is left as it is.
    const QString code = u"`[[not]] #code`\n```\n[[Mara]] #tag\n```\n"_s;
    QCOMPARE(readingMarkdown(code, embed), code);
    // A heading's # is not a tag, and lines keep their Windows line breaks.
    QCOMPARE(readingMarkdown(u"# Title\r\nA #tag\r\n"_s, embed),
             u"# Title\r\nA [#tag](jot-tag:tag)\r\n"_s);
}

void TestNoteScan::findsHeadingsForAContentsList()
{
    const QString note =
        u"# Ferry\n${toc}\n## Crossing ##\n```\n# not one\n```\n### [[Mara]] *aboard*\n#tag\n"_s;
    const QList<Heading> found = headingsOf(note);
    QCOMPARE(found.size(), 3);
    QCOMPARE(found.at(0).text, u"Ferry"_s);
    QCOMPARE(found.at(1).level, 2);
    // Closing marks go; the rest stays as written.
    QCOMPARE(found.at(1).text, u"Crossing"_s);
    QCOMPARE(found.at(1).position, note.indexOf(u"## Crossing"_s));
    QCOMPARE(found.at(2).text, u"[[Mara]] *aboard*"_s);

    // Underlined headings count, as Markdown reads them; a rule after a list item does not.
    const QList<Heading> underlined =
        headingsOf(u"Part One\n========\n2026 plans\n---\n- item\n---\n\n> # quoted\n"_s);
    QCOMPARE(underlined.size(), 2);
    QCOMPARE(underlined.at(0).text, u"Part One"_s);
    QCOMPARE(underlined.at(0).level, 1);
    QCOMPARE(underlined.at(1).text, u"2026 plans"_s);
    QCOMPARE(underlined.at(1).level, 2);
    QCOMPARE(underlined.at(1).position, qsizetype(18));
    // In a quote or under a list item, an underline makes a heading the contents leave out.
    const QList<Heading> nested = allHeadingsOf(u"> Quote\n> ===\n\n- Item\n  ---\n\n## Later\n"_s);
    QCOMPARE(nested.size(), 3);
    QCOMPARE(nested.at(0).text, u"Quote"_s);
    QVERIFY(nested.at(0).nested);
    QCOMPARE(nested.at(1).text, u"Item"_s);
    QVERIFY(nested.at(1).nested);
    QVERIFY(!nested.at(2).nested);
    QCOMPARE(headingsOf(u"> Quote\n> ===\n\n- Item\n  ---\n\n## Later\n"_s).size(), 1);
    // A rule under an underline is a rule, not a heading named by the underline.
    const QList<Heading> ruled = headingsOf(u"Title\n===\n---\nA\n---\n---\n"_s);
    QCOMPARE(ruled.size(), 2);
    QCOMPARE(ruled.at(0).text, u"Title"_s);
    QCOMPARE(ruled.at(1).text, u"A"_s);

    QVERIFY(isTocMarker(u"  ${toc} "));
    QVERIFY(!isTocMarker(u"see ${toc}"));

    // Read as nested links to each heading by number, its brackets kept as text.
    const auto none = [](const QString &) { return QString(); };
    QCOMPARE(
        readingMarkdown(note, none),
        u"# Ferry\n\n- [Ferry](jot-heading:0)\n  - [Crossing](jot-heading:1)\n"
        "    - [\\[\\[Mara\\]\\] *aboard*](jot-heading:2)\n\n## Crossing ##\n```\n# not one\n```\n"
        "### [Mara](jot-note:Mara) *aboard*\n[#tag](jot-tag:tag)\n"_s);
    // Without headings there is nothing to list, and a marker in code stays.
    QCOMPARE(readingMarkdown(u"${toc}\nText\n"_s, none), u"\nText\n"_s);
    QCOMPARE(readingMarkdown(u"`${toc}`\n"_s, none), u"`${toc}`\n"_s);
}

void TestNoteScan::readsCriticMarkupWithItsEditsMade()
{
    const auto none = [](const QString &) { return QString(); };
    QCOMPARE(readingMarkdown(u"A {++new++} and {--old--} {~~cat~>dog~~} {==bright==}"
                             "{>> check this <<}\n"_s,
                             none),
             u"A new and ~~old~~ ~~cat~~ dog bright *(check this)*\n"_s);
    // A link inside an addition is still a link; code keeps the marks as written.
    QCOMPARE(readingMarkdown(u"{++see [[Mara]]++} `{--x--}`\n"_s, none),
             u"see [Mara](jot-note:Mara) `{--x--}`\n"_s);
}

void TestNoteScan::findsMathAsPandocDoes()
{
    const auto spans = [](const QString &line) {
        QStringList found;
        for (const auto &[start, end] : inlineMath(line))
            found << line.mid(start, end - start);
        return found;
    };
    QCOMPARE(spans(u"Area $\\pi r^2$ and $a$."_s), (QStringList{u"$\\pi r^2$"_s, u"$a$"_s}));
    // Prices, an escaped dollar, a space inside the signs and code are not math.
    QCOMPARE(spans(u"$5 and $10, \\$x\\$, $ x $, `$y$`"_s), QStringList());
    QCOMPARE(spans(u"$$x$$"_s), QStringList());

    QString math;
    QVERIFY(isMathFence(u"  $$ "));
    QVERIFY(isMathFence(u"$$x^2$$", &math));
    QCOMPARE(math, u"x^2"_s);
    QVERIFY(!isMathFence(u"$$ x"));

    const auto none = [](const QString &) { return QString(); };
    const auto drawn = [](const QString &tex, bool display) {
        return u"file:///m/%1%2.png"_s.arg(display ? u"d-"_s : QString()).arg(tex.size());
    };
    QCOMPARE(
        readingMarkdown(u"So $a+b$ is.\n$$\nx\ny\n$$\n```\n$c$\n```\n"_s, none, drawn),
        u"So ![a+b](<file:///m/3.png>) is.\n\n![x y](<file:///m/d-3.png>)\n\n```\n$c$\n```\n"_s);
    // Without a way to draw it, math stays as written.
    QCOMPARE(readingMarkdown(u"So $a$.\n"_s, none), u"So $a$.\n"_s);
}

void TestNoteScan::tickingARepeatingTaskWritesTheNext()
{
    const QString note = u"# Chores\n\n- [ ] Water @weekly @due 2026-10-28\n- [ ] Sweep\n"_s;
    const std::optional<QString> ticked = setTaskDone(note, 3, true, {}, QDate(2026, 10, 28));
    QVERIFY(ticked);
    QCOMPARE(*ticked, u"# Chores\n\n- [ ] Water @weekly @due 2026-11-04\n"
                      "- [x] Water @weekly @due 2026-10-28 @done 2026-10-28\n- [ ] Sweep\n"_s);
    // The editor's edit for the same: the line's stretch, and what replaces it.
    const TickEdit edit = tickedLineAt(note, note.indexOf(u"[ ] Water"_s) + 1, QDate(2026, 10, 28));
    QCOMPARE(edit.start, note.indexOf(u"- [ ] Water"_s));
    QCOMPARE(edit.end, note.indexOf(u"\n- [ ] Sweep"_s));
    QVERIFY(edit.text.startsWith(u"- [ ] Water @weekly @due 2026-11-04\n- [x] Water"_s));
    // Unticking only unticks, and takes away the day it was done.
    QCOMPARE(*setTaskDone(*ticked, 4, false, {}, QDate(2026, 10, 28)),
             u"# Chores\n\n- [ ] Water @weekly @due 2026-11-04\n"
             "- [ ] Water @weekly @due 2026-10-28\n- [ ] Sweep\n"_s);
}

QTEST_GUILESS_MAIN(TestNoteScan)
#include "tst_notescan.moc"
