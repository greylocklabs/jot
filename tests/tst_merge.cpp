#include "jot/Merge.h"

#include <QTest>

using namespace jot;
using namespace Qt::StringLiterals;

class TestMerge : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void keepsEachLinesEnding();
    void merges_data();
    void merges();
    void givesUpOnTextTooDifferentToCompare();
    void movesAPositionWithTheTextAroundIt();
};

void TestMerge::merges_data()
{
    QTest::addColumn<QString>("base");
    QTest::addColumn<QString>("mine");
    QTest::addColumn<QString>("theirs");
    QTest::addColumn<bool>("clean");
    QTest::addColumn<QString>("merged");

    const QString base = u"one\ntwo\nthree\nfour\nfive\n"_s;
    QTest::newRow("only mine") << base << u"one\n2\nthree\nfour\nfive\n"_s << base << true
                               << u"one\n2\nthree\nfour\nfive\n"_s;
    QTest::newRow("only theirs") << base << base << u"one\ntwo\nthree\nfour\n5\n"_s << true
                                 << u"one\ntwo\nthree\nfour\n5\n"_s;
    QTest::newRow("apart") << base << u"1\ntwo\nthree\nfour\nfive\n"_s
                           << u"one\ntwo\nthree\nfour\nfive\nsix\n"_s << true
                           << u"1\ntwo\nthree\nfour\nfive\nsix\n"_s;
    QTest::newRow("a line each, apart")
        << base << u"one\nTWO\nthree\nfour\nfive\n"_s << u"one\ntwo\nthree\nFOUR\nfive\n"_s << true
        << u"one\nTWO\nthree\nFOUR\nfive\n"_s;
    QTest::newRow("the same change")
        << base << u"one\ntwo\n3\nfour\nfive\n"_s << u"one\ntwo\n3\nfour\nfive\nsix\n"_s << true
        << u"one\ntwo\n3\nfour\nfive\nsix\n"_s;
    QTest::newRow("one removes, one adds")
        << base << u"one\nthree\nfour\nfive\n"_s << u"one\ntwo\nthree\nfour\nfive\nsix\n"_s << true
        << u"one\nthree\nfour\nfive\nsix\n"_s;
    QTest::newRow("the same line") << base << u"one\nmine\nthree\nfour\nfive\n"_s
                                   << u"one\ntheirs\nthree\nfour\nfive\n"_s << false << QString();
    QTest::newRow("lines next to each other")
        << base << u"one\nmine\nthree\nfour\nfive\n"_s << u"one\ntwo\ntheirs\nfour\nfive\n"_s
        << false << QString();
    QTest::newRow("both add at the end")
        << base << base + u"mine\n"_s << base + u"theirs\n"_s << false << QString();
    QTest::newRow("from nothing") << QString() << u"mine\n"_s << u"theirs\n"_s << false
                                  << QString();
    QTest::newRow("windows lines")
        << u"one\r\ntwo\r\nthree\r\nfour\r\n"_s << u"1\r\ntwo\r\nthree\r\nfour\r\n"_s
        << u"one\r\ntwo\r\nthree\r\n4\r\n"_s << true << u"1\r\ntwo\r\nthree\r\n4\r\n"_s;
    QTest::newRow("no last line break")
        << u"one\ntwo\nthree"_s << u"1\ntwo\nthree"_s << u"one\ntwo\nthree, and more"_s << true
        << u"1\ntwo\nthree, and more"_s;
}

void TestMerge::merges()
{
    QFETCH(QString, base);
    QFETCH(QString, mine);
    QFETCH(QString, theirs);
    QFETCH(bool, clean);
    QFETCH(QString, merged);
    const std::optional<QString> result = mergeText(base, mine, theirs);
    QCOMPARE(result.has_value(), clean);
    if (clean)
        QCOMPARE(*result, merged);
}

void TestMerge::givesUpOnTextTooDifferentToCompare()
{
    QString base;
    QString mine;
    QString theirs;
    for (int i = 0; i < 3000; ++i) {
        base += u"line %1\n"_s.arg(i);
        mine += u"mine %1\n"_s.arg(i);
        theirs += u"line %1\n"_s.arg(i);
    }
    theirs += u"one more\n"_s;
    QVERIFY(!mergeText(base, mine, theirs));
}

void TestMerge::movesAPositionWithTheTextAroundIt()
{
    const QString before = u"one\ntwo\nthree\n"_s;
    // In "two", after the t.
    const int inTwo = 5;
    QCOMPARE(movedPosition(before, before, inTwo), inTwo);
    // A line put in ahead of it moves it along; one after leaves it.
    QCOMPARE(movedPosition(before, u"zero\none\ntwo\nthree\n"_s, inTwo), inTwo + 5);
    QCOMPARE(movedPosition(before, u"one\ntwo\nthree\nfour\n"_s, inTwo), inTwo);
    // Changes on both sides of it.
    QCOMPARE(movedPosition(before, u"ONE!\ntwo\nthree, four\n"_s, inTwo), inTwo + 1);
    // Its own line changed: at that line's end.
    QCOMPARE(movedPosition(before, u"one\nTWO, too\nthree\n"_s, inTwo), 12);
    // Ahead of it taken out.
    QCOMPARE(movedPosition(before, u"two\nthree\n"_s, inTwo), 1);
}

void TestMerge::keepsEachLinesEnding()
{
    // Without \r\n there is nothing to keep.
    QCOMPARE(keepLineEndings(u"a\nb\n"_s, u"a\nb\nc"_s), u"a\nb\nc"_s);
    // All CRLF: new and changed lines take it too.
    QCOMPARE(keepLineEndings(u"a\r\nb\r\n"_s, u"a\nB\nc\n"_s), u"a\r\nB\r\nc\r\n"_s);
    // Mixed: kept lines keep theirs, wherever they move to; new ones take the usual one.
    QCOMPARE(keepLineEndings(u"a\nb\r\nc\nd\n"_s, u"new\na\nb\nd\n"_s), u"new\na\nb\r\nd\n"_s);
    QCOMPARE(keepLineEndings(u"a\r\nb\r\nc\n"_s, u"a\nb\nc\nd\n"_s), u"a\r\nb\r\nc\nd\r\n"_s);
    // A last line with no break gets one when a line follows it, and none when it is last.
    QCOMPARE(keepLineEndings(u"a\r\nb"_s, u"a\nb\nc"_s), u"a\r\nb\r\nc"_s);
    QCOMPARE(keepLineEndings(u"a\r\nb\r\n"_s, u"a\nb"_s), u"a\r\nb"_s);
}

QTEST_GUILESS_MAIN(TestMerge)
#include "tst_merge.moc"
