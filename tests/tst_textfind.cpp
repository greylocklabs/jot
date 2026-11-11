#include "jot/TextFind.h"

#include <QTest>

using namespace jot;
using namespace Qt::StringLiterals;

class TestTextFind : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void findsEveryMatch_data();
    void findsEveryMatch();
};

void TestTextFind::findsEveryMatch_data()
{
    QTest::addColumn<QString>("text");
    QTest::addColumn<QString>("needle");
    QTest::addColumn<bool>("matchCase");
    QTest::addColumn<bool>("wholeWords");
    QTest::addColumn<QList<qsizetype>>("found");

    const QString ferry = u"Ferry at dawn. The ferryman ferries; FERRY!"_s;
    QTest::newRow("case aside") << ferry << u"ferry"_s << false << false
                                << QList<qsizetype>{0, 19, 37};
    QTest::newRow("matching case") << ferry << u"ferry"_s << true << false << QList<qsizetype>{19};
    QTest::newRow("whole words") << ferry << u"ferry"_s << false << true << QList<qsizetype>{0, 37};
    QTest::newRow("whole words and case")
        << ferry << u"Ferry"_s << true << true << QList<qsizetype>{0};
    QTest::newRow("no overlaps") << u"aaaa"_s << u"aa"_s << false << false
                                 << QList<qsizetype>{0, 2};
    QTest::newRow("underscore joins a word")
        << u"snake_case case"_s << u"case"_s << false << true << QList<qsizetype>{11};
    QTest::newRow("across lines") << u"one\ntwo\none"_s << u"one"_s << false << true
                                  << QList<qsizetype>{0, 8};
    QTest::newRow("accents are letters")
        << u"café cafés"_s << u"café"_s << false << true << QList<qsizetype>{0};
    QTest::newRow("empty needle") << ferry << QString() << false << false << QList<qsizetype>{};
    QTest::newRow("not there") << ferry << u"raft"_s << false << false << QList<qsizetype>{};
}

void TestTextFind::findsEveryMatch()
{
    QFETCH(QString, text);
    QFETCH(QString, needle);
    QFETCH(bool, matchCase);
    QFETCH(bool, wholeWords);
    QFETCH(QList<qsizetype>, found);
    QCOMPARE(findAll(text, needle, matchCase, wholeWords), found);
}

QTEST_GUILESS_MAIN(TestTextFind)
#include "tst_textfind.moc"
