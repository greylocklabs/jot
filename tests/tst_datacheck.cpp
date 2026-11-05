#include "jot/DataCheck.h"

#include <QTest>

using namespace Qt::StringLiterals;
using jot::checkData;
using jot::DataProblem;

class TestDataCheck : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void readsWellFormedFiles_data();
    void readsWellFormedFiles();
    void saysWhereAFileStopsReading_data();
    void saysWhereAFileStopsReading();
    void placesAProblemAtTheFirstCharacter();
    void namesKindsByExtension();
};

void TestDataCheck::readsWellFormedFiles_data()
{
    QTest::addColumn<QString>("kind");
    QTest::addColumn<QString>("text");
    QTest::newRow("json") << u"json"_s
                          << u"[{\"name\": \"Mara\", \"age\": 31, \"lead\": true}]\n"_s;
    QTest::newRow("yaml") << u"yaml"_s
                          << u"# cast\n- name: Mara's ferry\n  roles: [lead, \"pilot\"]\n"_s;
    QTest::newRow("csv") << u"csv"_s << u"name,line\nMara,\"says \"\"hi\"\",\nthen goes\"\n"_s;
    QTest::newRow("tsv") << u"tsv"_s << u"name\tline\nMara\t\"one\ttab\"\n"_s;
    QTest::newRow("empty csv") << u"csv"_s << QString();
    QTest::newRow("other kind") << u"txt"_s << u"{"_s;
}

void TestDataCheck::readsWellFormedFiles()
{
    QFETCH(QString, kind);
    QFETCH(QString, text);
    const DataProblem problem = checkData(kind, text);
    QVERIFY2(problem.ok(), qPrintable(problem.message));
}

void TestDataCheck::saysWhereAFileStopsReading_data()
{
    QTest::addColumn<QString>("kind");
    QTest::addColumn<QString>("text");
    QTest::addColumn<int>("line");
    QTest::addColumn<int>("column");
    QTest::addColumn<int>("position");
    // Qt places it just past the stray brace; columns count characters, so the accent before it
    // counts once.
    QTest::newRow("json") << u"json"_s << u"[\n  {\"name\": \"Zoë\",}\n]\n"_s << 2 << 19 << 20;
    QTest::newRow("yaml") << u"yaml"_s << u"name: Mara\n  bad: [indent\n"_s << 2 << 6 << 16;
    // An emoji is two UTF-16 units but one character, for YAML and JSON alike.
    QTest::newRow("yaml emoji") << u"yaml"_s << u"- \U0001F3B7: [a\n"_s << 2 << 1 << 9;
    QTest::newRow("json emoji") << u"json"_s << u"[\"\U0001F3B7\" 1]"_s << 1 << 7 << 7;
    QTest::newRow("unclosed quote") << u"csv"_s << u"a,b\n1,\"two\n3,4\n"_s << 2 << 3 << 6;
    QTest::newRow("text after quote") << u"csv"_s << u"a,b\n\"one\"x,2\n"_s << 2 << 6 << 9;
    QTest::newRow("tsv") << u"tsv"_s << u"a\tb\n1\t\"open\n"_s << 2 << 3 << 6;
}

void TestDataCheck::saysWhereAFileStopsReading()
{
    QFETCH(QString, kind);
    QFETCH(QString, text);
    QFETCH(int, line);
    QFETCH(int, column);
    QFETCH(int, position);
    const DataProblem problem = checkData(kind, text);
    QVERIFY(!problem.ok());
    QVERIFY(!problem.message.isEmpty());
    QCOMPARE(problem.line, line);
    QCOMPARE(problem.column, column);
    QCOMPARE(problem.position, position);
}

void TestDataCheck::placesAProblemAtTheFirstCharacter()
{
    // Qt versions differ on whether the offset is at the stray character or just past it.
    const DataProblem problem = checkData(u"json"_s, u"# not json\n[]\n"_s);
    QVERIFY(!problem.ok());
    QCOMPARE(problem.line, 1);
    QVERIFY(problem.position <= 1);
    QCOMPARE(problem.column, problem.position + 1);
}

void TestDataCheck::namesKindsByExtension()
{
    QCOMPARE(jot::dataKindOf(u"a/b.JSON"_s), u"json"_s);
    QCOMPARE(jot::dataKindOf(u"config.yml"_s), u"yaml"_s);
    QCOMPARE(jot::dataKindOf(u"rows.tsv"_s), u"tsv"_s);
    QCOMPARE(jot::dataKindOf(u"Mara.md"_s), QString());
}

QTEST_GUILESS_MAIN(TestDataCheck)
#include "tst_datacheck.moc"
