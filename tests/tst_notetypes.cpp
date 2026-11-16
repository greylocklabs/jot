#include "jot/JsonSchema.h"
#include "jot/NoteTypes.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using namespace jot;
using namespace Qt::StringLiterals;

class TestNoteTypes : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void readsATypesFile();
    void readsEachKindOfProperty();
    void takesWhatATypeExtends();
    void saysWhatIsWrongInAFile();
    void findsATypeInAnyCase();
    void startersReadWithoutProblems();
    void writesStartersOnlyIntoAVaultWithoutTypes();
    void restoresChangedAndMissingStarters();
    void warnsOfValuesATypeDoesNotTake();
    void warnsOfALinkToTheWrongType();
    void warnsOfAnUnknownType();
    void readsLinkValues();
    void knowsTypeFiles();
    void checksATypeFileWithTheOthersBesideIt();
    void ordersANotesProperties();
};

void TestNoteTypes::readsATypesFile()
{
    const NoteTypes types = NoteTypes::fromFiles({{u"character"_s, uR"(name: Character
icon: person
color: Teal
template: Templates/Character.md
properties:
  age: number
  faction: link Group
)"_s},
                                                  {u"group"_s, u"name: Group\n"_s}});
    QCOMPARE(types.all().size(), 2);
    const NoteType *character = types.find(u"Character"_s);
    QVERIFY(character);
    QCOMPARE(character->id, u"character"_s);
    QCOMPARE(character->icon, u"person"_s);
    QCOMPARE(character->color, u"teal"_s);
    QCOMPARE(character->templatePath, u"Templates/Character.md"_s);
    QCOMPARE(character->properties.size(), 2);
    // In the order written, not sorted.
    QCOMPARE(character->properties.at(0).key, u"age"_s);
    QCOMPARE(character->properties.at(1).key, u"faction"_s);
    QCOMPARE(character->properties.at(1).linkType, u"Group"_s);
    QVERIFY(types.problems(u"character"_s).isEmpty());
    // A file without a name is called by its own.
    QCOMPARE(types.find(u"group"_s)->name, u"Group"_s);
}

void TestNoteTypes::readsEachKindOfProperty()
{
    const NoteTypes types = NoteTypes::fromFiles({{u"thing"_s, uR"(properties:
  a: text
  b: number
  c: date
  d: checkbox
  e: url
  f: [one, two]
  g: link
  h: image
  i: list text
  j: list link Story Arc
  k: {kind: choice, choices: [x, y], list: true}
)"_s},
                                                  {u"arc"_s, u"name: Story Arc\n"_s}});
    QVERIFY2(types.problems(u"thing"_s).isEmpty(),
             qPrintable(types.problems(u"thing"_s).value(0).message));
    const NoteType *thing = types.find(u"thing"_s);
    QVERIFY(thing);
    using Kind = TypeProperty::Kind;
    const QList<Kind> kinds{Kind::Text, Kind::Number, Kind::Date,  Kind::Checkbox,
                            Kind::Url,  Kind::Choice, Kind::Link,  Kind::Image,
                            Kind::Text, Kind::Link,   Kind::Choice};
    QCOMPARE(thing->properties.size(), kinds.size());
    for (qsizetype i = 0; i < kinds.size(); ++i)
        QCOMPARE(thing->properties.at(i).kind, kinds.at(i));
    QCOMPARE(thing->property(u"f"_s)->choices, (QStringList{u"one"_s, u"two"_s}));
    QVERIFY(thing->property(u"g"_s)->linkType.isEmpty());
    QVERIFY(thing->property(u"i"_s)->list);
    QVERIFY(thing->property(u"j"_s)->list);
    QCOMPARE(thing->property(u"j"_s)->linkType, u"Story Arc"_s);
    QVERIFY(thing->property(u"k"_s)->list);
    QCOMPARE(thing->property(u"k"_s)->choices, (QStringList{u"x"_s, u"y"_s}));
}

void TestNoteTypes::takesWhatATypeExtends()
{
    const NoteTypes types = NoteTypes::fromFiles({
        {u"entity"_s, u"properties:\n  born: text\n  notes: text\n"_s},
        {u"person"_s, u"extends: entity\nproperties:\n  age: number\n"_s},
        {u"hero"_s, u"name: Hero\nextends: Person\nproperties:\n  notes: number\n"_s},
    });
    const NoteType *hero = types.find(u"hero"_s);
    QVERIFY(hero);
    QStringList keys;
    for (const TypeProperty &property : hero->properties)
        keys.append(property.key);
    QCOMPARE(keys, (QStringList{u"born"_s, u"notes"_s, u"age"_s}));
    // Its own key wins over the one it extends.
    QCOMPARE(hero->property(u"notes"_s)->kind, TypeProperty::Kind::Number);
    QVERIFY(types.isA(u"Hero"_s, u"entity"_s));
    QVERIFY(types.isA(u"hero"_s, u"Hero"_s));
    QVERIFY(!types.isA(u"entity"_s, u"hero"_s));
}

void TestNoteTypes::saysWhatIsWrongInAFile()
{
    const NoteTypes types = NoteTypes::fromFiles({
        {u"a"_s, uR"(name: A
colour: red
color: mauve
extends: Nothing
properties:
  age: nmber
  tags: list text
  kin: link Nobody
  mood: []
)"_s},
        {u"b"_s, u"extends: c\n"_s},
        {u"c"_s, u"extends: b\n"_s},
        {u"d"_s, u"name: [\n"_s},
        {u"e"_s, u"name: A\n"_s},
    });
    QList<int> lines;
    for (const TypeFileProblem &problem : types.problems(u"a"_s))
        lines.append(problem.line);
    std::ranges::sort(lines);
    QCOMPARE(lines, (QList<int>{2, 3, 4, 6, 7, 8, 9}));
    QVERIFY(!types.problems(u"b"_s).isEmpty());
    QVERIFY(types.problems(u"b"_s).first().message.contains(u"around"_s));
    QCOMPARE(types.problems(u"d"_s).size(), 1);
    // The second file with a name already taken is left out.
    QVERIFY(types.problems(u"e"_s).first().message.contains(u"a.yaml"_s));
    QCOMPARE(types.find(u"A"_s)->id, u"a"_s);
    // A property that cannot be read is left out; the rest of the type is still there.
    QVERIFY(types.find(u"A"_s)->property(u"kin"_s));
    QVERIFY(!types.find(u"A"_s)->property(u"age"_s));
}

void TestNoteTypes::findsATypeInAnyCase()
{
    const NoteTypes types = NoteTypes::fromFiles({{u"story-arc"_s, u"name: Story Arc\n"_s}});
    QVERIFY(types.find(u"story arc"_s));
    QVERIFY(types.find(u"STORY-ARC"_s));
    QVERIFY(types.find(u" Story Arc "_s));
    QVERIFY(!types.find(u"Story"_s));
    QVERIFY(!types.find(QString()));
}

void TestNoteTypes::startersReadWithoutProblems()
{
    QHash<QString, QString> files;
    for (const QString &id : NoteTypes::starterIds())
        files.insert(id, NoteTypes::starterText(id));
    const NoteTypes types = NoteTypes::fromFiles(files);
    QCOMPARE(types.all().size(), 7);
    for (const QString &id : NoteTypes::starterIds()) {
        QVERIFY2(types.problems(id).isEmpty(),
                 qPrintable(id + u": "_s + types.problems(id).value(0).message));
        QVERIFY(!types.find(id)->icon.isEmpty());
        QVERIFY(!types.find(id)->color.isEmpty());
    }
}

void TestNoteTypes::writesStartersOnlyIntoAVaultWithoutTypes()
{
    QTemporaryDir dir;
    QString error;
    QVERIFY(NoteTypes::writeStartersIfMissing(dir.path(), &error));
    const QDir types(dir.path() + u"/.jot/types"_s);
    QCOMPARE(types.entryList(QDir::Files).size(), 7);
    QCOMPARE(NoteTypes::read(dir.path()).all().size(), 7);

    // Deleted, they stay deleted.
    QVERIFY(QFile::remove(types.filePath(u"item.yaml"_s)));
    QVERIFY(NoteTypes::writeStartersIfMissing(dir.path(), &error));
    QCOMPARE(types.entryList(QDir::Files).size(), 6);
}

void TestNoteTypes::restoresChangedAndMissingStarters()
{
    QTemporaryDir dir;
    QString error;
    QVERIFY(NoteTypes::writeStartersIfMissing(dir.path(), &error));
    const QDir types(dir.path() + u"/.jot/types"_s);
    QVERIFY(QFile::remove(types.filePath(u"item.yaml"_s)));
    QFile changed(types.filePath(u"event.yaml"_s));
    QVERIFY(changed.open(QIODevice::WriteOnly));
    changed.write("name: Happening\n");
    changed.close();
    QFile own(types.filePath(u"spell.yaml"_s));
    QVERIFY(own.open(QIODevice::WriteOnly));
    own.write("name: Spell\n");
    own.close();

    QVERIFY(NoteTypes::restoreStarters(dir.path(), &error));
    const NoteTypes read = NoteTypes::read(dir.path());
    QVERIFY(read.find(u"Item"_s));
    QVERIFY(read.find(u"Event"_s));
    QVERIFY(!read.find(u"Happening"_s));
    QVERIFY(read.find(u"Spell"_s));
}

void TestNoteTypes::warnsOfValuesATypeDoesNotTake()
{
    const NoteTypes types = NoteTypes::fromFiles({{u"thing"_s, uR"(properties:
  age: number
  born: date
  alive: checkbox
  site: url
  status: [alive, dead]
  names: list text
  one: text
)"_s}});
    const QString note = uR"(---
type: Thing
age: old
born: 2026-13-40
alive: maybe
site: example
status: Alive
names: [a, b]
one: [a, b]
---
Body.
)"_s;
    const QList<FrontMatterProblem> problems = noteTypeProblems(note, types, {});
    QList<int> lines;
    for (const FrontMatterProblem &problem : problems)
        lines.append(problem.line);
    // status matches in any case, and names is a list.
    QCOMPARE(lines, (QList<int>{3, 4, 5, 6, 9}));

    const QString good = uR"(---
type: thing
age: 34.5
born: 2026-11-16
alive: true
site: https://example.org
status: dead
names: one name
---
)"_s;
    QVERIFY(noteTypeProblems(good, types, {}).isEmpty());
    QVERIFY(noteTypeProblems(u"No front matter."_s, types, {}).isEmpty());
    QVERIFY(noteTypeProblems(u"---\ntitle: Untyped\n---\n"_s, types, {}).isEmpty());
}

void TestNoteTypes::warnsOfALinkToTheWrongType()
{
    const NoteTypes types = NoteTypes::fromFiles({
        {u"character"_s, u"properties:\n  faction: link Group\n  friends: list link character\n"_s},
        {u"group"_s, u"name: Group\n"_s},
        {u"guild"_s, u"name: Guild\nextends: group\n"_s},
        {u"location"_s, u"name: Location\n"_s},
    });
    const QHash<QString, QString> typeOf{{u"Ash Court"_s, u"Location"_s},
                                         {u"Smiths"_s, u"guild"_s},
                                         {u"Mara"_s, u"Character"_s},
                                         {u"Untyped"_s, QString()}};
    const auto lookup = [&typeOf](const QString &target) -> std::optional<QString> {
        if (!typeOf.contains(target))
            return std::nullopt;
        return typeOf.value(target);
    };
    const auto problems = [&](const QString &frontMatter) {
        return noteTypeProblems(u"---\ntype: character\n"_s + frontMatter + u"---\n"_s, types,
                                lookup);
    };
    QCOMPARE(problems(u"faction: \"[[Ash Court]]\"\n"_s).size(), 1);
    QVERIFY(problems(u"faction: \"[[Ash Court]]\"\n"_s).first().message.contains(u"Location"_s));
    QCOMPARE(problems(u"faction: \"[[Untyped|them]]\"\n"_s).size(), 1);
    // A guild is a group, and a note not there yet is no problem.
    QVERIFY(problems(u"faction: Smiths\n"_s).isEmpty());
    QVERIFY(problems(u"faction: \"[[Not Written Yet]]\"\n"_s).isEmpty());
    QVERIFY(problems(u"friends:\n  - \"[[Mara]]\"\n"_s).isEmpty());
    QCOMPARE(problems(u"friends:\n  - \"[[Mara]]\"\n  - \"[[Smiths]]\"\n"_s).size(), 1);
}

void TestNoteTypes::warnsOfAnUnknownType()
{
    const NoteTypes types = NoteTypes::fromFiles({{u"character"_s, QString()}});
    const QList<FrontMatterProblem> problems =
        noteTypeProblems(u"---\ntitle: X\ntype: Charcter\n---\n"_s, types, {});
    QCOMPARE(problems.size(), 1);
    QCOMPARE(problems.first().line, 3);
}

void TestNoteTypes::readsLinkValues()
{
    QCOMPARE(linkValueTarget(u"[[Ash Court|the Court]]"_s), u"Ash Court"_s);
    QCOMPARE(linkValueTarget(u"[[Ash Court#History]]"_s), u"Ash Court"_s);
    QCOMPARE(linkValueTarget(u" Ash Court "_s), u"Ash Court"_s);
    QCOMPARE(linkValueTarget(QString()), QString());
}

void TestNoteTypes::knowsTypeFiles()
{
    QVERIFY(isNoteTypeFile(u".jot/types/character.yaml"_s));
    QVERIFY(isNoteTypeFile(u"/home/me/Notes/.jot/types/character.yaml"_s));
    QVERIFY(!isNoteTypeFile(u".jot/layout.yaml"_s));
    QVERIFY(!isNoteTypeFile(u"types/character.yaml"_s));
    QVERIFY(!isNoteTypeFile(u".jot/types/character.md"_s));
}

void TestNoteTypes::checksATypeFileWithTheOthersBesideIt()
{
    QTemporaryDir dir;
    QVERIFY(QDir().mkpath(dir.path() + u"/.jot/types"_s));
    QFile group(dir.path() + u"/.jot/types/group.yaml"_s);
    QVERIFY(group.open(QIODevice::WriteOnly));
    group.write("name: Group\n");
    group.close();
    const QString path = dir.path() + u"/.jot/types/character.yaml"_s;
    // Group is beside it; Nation is not, and age's kind is misspelled.
    const QString text =
        u"name: Character\nproperties:\n  faction: link Group\n  realm: link Nation\n  age: nmber\n"_s;
    const QList<DataProblem> problems = checkDataFile(path, text);
    QCOMPARE(problems.size(), 2);
    QCOMPARE(problems.at(0).line, 4);
    QCOMPARE(problems.at(1).line, 5);
    QVERIFY(checkDataFile(path, u"name: Character\n"_s).isEmpty());
    QCOMPARE(checkDataFile(path, u"name: [\n"_s).size(), 1);
}

void TestNoteTypes::ordersANotesProperties()
{
    const NoteTypes types = NoteTypes::fromFiles(
        {{u"character"_s, u"properties:\n  age: number\n  home: link\n  status: [a, b]\n"_s}});
    const QString note = u"---\nmood: dark\nstatus: a\ntype: Character\nzeal: high\n---\n"_s;
    QStringList keys;
    for (const PropertyRow &row : propertyRows(note, types))
        keys.append(row.key);
    // The type, its properties in its order, left out or not, then the rest as written.
    QCOMPARE(keys,
             (QStringList{u"type"_s, u"age"_s, u"home"_s, u"status"_s, u"mood"_s, u"zeal"_s}));
    const QList<PropertyRow> rows = propertyRows(note, types);
    QVERIFY(!rows.at(1).value.isValid());
    QCOMPARE(rows.at(3).value.toString(), u"a"_s);
    QVERIFY(rows.at(3).property);
    QVERIFY(!rows.at(4).property);
    QVERIFY(propertyRows(u"No front matter."_s, types).isEmpty());

    QCOMPARE(linkValue(u" Ash Court "_s), u"[[Ash Court]]"_s);
    QCOMPARE(linkValue(u"[[Ash Court]]"_s), u"[[Ash Court]]"_s);
    QCOMPARE(linkValue(QString()), QString());
}

QTEST_GUILESS_MAIN(TestNoteTypes)
#include "tst_notetypes.moc"
