#include "jot/FrontMatter.h"
#include "jot/NoteScan.h"

#include <QTest>

using namespace jot;
using namespace Qt::StringLiterals;

class TestNoteScan : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void tickingARepeatingTaskWritesTheNext();
    void readsAsMarkdownWithLinks();
    void findsEveryKindOfLink();
    void leavesUrlsAndAnchorsOut();
    void findsNestedTagsButNotHeadingsOrNumbers();
    void skipsCode();
    void findsTasks();
    void countsLinesFromTheTopOfTheFile();
    void ticksATaskAndNothingElse();
    void findsATaskThatMoved();
    void findsWhereLinesStart();
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
    const QString note = u"---\na: 1\n---\n- [ ] one\r\n  * [X] two\nplain\n3. [ ] three"_s;
    QCOMPARE(setTaskDone(note, 4, true),
             std::optional(u"---\na: 1\n---\n- [x] one\r\n  * [X] two\nplain\n3. [ ] three"_s));
    QCOMPARE(setTaskDone(note, 5, false),
             std::optional(u"---\na: 1\n---\n- [ ] one\r\n  * [ ] two\nplain\n3. [ ] three"_s));
    QCOMPARE(setTaskDone(note, 7, true),
             std::optional(u"---\na: 1\n---\n- [ ] one\r\n  * [X] two\nplain\n3. [x] three"_s));
    QVERIFY(!setTaskDone(note, 6, true));
    QVERIFY(!setTaskDone(note, 99, true));
}

void TestNoteScan::findsATaskThatMoved()
{
    // Read when "- [ ] buy" was line 1; a line has since gone in above it.
    const QString note = u"new line\n- [ ] buy\n- [ ] sell\n"_s;
    QCOMPARE(setTaskDone(note, 1, true, u"buy"_s),
             std::optional(u"new line\n- [x] buy\n- [ ] sell\n"_s));
    // The line it names says something else, and nothing says "buy": nothing changes.
    QVERIFY(!setTaskDone(u"- [ ] sell\n"_s, 1, true, u"buy"_s));
    // Two tasks say it, so which one moved cannot be told.
    QVERIFY(!setTaskDone(u"x\n- [ ] buy\n- [ ] buy\n"_s, 1, true, u"buy"_s));
    QCOMPARE(setTaskDone(u"- [ ] buy\n"_s, 1, true, u"buy"_s), std::optional(u"- [x] buy\n"_s));
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
    QCOMPARE(readingMarkdown(u"See [[Mara Vey]] and [[Cast/Soren|her brother]].\n"_s, embed),
             u"See [Mara Vey](jot-note:Mara%20Vey) and [her brother](jot-note:Cast/Soren).\n"_s);
    QCOMPARE(readingMarkdown(u"![[map.png]] ![[gone.png]]\n"_s, embed),
             u"![map.png](<file:///v/map.png>) [gone.png](jot-note:gone.png)\n"_s);
    QCOMPARE(readingMarkdown(u"#ferry and #cast/lead, not [x](#heading)\n"_s, embed),
             u"[#ferry](jot-tag:ferry) and [#cast/lead](jot-tag:cast/lead), not [x](#heading)\n"_s);
    // An image from the web is only linked to; one in the vault stays an image.
    QCOMPARE(readingMarkdown(u"![pixel](https://tracker.example/p.png) ![map](map.png)\n"_s, embed),
             u"[pixel](https://tracker.example/p.png) ![map](map.png)\n"_s);
    // Code is left as it is.
    const QString code = u"`[[not]] #code`\n```\n[[Mara]] #tag\n```\n"_s;
    QCOMPARE(readingMarkdown(code, embed), code);
    // A heading's # is not a tag, and lines keep their Windows line breaks.
    QCOMPARE(readingMarkdown(u"# Title\r\nA #tag\r\n"_s, embed),
             u"# Title\r\nA [#tag](jot-tag:tag)\r\n"_s);
}

void TestNoteScan::tickingARepeatingTaskWritesTheNext()
{
    const QString note = u"# Chores\n\n- [ ] Water 🔁 every week 📅 2026-10-28\n- [ ] Sweep\n"_s;
    const std::optional<QString> ticked = setTaskDone(note, 3, true, {}, QDate(2026, 10, 28));
    QVERIFY(ticked);
    QCOMPARE(*ticked, u"# Chores\n\n- [ ] Water 🔁 every week 📅 2026-11-04\n"
                      "- [x] Water 🔁 every week 📅 2026-10-28 ✅ 2026-10-28\n- [ ] Sweep\n"_s);
    // The editor's edit for the same: the line's stretch, and what replaces it.
    const TickEdit edit = tickedLineAt(note, note.indexOf(u"[ ] Water"_s) + 1, QDate(2026, 10, 28));
    QCOMPARE(edit.start, note.indexOf(u"- [ ] Water"_s));
    QCOMPARE(edit.end, note.indexOf(u"\n- [ ] Sweep"_s));
    QVERIFY(edit.text.startsWith(u"- [ ] Water 🔁 every week 📅 2026-11-04\n- [x] Water"_s));
    // Unticking only unticks.
    QCOMPARE(*setTaskDone(*ticked, 4, false, {}, QDate(2026, 10, 28)),
             u"# Chores\n\n- [ ] Water 🔁 every week 📅 2026-11-04\n"
             "- [ ] Water 🔁 every week 📅 2026-10-28 ✅ 2026-10-28\n- [ ] Sweep\n"_s);
}

QTEST_GUILESS_MAIN(TestNoteScan)
#include "tst_notescan.moc"
