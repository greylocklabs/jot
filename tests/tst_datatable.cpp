#include "jot/DataTable.h"

#include <QTest>

using namespace Qt::StringLiterals;
using jot::DataTable;
using jot::DataValue;
using Type = DataValue::Type;

/// Woodshed's exercises as they are written: two-space JSON, one record after another.
static const QString kExercises = uR"([
  {
    "name": "Long tones",
    "minutes": 10,
    "families": [],
    "applies_to_tune": false
  },
  {
    "name": "Scales",
    "minutes": 8,
    "families": ["major", "minor"],
    "applies_to_tune": true
  }
]
)"_s;

class TestDataTable : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void readsJsonRecordsWithTheirTypes();
    void refusesWhatIsNotAListOfRecords_data();
    void refusesWhatIsNotAListOfRecords();
    void editsJsonTouchingOnlyTheValue();
    void addsAndRemovesJsonFieldsInPlace();
    void addsAndRemovesJsonRecords();
    void addsRenamesAndRemovesJsonColumns();
    void writesNestedValuesInTheFilesStyle();
    void keepsACompactFileCompact();
    void readsCsvWithQuotesAndLineEndings();
    void editsCsvKeepingQuotingAndShortRows();
    void addsAndRemovesCsvRowsAndColumns();
    void typesWhatIsTyped();
    void flagsStrayValues();
};

void TestDataTable::readsJsonRecordsWithTheirTypes()
{
    const std::optional<DataTable> table = DataTable::read(u"json"_s, kExercises);
    QVERIFY(table);
    QCOMPARE(table->columns(),
             (QStringList{u"name"_s, u"minutes"_s, u"families"_s, u"applies_to_tune"_s}));
    QCOMPARE(table->rowCount(), 2);
    QCOMPARE(table->value(0, 0), (DataValue{Type::String, u"Long tones"_s}));
    QCOMPARE(table->value(0, 1), (DataValue{Type::Number, u"10"_s}));
    QCOMPARE(table->value(0, 2), (DataValue{Type::Nested, u"[]"_s}));
    QCOMPARE(table->value(1, 2), (DataValue{Type::Nested, u"[\"major\", \"minor\"]"_s}));
    QCOMPARE(table->value(1, 3), (DataValue{Type::Bool, u"true"_s}));
}

void TestDataTable::refusesWhatIsNotAListOfRecords_data()
{
    QTest::addColumn<QString>("kind");
    QTest::addColumn<QString>("text");
    QTest::newRow("object") << u"json"_s << u"{\"a\": 1}"_s;
    QTest::newRow("list of numbers") << u"json"_s << u"[1, 2]"_s;
    QTest::newRow("broken") << u"json"_s << u"[{\"a\": }]"_s;
    QTest::newRow("trailing text") << u"json"_s << u"[] x"_s;
    QTest::newRow("unclosed quote") << u"csv"_s << u"a,b\n\"1,2\n"_s;
    QTest::newRow("empty csv") << u"csv"_s << QString();
    QTest::newRow("yaml") << u"yaml"_s << u"- a: 1\n"_s;
}

void TestDataTable::refusesWhatIsNotAListOfRecords()
{
    QFETCH(QString, kind);
    QFETCH(QString, text);
    QString why;
    QVERIFY(!DataTable::read(kind, text, &why));
    QVERIFY(!why.isEmpty());
}

void TestDataTable::editsJsonTouchingOnlyTheValue()
{
    DataTable table = *DataTable::read(u"json"_s, kExercises);
    QVERIFY(table.setValue(0, 1, {Type::Number, u"12"_s}));
    QVERIFY(table.setValue(1, 0, {Type::String, u"Scales \"slow\"\n"_s}));
    QString expected = kExercises;
    expected.replace(u"\"minutes\": 10"_s, u"\"minutes\": 12"_s);
    expected.replace(u"\"Scales\""_s, uR"("Scales \"slow\"\n")"_s);
    QCOMPARE(table.text(), expected);
    QCOMPARE(table.value(1, 0).text, u"Scales \"slow\"\n"_s);
}

void TestDataTable::addsAndRemovesJsonFieldsInPlace()
{
    DataTable table = *DataTable::read(
        u"json"_s, u"[\n  {\"name\": \"a\", \"minutes\": 1},\n  {\"name\": \"b\"}\n]\n"_s);
    // Missing from the second record, it goes in after the field before it in column order.
    QCOMPARE(table.value(1, 1).type, Type::Missing);
    QVERIFY(table.setValue(1, 1, {Type::Number, u"2"_s}));
    // On one line, like the record it joins.
    QCOMPARE(
        table.text(),
        u"[\n  {\"name\": \"a\", \"minutes\": 1},\n  {\"name\": \"b\", \"minutes\": 2}\n]\n"_s);
    QVERIFY(table.clearValue(0, 0));
    QCOMPARE(table.text(), u"[\n  {\"minutes\": 1},\n  {\"name\": \"b\", \"minutes\": 2}\n]\n"_s);
    QVERIFY(table.clearValue(0, 1));
    QCOMPARE(table.text(), u"[\n  {},\n  {\"name\": \"b\", \"minutes\": 2}\n]\n"_s);
    // An empty record opens up for its first field.
    QVERIFY(table.setValue(0, 0, {Type::String, u"c"_s}));
    QCOMPARE(table.text(), u"[\n  {\n    \"name\": \"c\"\n  },\n  {\"name\": \"b\", "
                           u"\"minutes\": 2}\n]\n"_s);
}

void TestDataTable::addsAndRemovesJsonRecords()
{
    DataTable table = *DataTable::read(u"json"_s, kExercises);
    QVERIFY(table.insertRow(2));
    QCOMPARE(table.rowCount(), 3);
    QVERIFY(table.text().endsWith(u"  },\n  {}\n]\n"_s));
    QVERIFY(table.insertRow(0));
    QVERIFY(table.text().startsWith(u"[\n  {},\n  {\n    \"name\": \"Long tones\""_s));
    QVERIFY(table.removeRow(0));
    QVERIFY(table.removeRow(2));
    QCOMPARE(table.text(), kExercises);
    QVERIFY(table.removeRow(1));
    QVERIFY(table.removeRow(0));
    QCOMPARE(table.text(), u"[]\n"_s);
    QVERIFY(table.insertRow(0));
    QCOMPARE(table.rowCount(), 1);
}

void TestDataTable::addsRenamesAndRemovesJsonColumns()
{
    DataTable table = *DataTable::read(u"json"_s, kExercises);
    QString error;
    QVERIFY(!table.addColumn(u"name"_s, &error));
    QVERIFY(!error.isEmpty());
    // A new column is in the table at once, and in a record once it has a value there.
    QVERIFY(table.addColumn(u"tempo"_s));
    QCOMPARE(table.columns().last(), u"tempo"_s);
    QCOMPARE(table.text(), kExercises);
    QVERIFY(table.setValue(1, 4, {Type::Number, u"60"_s}));
    QVERIFY(table.text().contains(u"\"applies_to_tune\": true,\n    \"tempo\": 60\n"_s));
    QCOMPARE(table.columns().size(), 5);

    QVERIFY(table.renameColumn(1, u"length"_s));
    QCOMPARE(table.text().count(u"\"length\": "_s), 2);
    QCOMPARE(table.columns().at(1), u"length"_s);
    QVERIFY(table.removeColumn(4));
    QVERIFY(table.removeColumn(3));
    QVERIFY(!table.text().contains(u"applies_to_tune"_s));
    QVERIFY(table.text().contains(u"\"families\": []\n  },"_s));
}

void TestDataTable::writesNestedValuesInTheFilesStyle()
{
    DataTable table = *DataTable::read(u"json"_s, kExercises);
    QVERIFY(table.setValue(0, 2, {Type::Nested, u"[\"blues\", {\"key\": \"F\"}]"_s}));
    QVERIFY(table.text().contains(
        u"\"families\": [\n      \"blues\",\n      {\n        \"key\": \"F\"\n      }\n    ],"_s));
    QCOMPARE(table.value(0, 2).text, u"[\"blues\", {\"key\": \"F\"}]"_s);
    QString error;
    QVERIFY(!table.setValue(0, 2, {Type::Nested, u"[1,"_s}, &error));
    QVERIFY(!error.isEmpty());
    QVERIFY(!table.setValue(0, 2, {Type::Nested, u"3"_s}, &error));
}

void TestDataTable::keepsACompactFileCompact()
{
    DataTable table = *DataTable::read(u"json"_s, u"[{\"a\":1},{\"a\":2}]"_s);
    QVERIFY(table.addColumn(u"b"_s));
    QVERIFY(table.setValue(0, 1, {Type::Nested, u"{\"c\": [1, 2]}"_s}));
    QVERIFY(table.insertRow(2));
    QCOMPARE(table.text(), u"[{\"a\":1,\"b\":{\"c\":[1,2]}},{\"a\":2},{}]"_s);
}

void TestDataTable::readsCsvWithQuotesAndLineEndings()
{
    const std::optional<DataTable> table = DataTable::read(
        u"csv"_s, u"name,line\r\nMara,\"says \"\"hi\"\",\r\nthen goes\"\r\nSoren\r\n"_s);
    QVERIFY(table);
    QCOMPARE(table->columns(), (QStringList{u"name"_s, u"line"_s}));
    QCOMPARE(table->rowCount(), 2);
    QCOMPARE(table->value(0, 1).text, u"says \"hi\",\r\nthen goes"_s);
    QCOMPARE(table->value(1, 0).text, u"Soren"_s);
    QCOMPARE(table->value(1, 1).type, Type::Missing);

    const std::optional<DataTable> tabs = DataTable::read(u"tsv"_s, u"a\tb\n1\t2"_s);
    QVERIFY(tabs);
    QCOMPARE(tabs->value(0, 1).text, u"2"_s);
}

void TestDataTable::editsCsvKeepingQuotingAndShortRows()
{
    DataTable table = *DataTable::read(u"csv"_s, u"name,line,note\n\"Mara\",hi,x\nSoren\n"_s);
    // A quoted field stays quoted; one that needs quotes gets them.
    QVERIFY(table.setValue(0, 0, {Type::String, u"Mara Vey"_s}));
    QVERIFY(table.setValue(0, 1, {Type::String, u"one, two"_s}));
    QCOMPARE(table.text(), u"name,line,note\n\"Mara Vey\",\"one, two\",x\nSoren\n"_s);
    // A short row gets the empty fields it was missing.
    QVERIFY(table.setValue(1, 2, {Type::String, u"late"_s}));
    QCOMPARE(table.text(), u"name,line,note\n\"Mara Vey\",\"one, two\",x\nSoren,,late\n"_s);
    QVERIFY(table.clearValue(1, 2));
    QCOMPARE(table.value(1, 2).text, QString());
}

void TestDataTable::addsAndRemovesCsvRowsAndColumns()
{
    DataTable table = *DataTable::read(u"csv"_s, u"a,b\r\n1,2\r\n3,4\r\n"_s);
    QVERIFY(table.insertRow(1));
    QCOMPARE(table.text(), u"a,b\r\n1,2\r\n,\r\n3,4\r\n"_s);
    QVERIFY(table.insertRow(3));
    QCOMPARE(table.text(), u"a,b\r\n1,2\r\n,\r\n3,4\r\n,\r\n"_s);
    QVERIFY(table.removeRow(3));
    QVERIFY(table.removeRow(1));
    QCOMPARE(table.text(), u"a,b\r\n1,2\r\n3,4\r\n"_s);

    QVERIFY(table.addColumn(u"c d"_s));
    QCOMPARE(table.text(), u"a,b,c d\r\n1,2,\r\n3,4,\r\n"_s);
    QVERIFY(table.renameColumn(2, u"c,d"_s));
    QCOMPARE(table.text(), u"a,b,\"c,d\"\r\n1,2,\r\n3,4,\r\n"_s);
    QVERIFY(table.removeColumn(0));
    QCOMPARE(table.text(), u"b,\"c,d\"\r\n2,\r\n4,\r\n"_s);
    QVERIFY(table.removeColumn(1));
    QCOMPARE(table.text(), u"b\r\n2\r\n4\r\n"_s);

    // With no newline at the end, the last row takes the one before it.
    DataTable bare = *DataTable::read(u"tsv"_s, u"a\tb\n1\t2"_s);
    QVERIFY(bare.insertRow(1));
    QCOMPARE(bare.text(), u"a\tb\n1\t2\n\t"_s);
    QVERIFY(bare.removeRow(1));
    QCOMPARE(bare.text(), u"a\tb\n1\t2"_s);
}

void TestDataTable::typesWhatIsTyped()
{
    QCOMPARE(DataTable::typed(u" 12.5 "_s, Type::Number), (DataValue{Type::Number, u"12.5"_s}));
    QCOMPARE(DataTable::typed(u"12 apples"_s, Type::Number).type, Type::String);
    QCOMPARE(DataTable::typed(u"false"_s, Type::Bool), (DataValue{Type::Bool, u"false"_s}));
    QCOMPARE(DataTable::typed(u"12"_s, Type::String).type, Type::String);
    QCOMPARE(DataTable::typed(u"null"_s, Type::Null).type, Type::Null);
    QCOMPARE(DataTable::typed(u"[1]"_s, Type::Nested).type, Type::Nested);
}

void TestDataTable::flagsStrayValues()
{
    DataTable json = *DataTable::read(
        u"json"_s, u"[{\"m\": 1}, {\"m\": 2}, {\"m\": \"three\"}, {\"m\": null}, {}]"_s);
    QVERIFY(!json.stray(0, 0));
    QVERIFY(json.stray(2, 0));
    QVERIFY(!json.stray(3, 0));
    QVERIFY(!json.stray(4, 0));
    DataTable csv = *DataTable::read(u"csv"_s, u"m\n1\n2\nthree\n\n"_s);
    QVERIFY(csv.stray(2, 0));
    QVERIFY(!csv.stray(1, 0));
}

QTEST_GUILESS_MAIN(TestDataTable)
#include "tst_datatable.moc"
