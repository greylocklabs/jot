#include "jot/TextObjects.h"

#include <QTest>

using namespace jot;
using namespace Qt::StringLiterals;

class TestTextObjects : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void findsEachObject_data();
    void findsEachObject();
    void findsNothingWhereThereIsNone();
};

void TestTextObjects::findsEachObject_data()
{
    QTest::addColumn<QString>("text");
    // The cursor is where | is, and is taken out of the text.
    QTest::addColumn<QChar>("kind");
    QTest::addColumn<bool>("around");
    QTest::addColumn<QString>("covered");

    QTest::newRow("iw") << u"alpha be|ta gamma"_s << QChar(u'w') << false << u"beta"_s;
    QTest::newRow("aw, the space after")
        << u"alpha be|ta gamma"_s << QChar(u'w') << true << u"beta "_s;
    QTest::newRow("aw, the space before at the end")
        << u"alpha be|ta"_s << QChar(u'w') << true << u" beta"_s;
    QTest::newRow("iw, marks apart from letters")
        << u"sa|y, then"_s << QChar(u'w') << false << u"say"_s;
    QTest::newRow("iW, marks and all")
        << u"go to fo|o.bar/baz now"_s << QChar(u'W') << false << u"foo.bar/baz"_s;
    QTest::newRow("i\"") << u"say \"he|llo there\" now"_s << QChar(u'"') << false
                         << u"hello there"_s;
    QTest::newRow("a\"") << u"say \"he|llo there\" now"_s << QChar(u'"') << true
                         << u"\"hello there\" "_s;
    QTest::newRow("i\", before the quotes") << u"s|ay \"hi\""_s << QChar(u'"') << false << u"hi"_s;
    QTest::newRow("i'") << u"it is 'o|ne' or"_s << QChar(u'\'') << false << u"one"_s;
    QTest::newRow("i`") << u"run `ls -|l` here"_s << QChar(u'`') << false << u"ls -l"_s;
    QTest::newRow("i(") << u"f(a, (b|), c)"_s << QChar(u'(') << false << u"b"_s;
    QTest::newRow("ib, the outer pair")
        << u"f(a, (b), |c)"_s << QChar(u'b') << false << u"a, (b), c"_s;
    QTest::newRow("a(") << u"f(a, (b|), c)"_s << QChar(u'(') << true << u"(b)"_s;
    QTest::newRow("i(, on the bracket") << u"f|(x)"_s << QChar(u')') << false << u"x"_s;
    QTest::newRow("i[") << u"[[No|te]]"_s << QChar(u'[') << false << u"Note"_s;
    QTest::newRow("i{ across lines")
        << u"x {\n  a|b\n  cd\n}"_s << QChar(u'{') << false << u"  ab\n  cd\n"_s;
    QTest::newRow("aB") << u"x {a|b}"_s << QChar(u'B') << true << u"{ab}"_s;
    QTest::newRow("i<") << u"a <sp|an> b"_s << QChar(u'<') << false << u"span"_s;
    QTest::newRow("ip") << u"one\n\ntw|o\nthree\n\nfour"_s << QChar(u'p') << false
                        << u"two\nthree\n"_s;
    QTest::newRow("ap, the blank lines after")
        << u"one\n\ntw|o\nthree\n\n\nfour"_s << QChar(u'p') << true << u"two\nthree\n\n\n"_s;
    QTest::newRow("ap at the end, the blank lines before")
        << u"one\n\ntw|o\nthree"_s << QChar(u'p') << true << u"\ntwo\nthree"_s;
}

void TestTextObjects::findsEachObject()
{
    QFETCH(QString, text);
    QFETCH(QChar, kind);
    QFETCH(bool, around);
    QFETCH(QString, covered);
    const int position = int(text.indexOf(u'|'));
    text.remove(position, 1);
    const auto range = textObject(text, position, kind, around);
    QVERIFY(range);
    QCOMPARE(text.mid(range->first, range->second - range->first), covered);
}

void TestTextObjects::findsNothingWhereThereIsNone()
{
    QVERIFY(!textObject(u"no quotes here"_s, 3, u'"', false));
    QVERIFY(!textObject(u"no brackets"_s, 3, u'(', false));
    QVERIFY(!textObject(u"open ( only"_s, 8, u'(', false));
    QVERIFY(!textObject(QString(), 0, u'w', false));
    QVERIFY(!textObject(u"text"_s, 2, u'z', false));
}

QTEST_GUILESS_MAIN(TestTextObjects)
#include "tst_textobjects.moc"
