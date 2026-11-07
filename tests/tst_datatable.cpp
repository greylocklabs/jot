#include "jot/DataTable.h"

#include <QTest>

using namespace Qt::StringLiterals;
using jot::DataPart;
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
    void keepsJsonLineEndings();
    void readsYamlRecords();
    void refusesYamlThatIsNotRecords_data();
    void refusesYamlThatIsNotRecords();
    void editsYamlKeepingCommentsAndQuotes();
    void addsAndRemovesYamlFieldsAndRecords();
    void editsYamlColumnsAndNestedValues();
    void editsYamlBlockScalarsAndLeavesAnchorsAlone();
    void findsTheListsInAJsonObject();
    void findsTheListsInAYamlMapping();
    void findsTheListsInYamlDocuments();
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
    QTest::newRow("other kind") << u"txt"_s << u"- a: 1\n"_s;
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

    // A record at the very start of a line-broken file finds its indentation.
    DataTable first = *DataTable::read(u"json"_s, u"[{\"a\": 1,\n\"b\": 2}\n]"_s);
    QVERIFY(first.addColumn(u"c"_s));
    QVERIFY(first.setValue(0, 2, {Type::Number, u"3"_s}));
    QVERIFY(first.text().contains(u"\"b\": 2,\n\"c\": 3}"_s));
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
    // Shown to edit as written: big numbers, numbers' zeros, repeated and number-like keys.
    DataTable kept = *DataTable::read(
        u"json"_s, u"[{\"n\": {\"b\": 12345678901234567890, \"2\": 1.50, \"b\": 3}}]"_s);
    QCOMPARE(kept.nestedText(0, 0),
             u"{\n  \"b\": 12345678901234567890,\n  \"2\": 1.50,\n  \"b\": 3\n}"_s);
    QVERIFY(kept.nestedText(0, 1).isEmpty());
    QString error;
    QVERIFY(!table.setValue(0, 2, {Type::Nested, u"[1,"_s}, &error));
    QVERIFY(!error.isEmpty());
    QVERIFY(!table.setValue(0, 2, {Type::Nested, u"3"_s}, &error));
}

void TestDataTable::keepsJsonLineEndings()
{
    QString windows = kExercises;
    windows.replace(u"\n"_s, u"\r\n"_s);
    DataTable table = *DataTable::read(u"json"_s, windows);
    // A new field, a nested value over lines, and a new record all end lines as the file does.
    QVERIFY(table.addColumn(u"tempo"_s));
    QVERIFY(table.setValue(0, 4, {Type::Nested, u"{\"bpm\": 60}"_s}));
    QVERIFY(table.insertRow(2));
    QVERIFY(table.setValue(2, 0, {Type::String, u"Arpeggios"_s}));
    QVERIFY(!table.text().remove(u"\r\n"_s).contains(u'\n'));
    QVERIFY(table.text().contains(u"\"tempo\": {\r\n      \"bpm\": 60\r\n    }"_s));
}

void TestDataTable::keepsACompactFileCompact()
{
    DataTable table = *DataTable::read(u"json"_s, u"[{\"a\":1},{\"a\":2}]"_s);
    QVERIFY(table.addColumn(u"b"_s));
    QVERIFY(table.setValue(0, 1, {Type::Nested, u"{\"c\": [1, 2]}"_s}));
    QVERIFY(table.insertRow(2));
    QCOMPARE(table.text(), u"[{\"a\":1,\"b\":{\"c\":[1,2]}},{\"a\":2},{}]"_s);
}

/// A cast list as people write YAML: comments, quotes, a nested list, a missing field.
static const QString kCast = uR"(# The cast of Ashfall
- name: Mara Vey   # the lead
  role: 'ferry pilot'
  age: 31
  places: [ferry, harbor]
- name: "Söre🎷n"
  age: ~
  lines:
    - Not tonight.
    - Ask the river.
)"_s;

void TestDataTable::readsYamlRecords()
{
    const std::optional<DataTable> table = DataTable::read(u"yaml"_s, kCast);
    QVERIFY(table);
    QCOMPARE(table->columns(),
             (QStringList{u"name"_s, u"role"_s, u"age"_s, u"places"_s, u"lines"_s}));
    QCOMPARE(table->rowCount(), 2);
    QCOMPARE(table->value(0, 0), (DataValue{Type::String, u"Mara Vey"_s}));
    QCOMPARE(table->value(0, 1), (DataValue{Type::String, u"ferry pilot"_s}));
    QCOMPARE(table->value(0, 2), (DataValue{Type::Number, u"31"_s}));
    QCOMPARE(table->value(0, 3), (DataValue{Type::Nested, u"[\"ferry\", \"harbor\"]"_s}));
    QCOMPARE(table->value(1, 0), (DataValue{Type::String, u"Söre🎷n"_s}));
    QCOMPARE(table->value(1, 1).type, Type::Missing);
    QCOMPARE(table->value(1, 2), (DataValue{Type::Null, u"~"_s}));
    QCOMPARE(table->value(1, 4),
             (DataValue{Type::Nested, u"[\"Not tonight.\", \"Ask the river.\"]"_s}));
}

void TestDataTable::refusesYamlThatIsNotRecords_data()
{
    QTest::addColumn<QString>("text");
    QTest::newRow("mapping") << u"name: Mara\n"_s;
    QTest::newRow("list of words") << u"- Mara\n- Soren\n"_s;
    QTest::newRow("two documents") << u"- a: 1\n---\n- a: 2\n"_s;
    QTest::newRow("one line") << u"[{a: 1}]\n"_s;
    QTest::newRow("broken") << u"- a: [1\n"_s;
}

void TestDataTable::refusesYamlThatIsNotRecords()
{
    QFETCH(QString, text);
    QString why;
    QVERIFY(!DataTable::read(u"yaml"_s, text, &why));
    QVERIFY(!why.isEmpty());
}

void TestDataTable::editsYamlKeepingCommentsAndQuotes()
{
    DataTable table = *DataTable::read(u"yaml"_s, kCast);
    // Each value changes where it is written, in its own quotes; comments stay.
    QVERIFY(table.setValue(0, 0, {Type::String, u"Mara"_s}));
    QVERIFY(table.setValue(0, 1, {Type::String, u"the pilot's"_s}));
    QVERIFY(table.setValue(0, 2, {Type::Number, u"32"_s}));
    QVERIFY(table.setValue(1, 0, {Type::String, u"Soren"_s}));
    // A string that reads as something else is quoted; ~ becomes a value.
    QVERIFY(table.setValue(1, 2, {Type::String, u"40"_s}));
    QString expected = kCast;
    expected.replace(u"name: Mara Vey   #"_s, u"name: Mara   #"_s);
    expected.replace(u"'ferry pilot'"_s, u"'the pilot''s'"_s);
    expected.replace(u"age: 31"_s, u"age: 32"_s);
    expected.replace(u"\"Söre🎷n\""_s, u"\"Soren\""_s);
    expected.replace(u"age: ~"_s, u"age: \"40\""_s);
    QCOMPARE(table.text(), expected);
}

void TestDataTable::addsAndRemovesYamlFieldsAndRecords()
{
    DataTable table = *DataTable::read(u"yaml"_s, u"- name: a\n  age: 1\n- name: b\n"_s);
    // A field it lacks goes on a line of its own, after the one before it.
    QVERIFY(table.setValue(1, 1, {Type::Number, u"2"_s}));
    QCOMPARE(table.text(), u"- name: a\n  age: 1\n- name: b\n  age: 2\n"_s);
    // The first field taken out lets the next move up beside the dash.
    QVERIFY(table.clearValue(0, 0));
    QCOMPARE(table.text(), u"- age: 1\n- name: b\n  age: 2\n"_s);
    QVERIFY(table.clearValue(1, 1));
    QCOMPARE(table.text(), u"- age: 1\n- name: b\n"_s);
    // The last one leaves an empty mapping, which a new field fills.
    QVERIFY(table.clearValue(0, 1));
    QCOMPARE(table.text(), u"- {}\n- name: b\n"_s);
    QVERIFY(table.setValue(0, 0, {Type::String, u"c"_s}));
    QCOMPARE(table.text(), u"- name: c\n- name: b\n"_s);

    QVERIFY(table.insertRow(1));
    QCOMPARE(table.text(), u"- name: c\n- {}\n- name: b\n"_s);
    QVERIFY(table.insertRow(3));
    QCOMPARE(table.text(), u"- name: c\n- {}\n- name: b\n- {}\n"_s);
    QVERIFY(table.removeRow(3));
    QVERIFY(table.removeRow(1));
    QCOMPARE(table.text(), u"- name: c\n- name: b\n"_s);
    QVERIFY(table.removeRow(1));
    QVERIFY(table.removeRow(0));
    QCOMPARE(table.text(), u"[]\n"_s);
    QVERIFY(table.insertRow(0));
    QCOMPARE(table.text(), u"- {}\n"_s);
}

void TestDataTable::editsYamlColumnsAndNestedValues()
{
    DataTable table = *DataTable::read(u"yaml"_s, kCast);
    QVERIFY(table.renameColumn(2, u"years"_s));
    QCOMPARE(table.text().count(u"years: "_s), 2);
    QVERIFY(table.removeColumn(1));
    QVERIFY(!table.text().contains(u"ferry pilot"_s));
    QVERIFY(table.text().contains(u"# the lead\n  years: 31\n"_s));
    // A nested value, block or flow, is written in flow form from its colon.
    QVERIFY(table.setValue(1, 3, {Type::Nested, u"[\"Go.\"]"_s}));
    QVERIFY(table.text().endsWith(u"  years: ~\n  lines: [\"Go.\"]\n"_s));
    QCOMPARE(table.value(1, 3), (DataValue{Type::Nested, u"[\"Go.\"]"_s}));
}

void TestDataTable::editsYamlBlockScalarsAndLeavesAnchorsAlone()
{
    const QString text = u"- a: |\n    hello\n    there\n  b: 1\n"
                         u"- a: &x foo\n  b: *x\n  c: !!str 12\n"_s;
    DataTable table = *DataTable::read(u"yaml"_s, text);
    QCOMPARE(table.value(0, 0), (DataValue{Type::String, u"hello\nthere\n"_s}));
    // A block scalar is replaced from its colon, and the field after it stays.
    QVERIFY(table.setValue(0, 0, {Type::String, u"hi"_s}));
    QVERIFY(table.text().startsWith(u"- a: hi\n  b: 1\n"_s));
    DataTable cleared = *DataTable::read(u"yaml"_s, text);
    QVERIFY(cleared.clearValue(0, 0));
    QVERIFY(cleared.text().startsWith(u"- b: 1\n- a: &x foo\n"_s));

    // An anchor, an alias or a tag is left for the text, with a reason.
    QString error;
    QVERIFY(!table.setValue(1, 0, {Type::String, u"bar"_s}, &error));
    QVERIFY(error.contains(u"anchor"_s));
    QVERIFY(!table.setValue(1, 1, {Type::String, u"bar"_s}));
    QVERIFY(!table.setValue(1, 2, {Type::String, u"13"_s}));
    QVERIFY(!table.clearValue(1, 0));
    QVERIFY(table.text().endsWith(u"- a: &x foo\n  b: *x\n  c: !!str 12\n"_s));

    // Windows line endings, and comment lines after a nested value, stay as they are.
    DataTable windows = *DataTable::read(u"yaml"_s, u"- a:\r\n    - x\r\n  # note\r\n  b: 1\r\n"_s);
    QVERIFY(windows.setValue(0, 0, {Type::Nested, u"[\"y\"]"_s}));
    QVERIFY(windows.addColumn(u"c"_s));
    QVERIFY(windows.setValue(0, 2, {Type::Number, u"2"_s}));
    QCOMPARE(windows.text(), u"- a: [\"y\"]\r\n  # note\r\n  b: 1\r\n  c: 2\r\n"_s);
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

    // One column and no newline at the end: the new row still reads as one.
    DataTable single = *DataTable::read(u"csv"_s, u"a\n1"_s);
    QVERIFY(single.insertRow(1));
    QCOMPARE(single.rowCount(), 2);
    QCOMPARE(single.value(1, 0).text, QString());

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

void TestDataTable::findsTheListsInAJsonObject()
{
    const QString text = uR"({
  "title": "Ashfall",
  "cast": [
    {
      "name": "Mara",
      "role": "lead"
    }
  ],
  "tags": ["drama"],
  "crew": []
}
)"_s;
    // Only lists of records get a tab; the title and the list of words do not.
    const QList<DataPart> parts = DataTable::parts(u"json"_s, text);
    QCOMPARE(parts.size(), 2);
    QCOMPARE(parts.at(0).name, u"cast"_s);
    QCOMPARE(parts.at(1).key, u"crew"_s);
    QVERIFY(DataTable::parts(u"json"_s, kExercises).isEmpty());

    std::optional<DataTable> cast = DataTable::read(u"json"_s, text, nullptr, parts.at(0));
    QVERIFY(cast);
    QCOMPARE(cast->rowCount(), 1);
    QCOMPARE(cast->columns(), (QStringList{u"name"_s, u"role"_s}));
    // An edit rewrites only its value, and the table stays the cast's.
    QVERIFY(cast->setValue(0, 1, {Type::String, u"captain"_s}));
    QCOMPARE(cast->text(), QString(text).replace(u"\"lead\""_s, u"\"captain\""_s));
    // A new record and a nested value take the list's depth in the file.
    QVERIFY(cast->insertRow(1));
    QVERIFY(cast->setValue(1, 0, {Type::String, u"Ox"_s}));
    QVERIFY(cast->setValue(0, 1, {Type::Nested, uR"({"ship":"Kestrel"})"_s}));
    QVERIFY2(cast->text().contains(uR"(    {
      "name": "Mara",
      "role": {
        "ship": "Kestrel"
      }
    },
    {
      "name": "Ox"
    }
  ],)"_s),
             qPrintable(cast->text()));
    QVERIFY(cast->text().contains(u"\"title\": \"Ashfall\""_s));

    // An empty list takes its first record.
    std::optional<DataTable> crew = DataTable::read(u"json"_s, cast->text(), nullptr, parts.at(1));
    QVERIFY(crew);
    QCOMPARE(crew->rowCount(), 0);
    QVERIFY(crew->insertRow(0));
    QCOMPARE(crew->rowCount(), 1);
    QVERIFY(DataTable::read(u"json"_s, crew->text(), nullptr, parts.at(0))->rowCount() == 2);
}

void TestDataTable::findsTheListsInAYamlMapping()
{
    const QString text = uR"(# The ferry's people
title: Ashfall
cast:
  - name: Mara # the lead
    role: lead
  - name: Ox
    role: crew
settings:
  season: winter
extras: []
crew:
- name: Rhee
)"_s;
    const QList<DataPart> parts = DataTable::parts(u"yaml"_s, text);
    QStringList names;
    for (const DataPart &part : parts)
        names << part.name;
    QCOMPARE(names, (QStringList{u"cast"_s, u"extras"_s, u"crew"_s}));

    std::optional<DataTable> cast = DataTable::read(u"yaml"_s, text, nullptr, parts.at(0));
    QVERIFY(cast);
    QCOMPARE(cast->rowCount(), 2);
    QVERIFY(cast->setValue(1, 1, {Type::String, u"cook"_s}));
    QCOMPARE(cast->text(), QString(text).replace(u"role: crew"_s, u"role: cook"_s));
    QVERIFY(cast->insertRow(2));
    QVERIFY(cast->setValue(2, 0, {Type::String, u"Pell"_s}));
    QVERIFY2(cast->text().contains(u"    role: cook\n  - name: Pell\nsettings:"_s),
             qPrintable(cast->text()));

    // The empty list's first record goes on a line of its own, under its key.
    std::optional<DataTable> extras =
        DataTable::read(u"yaml"_s, cast->text(), nullptr, parts.at(1));
    QVERIFY(extras);
    QVERIFY(extras->insertRow(0));
    QVERIFY2(extras->text().contains(u"extras:\n  - {}\ncrew:"_s), qPrintable(extras->text()));
    // And removing the last record of a list leaves it empty, not gone.
    std::optional<DataTable> crew =
        DataTable::read(u"yaml"_s, extras->text(), nullptr, parts.at(2));
    QVERIFY(crew);
    QVERIFY(crew->removeRow(0));
    QVERIFY2(crew->text().endsWith(u"crew: []\n"_s), qPrintable(crew->text()));
    QVERIFY(crew->text().startsWith(u"# The ferry's people\ntitle: Ashfall\n"_s));
    QCOMPARE(DataTable::read(u"yaml"_s, crew->text(), nullptr, parts.at(2))->rowCount(), 0);
}

void TestDataTable::findsTheListsInYamlDocuments()
{
    const QString text =
        u"- name: Mara\n---\n# second\n- name: Ox\n- name: Rhee\n---\ntitle: x\n"_s;
    const QList<DataPart> parts = DataTable::parts(u"yaml"_s, text);
    QCOMPARE(parts.size(), 2);
    QCOMPARE(parts.at(0).name, u"Document 1"_s);
    QCOMPARE(parts.at(1).document, 1);
    std::optional<DataTable> second = DataTable::read(u"yaml"_s, text, nullptr, parts.at(1));
    QVERIFY(second);
    QCOMPARE(second->rowCount(), 2);
    QVERIFY(second->removeRow(0));
    QCOMPARE(second->text(), u"- name: Mara\n---\n# second\n- name: Rhee\n---\ntitle: x\n"_s);
    // A part no longer there reads as nothing, with why.
    QString why;
    QVERIFY(!DataTable::read(u"yaml"_s, u"- a: 1\n"_s, &why, parts.at(1)));
    QVERIFY(!why.isEmpty());
}

QTEST_GUILESS_MAIN(TestDataTable)
#include "tst_datatable.moc"
