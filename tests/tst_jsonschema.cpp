#include "jot/JsonSchema.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTest>

using namespace Qt::StringLiterals;
using jot::SchemaProblem;

namespace {

QJsonValue json(const QByteArray &text)
{
    const QJsonDocument document = QJsonDocument::fromJson(text);
    return document.isArray() ? QJsonValue(document.array()) : QJsonValue(document.object());
}

QStringList messages(const QList<SchemaProblem> &problems)
{
    QStringList found;
    for (const SchemaProblem &each : problems)
        found.append(each.path.join(u'.') + u": "_s + each.message);
    return found;
}

} // namespace

class TestJsonSchema : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void acceptsWhatFits();
    void saysWhereAndWhy_data();
    void saysWhereAndWhy();
    void followsRefsAndCombinations();
    void resolvesPointersIntoTheSchema();
    void readsYamlAndCsvAsJson();
    void findsTheSchemaBesideTheFile();
    void placesProblemsAtTheirLines();
};

static const QByteArray kCastSchema = R"({
  "$defs": {"role": {"enum": ["lead", "chorus"]}},
  "type": "array",
  "items": {
    "type": "object",
    "required": ["name"],
    "additionalProperties": false,
    "properties": {
      "name": {"type": "string", "minLength": 2, "maxLength": 3},
      "age": {"type": "integer", "minimum": 0},
      "role": {"$ref": "#/$defs/role"},
      "code": {"type": "string", "pattern": "^[A-Z]{2}$"},
      "tags": {"type": "array", "items": {"type": "string"}, "uniqueItems": true}
    }
  }
})";

void TestJsonSchema::acceptsWhatFits()
{
    const QJsonValue data =
        json("[{\"name\": \"Mo\", \"age\": 31, \"role\": \"lead\", \"code\": \"MV\", \"tags\": "
             "[\"a\", \"b\"]},\n                 {\"name\": \"🎷🎷\"}]");
    QCOMPARE(messages(jot::checkSchema(json(kCastSchema), data)), QStringList());
    QVERIFY(jot::checkSchema(QJsonValue(true), data).isEmpty());
}

void TestJsonSchema::saysWhereAndWhy_data()
{
    QTest::addColumn<QByteArray>("data");
    QTest::addColumn<QString>("problem");
    QTest::newRow("type") << QByteArray("[{\"name\": \"Mo\", \"age\": \"31\"}]")
                          << u"0.age: should be a whole number"_s;
    QTest::newRow("fraction") << QByteArray("[{\"name\": \"Mo\", \"age\": 3.5}]")
                              << u"0.age: should be a whole number"_s;
    QTest::newRow("minimum") << QByteArray("[{\"name\": \"Mo\", \"age\": -1}]")
                             << u"0.age: should be at least 0"_s;
    QTest::newRow("required") << QByteArray("[{\"age\": 3}]") << u"0.name: is required"_s;
    QTest::newRow("extra") << QByteArray("[{\"name\": \"Mo\", \"aeg\": 3}]")
                           << u"0.aeg: is not a field this may have"_s;
    QTest::newRow("enum") << QByteArray("[{\"name\": \"Mo\", \"role\": \"extra\"}]")
                          << u"0.role: should be one of \"lead\", \"chorus\""_s;
    QTest::newRow("long") << QByteArray("[{\"name\": \"Mara\"}]")
                          << u"0.name: should be at most 3 characters"_s;
    QTest::newRow("pattern") << QByteArray("[{\"name\": \"Mo\", \"code\": \"mv\"}]")
                             << u"0.code: should match ^[A-Z]{2}$"_s;
    QTest::newRow("unique") << QByteArray("[{\"name\": \"Mo\", \"tags\": [\"a\", \"a\"]}]")
                            << u"0.tags.1: is the same as item 1"_s;
    QTest::newRow("not a list") << QByteArray("{\"name\": \"Mo\"}") << u": should be a list"_s;
}

void TestJsonSchema::saysWhereAndWhy()
{
    QFETCH(QByteArray, data);
    QFETCH(QString, problem);
    QCOMPARE(messages(jot::checkSchema(json(kCastSchema), json(data))), QStringList{problem});
}

void TestJsonSchema::followsRefsAndCombinations()
{
    const QJsonValue schema =
        json("{\n        \"anyOf\": [{\"type\": \"string\"}, {\"type\": \"number\"}],\n        "
             "\"oneOf\": [{\"minimum\": 0}, {\"maximum\": 10}],\n        \"not\": {\"const\": "
             "5},\n        \"if\": {\"type\": \"number\"}, \"then\": {\"multipleOf\": 2}\n    }");
    QVERIFY(jot::checkSchema(schema, 12).isEmpty());
    QCOMPARE(messages(jot::checkSchema(schema, 4)),
             QStringList{u": matches more than one of the shapes it may take"_s});
    // Number bounds leave a boolean alone, so it fits both of oneOf's shapes.
    QCOMPARE(messages(jot::checkSchema(schema, true)),
             (QStringList{u": matches none of the shapes it may take"_s,
                          u": matches more than one of the shapes it may take"_s}));
    QCOMPARE(messages(jot::checkSchema(schema, 13)), QStringList{u": should be a multiple of 2"_s});
    QVERIFY(messages(jot::checkSchema(json("{\"$ref\": \"#/$defs/nope\"}"), 1))
                .first()
                .contains(u"#/$defs/nope"_s));
}

void TestJsonSchema::resolvesPointersIntoTheSchema()
{
    const QJsonValue schema =
        json("{\"$defs\": {\"a/b\": {\"enum\": [1]}, \"shapes\": [{\"type\": \"string\"}]}}");
    QCOMPARE(jot::resolveSchemaRef(schema, u"#"_s), schema);
    QCOMPARE(jot::resolveSchemaRef(schema, u"#/$defs/a~1b"_s), json("{\"enum\": [1]}"));
    QCOMPARE(jot::resolveSchemaRef(schema, u"#/$defs/shapes/0"_s), json("{\"type\": \"string\"}"));
    QVERIFY(!jot::resolveSchemaRef(schema, u"#/$defs/shapes/1"_s));
    QVERIFY(!jot::resolveSchemaRef(schema, u"#/$defs/shapes/-1"_s));
    QVERIFY(!jot::resolveSchemaRef(schema, u"#/$defs/none"_s));
    QVERIFY(!jot::resolveSchemaRef(schema, u"other.json#/x"_s));
    QCOMPARE(jot::jsonText(QJsonValue(u"Mo"_s)), u"\"Mo\""_s);
    QCOMPARE(jot::jsonText(QJsonValue(2.5)), u"2.5"_s);
    QCOMPARE(jot::jsonText(json("[1, true]")), u"[1,true]"_s);
}

void TestJsonSchema::readsYamlAndCsvAsJson()
{
    QCOMPARE(jot::dataAsJson(u"yaml"_s, u"- name: Mo\n  age: 31\n  lead: true\n  note: ~\n"_s),
             json("[{\"name\": \"Mo\", \"age\": 31, \"lead\": true, \"note\": null}]"));
    QCOMPARE(jot::dataAsJson(u"yaml"_s, u"- name: '31'\n"_s), json("[{\"name\": \"31\"}]"));
    QCOMPARE(jot::dataAsJson(u"csv"_s, u"name,age,lead,note\nMo,31,true,\n"_s),
             json("[{\"name\": \"Mo\", \"age\": 31, \"lead\": true}]"));
    // Aliases stand for what their anchor named, and merge keys bring a mapping's fields in,
    // under the fields written beside them; a quoted << is only a key.
    QCOMPARE(jot::dataAsJson(u"yaml"_s, u"- &base {name: Mo, role: lead}\n"
                                        "- {<<: *base, role: chorus}\n"
                                        "- {<<: [*base, {age: 3}], name: Al}\n"
                                        "- {owner: *base, '<<': x}\n"_s),
             json("[{\"name\": \"Mo\", \"role\": \"lead\"},"
                  " {\"name\": \"Mo\", \"role\": \"chorus\"},"
                  " {\"name\": \"Al\", \"role\": \"lead\", \"age\": 3},"
                  " {\"owner\": {\"name\": \"Mo\", \"role\": \"lead\"}, \"<<\": \"x\"}]"));
    QString error;
    QVERIFY(!jot::dataAsJson(u"json"_s, u"[1,"_s, &error));
    QVERIFY(!error.isEmpty());
}

void TestJsonSchema::findsTheSchemaBesideTheFile()
{
    QTemporaryDir dir;
    const auto write = [&dir](const QString &name, const QByteArray &text) {
        QFile file(dir.filePath(name));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(text);
    };
    write(u"cast.json"_s, "[]");
    QVERIFY(jot::schemaPathFor(dir.filePath(u"cast.json"_s), json("[]")).isEmpty());
    write(u"cast.schema.json"_s, "{}");
    QCOMPARE(jot::schemaPathFor(dir.filePath(u"cast.json"_s), json("[]")),
             dir.filePath(u"cast.schema.json"_s));
    QCOMPARE(jot::schemaPathFor(dir.filePath(u"cast.yaml"_s), json("[]")),
             dir.filePath(u"cast.schema.json"_s));
    // A file $schema names beside it comes first; one on the web is never fetched.
    write(u"people.json"_s, "{}");
    QCOMPARE(
        jot::schemaPathFor(dir.filePath(u"cast.json"_s), json("{\"$schema\": \"people.json\"}")),
        dir.filePath(u"people.json"_s));
    QCOMPARE(jot::schemaPathFor(dir.filePath(u"cast.json"_s),
                                json("{\"$schema\": \"https://example.org/s.json\"}")),
             dir.filePath(u"cast.schema.json"_s));
}

void TestJsonSchema::placesProblemsAtTheirLines()
{
    QTemporaryDir dir;
    QFile schema(dir.filePath(u"cast.schema.json"_s));
    QVERIFY(schema.open(QIODevice::WriteOnly));
    schema.write(kCastSchema);
    schema.close();
    const QString text =
        u"[\n  {\"name\": \"Mo\"},\n  {\"name\": \"Mo\",\n   \"age\": \"x\"}\n]\n"_s;
    const QList<jot::DataProblem> problems = jot::checkDataFile(dir.filePath(u"cast.json"_s), text);
    QCOMPARE(problems.size(), 1);
    QCOMPARE(problems.first().line, 4);
    QCOMPARE(problems.first().column, 11);
    QCOMPARE(problems.first().message, u"age: should be a whole number"_s);
    // A missing field is placed at its record.
    const QList<jot::DataProblem> missing =
        jot::checkDataFile(dir.filePath(u"cast.yaml"_s), u"- name: Mo\n- age: 3\n"_s);
    QCOMPARE(missing.size(), 1);
    QCOMPARE(missing.first().line, 2);
    QCOMPARE(missing.first().message, u"name: is required"_s);
    // A merge key fills a required field, and is no field of its own.
    QVERIFY(jot::checkDataFile(dir.filePath(u"cast.yaml"_s),
                               u"- &mo {name: Mo}\n- {<<: *mo, role: lead}\n- *mo\n"_s)
                .isEmpty());
    // A file that does not read says so before anything else.
    QCOMPARE(jot::checkDataFile(dir.filePath(u"cast.json"_s), u"[1,"_s).size(), 1);
}

QTEST_GUILESS_MAIN(TestJsonSchema)
#include "tst_jsonschema.moc"
