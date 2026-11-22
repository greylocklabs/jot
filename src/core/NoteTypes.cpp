#include "jot/NoteTypes.h"

#include <QDate>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QObject>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QUrl>

#include <yaml.h>

#include <algorithm>

using namespace Qt::StringLiterals;

namespace jot {

namespace {

/// A YAML value as a type file holds it, with its keys in the order written.
struct Node
{
    enum class Kind { Scalar, Map, List };
    Kind kind = Kind::Scalar;
    QString text;
    /// From 1.
    int line = 0;
    QList<std::pair<QString, Node>> members;
    QList<Node> items;
};

/// Reads `text` as one YAML document, or nothing with `error` and `errorLine` set.
std::optional<Node> readYaml(const QString &text, QString *error, int *errorLine)
{
    const QByteArray bytes = text.toUtf8();
    yaml_parser_t parser;
    if (!yaml_parser_initialize(&parser)) {
        *error = QObject::tr("YAML could not be read");
        *errorLine = 1;
        return std::nullopt;
    }
    yaml_parser_set_input_string(&parser, reinterpret_cast<const unsigned char *>(bytes.data()),
                                 size_t(bytes.size()));
    QList<Node> open;
    // For each open map, the key waiting for its value, or null.
    QList<QString> keys;
    std::optional<Node> done;
    const auto add = [&](Node node) {
        if (open.isEmpty()) {
            done = std::move(node);
            return;
        }
        Node &parent = open.last();
        if (parent.kind == Node::Kind::List) {
            parent.items.append(std::move(node));
        } else if (keys.last().isNull()) {
            keys.last() = node.text;
        } else {
            parent.members.append({keys.last(), std::move(node)});
            keys.last() = QString();
        }
    };
    for (;;) {
        yaml_event_t event;
        if (!yaml_parser_parse(&parser, &event)) {
            *error = QString::fromUtf8(parser.problem ? parser.problem : "not valid YAML");
            *errorLine = int(parser.problem_mark.line) + 1;
            yaml_parser_delete(&parser);
            return std::nullopt;
        }
        const int line = int(event.start_mark.line) + 1;
        const yaml_event_type_t type = event.type;
        switch (type) {
        case YAML_SCALAR_EVENT:
            add({Node::Kind::Scalar,
                 QString::fromUtf8(reinterpret_cast<const char *>(event.data.scalar.value),
                                   qsizetype(event.data.scalar.length)),
                 line,
                 {},
                 {}});
            break;
        case YAML_MAPPING_START_EVENT:
            open.append({Node::Kind::Map, {}, line, {}, {}});
            keys.append(QString());
            break;
        case YAML_SEQUENCE_START_EVENT:
            open.append({Node::Kind::List, {}, line, {}, {}});
            keys.append(QString());
            break;
        case YAML_MAPPING_END_EVENT:
        case YAML_SEQUENCE_END_EVENT: {
            Node node = open.takeLast();
            keys.removeLast();
            add(std::move(node));
            break;
        }
        default: break;
        }
        yaml_event_delete(&event);
        if (type == YAML_STREAM_END_EVENT || type == YAML_DOCUMENT_END_EVENT)
            break;
    }
    yaml_parser_delete(&parser);
    if (!done)
        return Node{Node::Kind::Map, {}, 1, {}, {}};
    return done;
}

const QStringList kKindNames{u"text"_s, u"number"_s, u"date"_s, u"checkbox"_s,
                             u"url"_s,  u"choice"_s, u"link"_s, u"image"_s};

/// Keys Jot reads in every note, which a type cannot give another meaning.
const QStringList kReservedKeys{u"title"_s, u"tags"_s,   u"aliases"_s, u"created"_s,
                                u"type"_s,  u"target"_s, u"lang"_s,    u"tasks"_s};

/// A property as `spec` describes it: "number", "list text", "link Group", a list of choices,
/// or a map of `kind`, `list`, `choices` and `type`. Nothing, with `problem` set, when it says
/// nothing Jot can use.
std::optional<TypeProperty> readProperty(const QString &key, const Node &spec, QString *problem)
{
    TypeProperty property;
    property.key = key;
    if (spec.kind == Node::Kind::List) {
        property.kind = TypeProperty::Kind::Choice;
        for (const Node &item : spec.items)
            property.choices.append(item.text);
        // Kept, so a choice being set up keeps its kind until its choices are filled in.
        if (property.choices.isEmpty())
            *problem = QObject::tr("%1 has no choices").arg(key);
        return property;
    }
    QString words = spec.text.simplified();
    if (spec.kind == Node::Kind::Map) {
        words.clear();
        for (const auto &[name, value] : spec.members) {
            if (name == u"kind") {
                words.prepend(value.text.simplified() + u' ');
            } else if (name == u"list") {
                property.list = value.text.toLower() == u"true";
            } else if (name == u"choices") {
                for (const Node &item : value.items)
                    property.choices.append(item.text);
            } else if (name == u"type") {
                property.linkType = value.text.trimmed();
            } else {
                *problem = QObject::tr("%1 has %2, which is not kind, list, choices or type")
                               .arg(key, name);
                return std::nullopt;
            }
        }
        words = words.trimmed();
        if (words.isEmpty() && !property.choices.isEmpty())
            words = u"choice"_s;
    }
    if (words.startsWith(u"list ")) {
        property.list = true;
        words = words.mid(5);
    }
    const QString kind = words.section(u' ', 0, 0).toLower();
    const QString rest = words.section(u' ', 1).trimmed();
    const qsizetype index = kKindNames.indexOf(kind);
    if (index < 0) {
        *problem =
            QObject::tr("%1 is %2, which is not a kind: %3")
                .arg(key, words.isEmpty() ? QObject::tr("empty") : words, kKindNames.join(u", "_s));
        return std::nullopt;
    }
    property.kind = TypeProperty::Kind(index);
    if (property.kind == TypeProperty::Kind::Link && !rest.isEmpty())
        property.linkType = rest;
    else if (!rest.isEmpty()) {
        *problem =
            QObject::tr("%1 is %2, with %3 after it, which only a link takes").arg(key, kind, rest);
        return std::nullopt;
    }
    if (property.kind == TypeProperty::Kind::Choice && property.choices.isEmpty())
        *problem = QObject::tr("%1 has no choices").arg(key);
    return property;
}

/// A type as its file says, without what it extends, and what is wrong in the file.
NoteType readType(const QString &id, const QString &text, QList<TypeFileProblem> *problems,
                  QHash<QString, int> *lines)
{
    NoteType type;
    type.id = id;
    type.name = id;
    QString error;
    int errorLine = 0;
    const std::optional<Node> root = readYaml(text, &error, &errorLine);
    if (!root) {
        problems->append({errorLine, error});
        return type;
    }
    if (root->kind != Node::Kind::Map) {
        problems->append(
            {root->line, QObject::tr("A type is keys and values, as name: Character")});
        return type;
    }
    static const QRegularExpression hex(u"^#[0-9a-fA-F]{6}$"_s);
    for (const auto &[key, value] : root->members) {
        const bool scalar = value.kind == Node::Kind::Scalar;
        lines->insert(key, value.line);
        if (key == u"name" && scalar && !value.text.trimmed().isEmpty()) {
            type.name = value.text.trimmed();
        } else if (key == u"icon" && scalar) {
            type.icon = value.text.trimmed();
        } else if (key == u"color" && scalar) {
            const QString color = value.text.trimmed();
            if (colorNames().contains(color.toLower()) || hex.match(color).hasMatch())
                type.color = color.toLower();
            else
                problems->append(
                    {value.line, QObject::tr("color is one of %1, or as #2a9d8f, not %2")
                                     .arg(colorNames().join(u", "_s), color)});
        } else if (key == u"extends" && scalar) {
            type.extends = value.text.trimmed();
        } else if (key == u"template" && scalar) {
            type.templatePath = value.text.trimmed();
        } else if (key == u"properties" && value.kind == Node::Kind::Map) {
            for (const auto &[name, spec] : value.members) {
                lines->insert(u"properties."_s + name, spec.line);
                if (kReservedKeys.contains(name)) {
                    problems->append(
                        {spec.line, QObject::tr("%1 is a key Jot reads in every note").arg(name)});
                    continue;
                }
                QString problem;
                if (const std::optional<TypeProperty> property = readProperty(name, spec, &problem))
                    type.properties.append(*property);
                if (!problem.isEmpty())
                    problems->append({spec.line, problem});
            }
        } else if (key == u"properties" && scalar && value.text.isEmpty()) {
            // `properties:` with nothing under it yet.
        } else if (QStringList{u"name"_s, u"icon"_s, u"color"_s, u"extends"_s, u"template"_s,
                               u"properties"_s}
                       .contains(key)) {
            problems->append({value.line, key == u"properties"
                                              ? QObject::tr("properties is keys and kinds, as "
                                                            "age: number")
                                              : QObject::tr("%1 is one value").arg(key)});
        } else {
            problems->append(
                {value.line,
                 QObject::tr("%1 is not name, icon, color, extends, template or properties")
                     .arg(key)});
        }
    }
    return type;
}

/// The starter types, by id, as Jot writes them.
const QList<std::pair<QString, QString>> &starters()
{
    static const QList<std::pair<QString, QString>> list{
        {u"character"_s, uR"(name: Character
icon: person
color: teal
properties:
  role: [protagonist, antagonist, supporting, minor]
  status: [alive, dead, unknown]
  born: text
  home: link Location
  groups: list link Group
  portrait: image
)"_s},
        {u"location"_s, uR"(name: Location
icon: pin
color: green
properties:
  region: link Location
  population: number
  map: image
)"_s},
        {u"group"_s, uR"(name: Group
icon: people
color: purple
properties:
  leader: link Character
  base: link Location
  founded: text
)"_s},
        {u"item"_s, uR"(name: Item
icon: box
color: orange
properties:
  owner: link Character
  origin: link Location
  picture: image
)"_s},
        {u"scene"_s, uR"(name: Scene
icon: video
color: blue
properties:
  status: [idea, draft, revised, done]
  chapter: link Chapter
  pov: link Character
  location: link Location
  characters: list link Character
)"_s},
        {u"chapter"_s, uR"(name: Chapter
icon: note
color: yellow
properties:
  number: number
  status: [idea, draft, revised, done]
)"_s},
        {u"event"_s, uR"(name: Event
icon: flag
color: red
properties:
  when: text
  where: link Location
  who: list link Character
)"_s},
    };
    return list;
}

bool writeFile(const QString &path, const QString &text, QString *error)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(text.toUtf8()) < 0 || !file.commit()) {
        if (error)
            *error = QObject::tr("Could not write %1: %2").arg(path, file.errorString());
        return false;
    }
    return true;
}

/// Where each top-level key of the note's front matter is, by its line, counted from 1.
QHash<QString, int> keyLines(QStringView note)
{
    QHash<QString, int> lines;
    const NoteParts parts = splitFrontMatter(note);
    if (!parts.hasFrontMatter)
        return lines;
    // The front matter's first line comes after the opening fence.
    int number = int(note.first(parts.frontMatter.data() - note.data()).count(u'\n')) + 1;
    static const QRegularExpression key(u"^([^\\s#:-][^:]*?)\\s*:(\\s|$)"_s);
    for (const QStringView line : parts.frontMatter.tokenize(u'\n')) {
        if (const QRegularExpressionMatch match = key.matchView(line); match.hasMatch())
            lines.insert(match.captured(1), number);
        ++number;
    }
    return lines;
}

/// Why `value` is no value of `property`'s kind, or empty when it is one.
QString wrongValue(const TypeProperty &property, const QString &value, const NoteTypes &types,
                   const std::function<std::optional<QString>(const QString &)> &typeOfLink)
{
    const QString &key = property.key;
    switch (property.kind) {
    case TypeProperty::Kind::Text:
    case TypeProperty::Kind::Image: return {};
    case TypeProperty::Kind::Number: {
        bool number = false;
        value.toDouble(&number);
        return number ? QString() : QObject::tr("%1 is a number, not %2").arg(key, value);
    }
    case TypeProperty::Kind::Date:
        return QDate::fromString(value, Qt::ISODate).isValid() ||
                       QDateTime::fromString(value, Qt::ISODate).isValid()
                   ? QString()
                   : QObject::tr("%1 is a date, as 2026-11-16, not %2").arg(key, value);
    case TypeProperty::Kind::Checkbox: {
        static const QStringList yesNo{u"true"_s, u"false"_s, u"yes"_s, u"no"_s};
        return yesNo.contains(value.toLower())
                   ? QString()
                   : QObject::tr("%1 is true or false, not %2").arg(key, value);
    }
    case TypeProperty::Kind::Url: {
        const QUrl url(value, QUrl::StrictMode);
        return url.isValid() && !url.scheme().isEmpty()
                   ? QString()
                   : QObject::tr("%1 is a web address, as https://example.org, not %2")
                         .arg(key, value);
    }
    case TypeProperty::Kind::Choice:
        for (const QString &choice : property.choices) {
            if (choice.compare(value, Qt::CaseInsensitive) == 0)
                return {};
        }
        return QObject::tr("%1 is one of %2, not %3")
            .arg(key, property.choices.join(u", "_s), value);
    case TypeProperty::Kind::Link: {
        const QString target = linkValueTarget(value);
        if (property.linkType.isEmpty() || target.isEmpty() || !typeOfLink)
            return {};
        const std::optional<QString> type = typeOfLink(target);
        if (!type || types.isA(*type, property.linkType))
            return {};
        const NoteType *wanted = types.find(property.linkType);
        const QString wantedName = wanted ? wanted->name : property.linkType;
        return type->isEmpty() ? QObject::tr("%1 links to a %2, and %3 has no type")
                                     .arg(key, wantedName, target)
                               : QObject::tr("%1 links to a %2, and %3 is a %4")
                                     .arg(key, wantedName, target, *type);
    }
    }
    return {};
}

} // namespace

QString kindName(TypeProperty::Kind kind)
{
    return kKindNames.value(int(kind));
}

const TypeProperty *NoteType::property(const QString &key) const
{
    for (const TypeProperty &each : properties) {
        if (each.key == key)
            return &each;
    }
    return nullptr;
}

QStringList colorNames()
{
    return {u"red"_s,  u"orange"_s, u"yellow"_s, u"green"_s, u"teal"_s,
            u"blue"_s, u"purple"_s, u"pink"_s,   u"gray"_s};
}

bool isNoteTypeFile(const QString &path)
{
    const QFileInfo info(path);
    return QDir::cleanPath(info.path()).endsWith(NoteTypes::kFolder) && info.suffix() == u"yaml";
}

NoteTypes NoteTypes::read(const QString &root)
{
    QHash<QString, QString> files;
    const QDir dir(root + u'/' + kFolder);
    for (const QFileInfo &info : dir.entryInfoList({u"*.yaml"_s}, QDir::Files)) {
        QFile file(info.filePath());
        if (file.open(QIODevice::ReadOnly))
            files.insert(info.completeBaseName(), QString::fromUtf8(file.readAll()));
    }
    return fromFiles(files);
}

NoteTypes NoteTypes::fromFiles(const QHash<QString, QString> &files)
{
    NoteTypes types;
    QHash<QString, NoteType> own;
    QHash<QString, QHash<QString, int>> lines;
    for (auto it = files.constBegin(); it != files.constEnd(); ++it) {
        QList<TypeFileProblem> problems;
        own.insert(it.key(), readType(it.key(), it.value(), &problems, &lines[it.key()]));
        if (!problems.isEmpty())
            types.m_problems.insert(it.key(), problems);
    }
    // Two files naming the same type: the first by id keeps the name.
    QStringList ids = own.keys();
    std::ranges::sort(ids);
    QHash<QString, QString> byName;
    for (const QString &id : std::as_const(ids)) {
        const QString name = own.value(id).name.toLower();
        if (byName.contains(name) && byName.value(name) != id) {
            types.m_problems[id].append({lines.value(id).value(u"name"_s, 1),
                                         QObject::tr("%1 is also the name of %2.yaml")
                                             .arg(own.value(id).name, byName.value(name))});
            own.remove(id);
            continue;
        }
        byName.insert(name, id);
        byName.insert(id.toLower(), id);
    }
    // Each type takes the properties of those it extends, theirs first.
    for (const QString &id : std::as_const(ids)) {
        if (!own.contains(id))
            continue;
        NoteType type = own.value(id);
        QList<TypeProperty> inherited;
        QSet<QString> seen{id};
        QString base = type.extends;
        while (!base.isEmpty()) {
            const QString baseId = byName.value(base.toLower());
            if (baseId.isEmpty()) {
                types.m_problems[id].append(
                    {lines.value(id).value(u"extends"_s, 1),
                     QObject::tr("extends %1, and there is no type %1").arg(base)});
                break;
            }
            if (seen.contains(baseId)) {
                types.m_problems[id].append(
                    {lines.value(id).value(u"extends"_s, 1),
                     QObject::tr("extends %1, which comes back around to it").arg(base)});
                break;
            }
            seen.insert(baseId);
            const NoteType &parent = own[baseId];
            inherited = parent.properties + inherited;
            base = parent.extends;
        }
        QList<TypeProperty> properties;
        for (const TypeProperty &property : std::as_const(inherited) + type.properties) {
            const auto same = std::ranges::find(properties, property.key, &TypeProperty::key);
            if (same != properties.end())
                *same = property; // a type's own key overrides what it extends
            else
                properties.append(property);
        }
        type.properties = properties;
        for (const TypeProperty &property : std::as_const(type.properties)) {
            if (!property.linkType.isEmpty() && !byName.contains(property.linkType.toLower()) &&
                lines.value(id).contains(u"properties."_s + property.key))
                types.m_problems[id].append({lines.value(id).value(u"properties."_s + property.key),
                                             QObject::tr("%1 links to %2, and there is no type %2")
                                                 .arg(property.key, property.linkType)});
        }
        types.m_types.append(type);
    }
    std::ranges::sort(types.m_types, [](const NoteType &a, const NoteType &b) {
        return a.name.localeAwareCompare(b.name) < 0;
    });
    return types;
}

NoteType NoteTypes::readFile(const QString &id, const QString &text)
{
    QList<TypeFileProblem> problems;
    QHash<QString, int> lines;
    return readType(id, text, &problems, &lines);
}

namespace {

/// `text` as an item of a YAML flow list, quoted when a comma or bracket would end it.
QString flowScalar(const QString &text)
{
    if (!text.contains(QRegularExpression(u"[,\\[\\]{}]"_s)))
        return yamlScalar(text);
    QString escaped = text;
    escaped.replace(u'\\', u"\\\\"_s).replace(u'"', u"\\\""_s);
    return u'"' + escaped + u'"';
}

QString flowList(const QStringList &items)
{
    QStringList written;
    for (const QString &item : items)
        written.append(flowScalar(item));
    return u'[' + written.join(u", "_s) + u']';
}

} // namespace

QString NoteTypes::fileText(const NoteType &type)
{
    QString text = u"name: %1\n"_s.arg(yamlScalar(type.name.isEmpty() ? type.id : type.name));
    const auto add = [&text](const QString &key, const QString &value) {
        if (!value.trimmed().isEmpty())
            text += u"%1: %2\n"_s.arg(key, yamlScalar(value.trimmed()));
    };
    add(u"icon"_s, type.icon);
    add(u"color"_s, type.color);
    add(u"extends"_s, type.extends);
    add(u"template"_s, type.templatePath);
    if (type.properties.isEmpty())
        return text;
    text += u"properties:\n"_s;
    for (const TypeProperty &property : type.properties) {
        QString spec;
        if (property.kind == TypeProperty::Kind::Choice) {
            spec =
                property.list
                    ? u"{kind: choice, choices: %1, list: true}"_s.arg(flowList(property.choices))
                    : flowList(property.choices);
        } else {
            spec = (property.list ? u"list "_s : QString()) + kindName(property.kind);
            if (property.kind == TypeProperty::Kind::Link && !property.linkType.isEmpty())
                spec += u' ' + property.linkType;
        }
        text += u"  %1: %2\n"_s.arg(yamlScalar(property.key), spec);
    }
    return text;
}

const NoteType *NoteTypes::find(const QString &name) const
{
    const QString wanted = name.trimmed();
    if (wanted.isEmpty())
        return nullptr;
    for (const NoteType &type : m_types) {
        if (type.name.compare(wanted, Qt::CaseInsensitive) == 0)
            return &type;
    }
    for (const NoteType &type : m_types) {
        if (type.id.compare(wanted, Qt::CaseInsensitive) == 0)
            return &type;
    }
    return nullptr;
}

bool NoteTypes::isA(const QString &name, const QString &base) const
{
    const NoteType *wanted = find(base);
    if (!wanted)
        return false;
    QSet<QString> seen;
    for (const NoteType *type = find(name); type && !seen.contains(type->id);
         type = find(type->extends)) {
        if (type->id == wanted->id)
            return true;
        seen.insert(type->id);
    }
    return false;
}

QStringList NoteTypes::starterIds()
{
    QStringList ids;
    for (const auto &[id, text] : starters())
        ids.append(id);
    return ids;
}

QString NoteTypes::starterText(const QString &id)
{
    for (const auto &[starter, text] : starters()) {
        if (starter == id)
            return text;
    }
    return {};
}

bool NoteTypes::writeStartersIfMissing(const QString &root, QString *error)
{
    if (QFileInfo::exists(root + u'/' + kFolder))
        return true;
    return restoreStarters(root, error);
}

QStringList NoteTypes::removeUntouchedStarters(const QString &root)
{
    const QString folder = root + u'/' + kFolder;
    QStringList removed;
    for (const auto &[id, text] : starters()) {
        QFile file(folder + u'/' + id + u".yaml"_s);
        if (file.open(QIODevice::ReadOnly) && QString::fromUtf8(file.readAll()) == text) {
            file.close();
            if (file.remove())
                removed.append(id);
        }
    }
    QDir().rmdir(folder);
    return removed;
}

QStringList NoteTypes::fileIds(const QString &root)
{
    QStringList ids;
    for (const QFileInfo &info :
         QDir(root + u'/' + kFolder).entryInfoList({u"*.yaml"_s}, QDir::Files))
        ids.append(info.completeBaseName());
    return ids;
}

QStringList NoteTypes::startersToRestore(const QString &root)
{
    QStringList ids;
    for (const auto &[id, text] : starters()) {
        QFile file(root + u'/' + kFolder + u'/' + id + u".yaml"_s);
        if (!file.open(QIODevice::ReadOnly) || QString::fromUtf8(file.readAll()) != text)
            ids.append(id);
    }
    return ids;
}

bool NoteTypes::restoreStarters(const QString &root, QString *error)
{
    const QString folder = root + u'/' + kFolder;
    if (!QDir().mkpath(folder)) {
        if (error)
            *error = QObject::tr("Could not make %1").arg(folder);
        return false;
    }
    for (const auto &[id, text] : starters()) {
        const QString path = folder + u'/' + id + u".yaml"_s;
        QFile file(path);
        if (file.open(QIODevice::ReadOnly) && QString::fromUtf8(file.readAll()) == text)
            continue;
        file.close();
        if (!writeFile(path, text, error))
            return false;
    }
    return true;
}

QString linkValueTarget(const QString &value)
{
    QString target = value.trimmed();
    if (target.startsWith(u"[[") && target.endsWith(u"]]"))
        target = target.mid(2, target.size() - 4);
    target = target.section(u'|', 0, 0).section(u'#', 0, 0).trimmed();
    return target;
}

QList<FrontMatterProblem>
noteTypeProblems(QStringView note, const NoteTypes &types,
                 const std::function<std::optional<QString>(const QString &)> &typeOfLink)
{
    const NoteParts parts = splitFrontMatter(note);
    if (!parts.hasFrontMatter)
        return {};
    const QVariantMap keys = readFrontMatter(parts.frontMatter);
    const QString typeName = keys.value(u"type"_s).toString().trimmed();
    if (typeName.isEmpty())
        return {};
    const QHash<QString, int> lines = keyLines(note);
    const NoteType *type = types.find(typeName);
    if (!type)
        return {{lines.value(u"type"_s), QObject::tr("There is no type %1").arg(typeName)}};
    QList<FrontMatterProblem> problems;
    for (const TypeProperty &property : type->properties) {
        const QVariant value = keys.value(property.key);
        if (!value.isValid())
            continue;
        const int line = lines.value(property.key);
        if (value.typeId() == QMetaType::QStringList) {
            if (!property.list) {
                problems.append(
                    {line, QObject::tr("%1 is one value, not a list").arg(property.key)});
                continue;
            }
            for (const QString &item : value.toStringList()) {
                if (const QString why = wrongValue(property, item, types, typeOfLink);
                    !why.isEmpty()) {
                    problems.append({line, why});
                    break;
                }
            }
        } else if (const QString text = value.toString(); !text.isEmpty()) {
            if (text.contains(u'\n')) {
                problems.append({line, QObject::tr("%1 is a %2, not a nested value")
                                           .arg(property.key, kindName(property.kind))});
            } else if (const QString why = wrongValue(property, text, types, typeOfLink);
                       !why.isEmpty()) {
                problems.append({line, why});
            }
        }
    }
    return problems;
}

} // namespace jot
