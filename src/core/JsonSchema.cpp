#include "jot/JsonSchema.h"

#include "jot/DataTable.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QObject>
#include <QRegularExpression>
#include <QUrl>

#include <cmath>

using namespace Qt::StringLiterals;

namespace jot {

namespace {

QString typeOf(const QJsonValue &value)
{
    switch (value.type()) {
    case QJsonValue::Null: return u"null"_s;
    case QJsonValue::Bool: return u"boolean"_s;
    case QJsonValue::Double: {
        const double number = value.toDouble();
        return std::isfinite(number) && std::floor(number) == number ? u"integer"_s : u"number"_s;
    }
    case QJsonValue::String: return u"string"_s;
    case QJsonValue::Array: return u"array"_s;
    case QJsonValue::Object: return u"object"_s;
    default: return {};
    }
}

/// Names a JSON type for people: "a number", "text".
QString typeName(const QString &type)
{
    if (type == u"string")
        return QObject::tr("text");
    if (type == u"integer")
        return QObject::tr("a whole number");
    if (type == u"number")
        return QObject::tr("a number");
    if (type == u"boolean")
        return QObject::tr("true or false");
    if (type == u"array")
        return QObject::tr("a list");
    if (type == u"object")
        return QObject::tr("a set of fields");
    if (type == u"null")
        return QObject::tr("null");
    return type;
}

/// Characters as people count them, an emoji once.
qsizetype characters(const QString &text)
{
    return text.toUcs4().size();
}

class Checker
{
public:
    explicit Checker(const QJsonValue &root) : m_root(root) {}

    QList<SchemaProblem> problems;

    void check(const QJsonValue &schema, const QJsonValue &data, const QStringList &path,
               int depth = 0)
    {
        if (depth > 64) {
            add(path, QObject::tr("the schema refers to itself too deeply"));
            return;
        }
        if (schema.isBool()) {
            if (!schema.toBool())
                add(path, QObject::tr("is not allowed here"));
            return;
        }
        if (!schema.isObject())
            return;
        const QJsonObject rules = schema.toObject();

        if (rules.contains(u"$ref"_s)) {
            const std::optional<QJsonValue> target = resolve(rules.value(u"$ref"_s).toString());
            if (!target)
                add(path, QObject::tr("the schema's $ref %1 is not in it")
                              .arg(rules.value(u"$ref"_s).toString()));
            else
                check(*target, data, path, depth + 1);
        }

        const QString type = typeOf(data);
        if (const QJsonValue wanted = rules.value(u"type"_s); !wanted.isUndefined()) {
            QStringList types;
            if (wanted.isArray()) {
                for (const QJsonValue &each : wanted.toArray())
                    types.append(each.toString());
            } else {
                types.append(wanted.toString());
            }
            const bool fits =
                types.contains(type) || (type == u"integer" && types.contains(u"number"_s));
            if (!fits) {
                QStringList names;
                for (const QString &each : std::as_const(types))
                    names.append(typeName(each));
                add(path, QObject::tr("should be %1").arg(names.join(QObject::tr(" or "))));
                return;
            }
        }
        if (const QJsonValue options = rules.value(u"enum"_s); options.isArray()) {
            if (!options.toArray().contains(data)) {
                QStringList names;
                for (const QJsonValue &each : options.toArray())
                    names.append(jsonText(each));
                add(path, QObject::tr("should be one of %1").arg(names.join(u", "_s)));
            }
        }
        if (rules.contains(u"const"_s) && rules.value(u"const"_s) != data)
            add(path, QObject::tr("should be %1").arg(jsonText(rules.value(u"const"_s))));

        if (data.isDouble())
            checkNumber(rules, data.toDouble(), path);
        if (data.isString())
            checkString(rules, data.toString(), path);
        if (data.isArray())
            checkArray(rules, data.toArray(), path, depth);
        if (data.isObject())
            checkObject(rules, data.toObject(), path, depth);

        for (const QJsonValue &each : rules.value(u"allOf"_s).toArray())
            check(each, data, path, depth + 1);
        if (const QJsonValue any = rules.value(u"anyOf"_s); any.isArray()) {
            if (std::ranges::none_of(any.toArray(), [&](const QJsonValue &each) {
                    return passes(each, data, depth);
                }))
                add(path, QObject::tr("matches none of the shapes it may take"));
        }
        if (const QJsonValue one = rules.value(u"oneOf"_s); one.isArray()) {
            const auto matching = std::ranges::count_if(
                one.toArray(), [&](const QJsonValue &each) { return passes(each, data, depth); });
            if (matching != 1)
                add(path, matching == 0
                              ? QObject::tr("matches none of the shapes it may take")
                              : QObject::tr("matches more than one of the shapes it may take"));
        }
        if (const QJsonValue never = rules.value(u"not"_s); !never.isUndefined()) {
            if (passes(never, data, depth))
                add(path, QObject::tr("matches a shape it must not"));
        }
        if (const QJsonValue condition = rules.value(u"if"_s); !condition.isUndefined()) {
            const QJsonValue branch =
                rules.value(passes(condition, data, depth) ? u"then"_s : u"else"_s);
            if (!branch.isUndefined())
                check(branch, data, path, depth + 1);
        }
    }

private:
    void add(const QStringList &path, const QString &message) { problems.append({path, message}); }

    bool passes(const QJsonValue &schema, const QJsonValue &data, int depth)
    {
        Checker trial(m_root);
        trial.check(schema, data, {}, depth + 1);
        return trial.problems.isEmpty();
    }

    std::optional<QJsonValue> resolve(const QString &ref) const
    {
        return resolveSchemaRef(m_root, ref);
    }

    void checkNumber(const QJsonObject &rules, double number, const QStringList &path)
    {
        if (const QJsonValue v = rules.value(u"minimum"_s); v.isDouble() && number < v.toDouble())
            add(path, QObject::tr("should be at least %1").arg(v.toDouble()));
        if (const QJsonValue v = rules.value(u"maximum"_s); v.isDouble() && number > v.toDouble())
            add(path, QObject::tr("should be at most %1").arg(v.toDouble()));
        if (const QJsonValue v = rules.value(u"exclusiveMinimum"_s);
            v.isDouble() && number <= v.toDouble())
            add(path, QObject::tr("should be more than %1").arg(v.toDouble()));
        if (const QJsonValue v = rules.value(u"exclusiveMaximum"_s);
            v.isDouble() && number >= v.toDouble())
            add(path, QObject::tr("should be less than %1").arg(v.toDouble()));
        if (const QJsonValue v = rules.value(u"multipleOf"_s); v.isDouble() && v.toDouble() > 0) {
            const double steps = number / v.toDouble();
            if (std::abs(steps - std::round(steps)) > 1e-9)
                add(path, QObject::tr("should be a multiple of %1").arg(v.toDouble()));
        }
    }

    void checkString(const QJsonObject &rules, const QString &text, const QStringList &path)
    {
        const qsizetype length = characters(text);
        if (const QJsonValue v = rules.value(u"minLength"_s); v.isDouble() && length < v.toInt())
            add(path, QObject::tr("should be at least %n characters", nullptr, v.toInt()));
        if (const QJsonValue v = rules.value(u"maxLength"_s); v.isDouble() && length > v.toInt())
            add(path, QObject::tr("should be at most %n characters", nullptr, v.toInt()));
        if (const QJsonValue v = rules.value(u"pattern"_s); v.isString()) {
            const QRegularExpression pattern(v.toString());
            if (pattern.isValid() && !pattern.match(text).hasMatch())
                add(path, QObject::tr("should match %1").arg(v.toString()));
        }
    }

    void checkArray(const QJsonObject &rules, const QJsonArray &items, const QStringList &path,
                    int depth)
    {
        qsizetype from = 0;
        if (const QJsonValue prefix = rules.value(u"prefixItems"_s); prefix.isArray()) {
            const QJsonArray schemas = prefix.toArray();
            for (; from < std::min(schemas.size(), items.size()); ++from)
                check(schemas.at(from), items.at(from), path + QStringList{QString::number(from)},
                      depth + 1);
        }
        if (const QJsonValue each = rules.value(u"items"_s); !each.isUndefined()) {
            for (qsizetype i = from; i < items.size(); ++i)
                check(each, items.at(i), path + QStringList{QString::number(i)}, depth + 1);
        }
        if (const QJsonValue v = rules.value(u"minItems"_s);
            v.isDouble() && items.size() < v.toInt())
            add(path, QObject::tr("should have at least %n items", nullptr, v.toInt()));
        if (const QJsonValue v = rules.value(u"maxItems"_s);
            v.isDouble() && items.size() > v.toInt())
            add(path, QObject::tr("should have at most %n items", nullptr, v.toInt()));
        if (rules.value(u"uniqueItems"_s).toBool()) {
            for (qsizetype i = 0; i < items.size(); ++i) {
                for (qsizetype j = 0; j < i; ++j) {
                    if (items.at(i) == items.at(j)) {
                        add(path + QStringList{QString::number(i)},
                            QObject::tr("is the same as item %1").arg(j + 1));
                        break;
                    }
                }
            }
        }
        if (const QJsonValue contains = rules.value(u"contains"_s); !contains.isUndefined()) {
            const auto found = std::ranges::count_if(
                items, [&](const QJsonValue &item) { return passes(contains, item, depth); });
            const int least = rules.value(u"minContains"_s).toInt(1);
            if (found < least)
                add(path, QObject::tr("should hold at least %n matching items", nullptr, least));
            if (const QJsonValue most = rules.value(u"maxContains"_s);
                most.isDouble() && found > most.toInt())
                add(path,
                    QObject::tr("should hold at most %n matching items", nullptr, most.toInt()));
        }
    }

    void checkObject(const QJsonObject &rules, const QJsonObject &fields, const QStringList &path,
                     int depth)
    {
        for (const QJsonValue &key : rules.value(u"required"_s).toArray()) {
            if (!fields.contains(key.toString()))
                add(path + QStringList{key.toString()}, QObject::tr("is required"));
        }
        const QJsonObject properties = rules.value(u"properties"_s).toObject();
        const QJsonObject patterns = rules.value(u"patternProperties"_s).toObject();
        const QJsonValue additional = rules.value(u"additionalProperties"_s);
        for (auto it = fields.constBegin(); it != fields.constEnd(); ++it) {
            const QStringList here = path + QStringList{it.key()};
            bool known = false;
            if (properties.contains(it.key())) {
                known = true;
                check(properties.value(it.key()), it.value(), here, depth + 1);
            }
            for (auto p = patterns.constBegin(); p != patterns.constEnd(); ++p) {
                if (QRegularExpression(p.key()).match(it.key()).hasMatch()) {
                    known = true;
                    check(p.value(), it.value(), here, depth + 1);
                }
            }
            if (!known && !additional.isUndefined()) {
                if (additional.isBool() && !additional.toBool())
                    add(here, QObject::tr("is not a field this may have"));
                else
                    check(additional, it.value(), here, depth + 1);
            }
            if (const QJsonValue names = rules.value(u"propertyNames"_s); !names.isUndefined()) {
                if (!passes(names, it.key(), depth))
                    add(here, QObject::tr("is not a name this field may have"));
            }
        }
        if (const QJsonValue v = rules.value(u"minProperties"_s);
            v.isDouble() && fields.size() < v.toInt())
            add(path, QObject::tr("should have at least %n fields", nullptr, v.toInt()));
        if (const QJsonValue v = rules.value(u"maxProperties"_s);
            v.isDouble() && fields.size() > v.toInt())
            add(path, QObject::tr("should have at most %n fields", nullptr, v.toInt()));
        const QJsonObject dependent = rules.value(u"dependentRequired"_s).toObject();
        for (auto it = dependent.constBegin(); it != dependent.constEnd(); ++it) {
            if (!fields.contains(it.key()))
                continue;
            for (const QJsonValue &key : it.value().toArray()) {
                if (!fields.contains(key.toString()))
                    add(path + QStringList{key.toString()},
                        QObject::tr("is required with %1").arg(it.key()));
            }
        }
    }

    QJsonValue m_root;
};

QJsonValue csvValue(const QString &text)
{
    static const QRegularExpression number(uR"(^-?(?:0|[1-9]\d*)(?:\.\d+)?(?:[eE][+-]?\d+)?$)"_s);
    if (number.match(text).hasMatch())
        return text.toDouble();
    if (text == u"true" || text == u"false")
        return text == u"true";
    return text;
}

} // namespace

QList<SchemaProblem> checkSchema(const QJsonValue &schema, const QJsonValue &data)
{
    Checker checker(schema);
    checker.check(schema, data, {});
    return checker.problems;
}

std::optional<QJsonValue> resolveSchemaRef(const QJsonValue &schema, const QString &ref)
{
    if (!ref.startsWith(u'#'))
        return std::nullopt;
    QJsonValue at = schema;
    for (QString part : ref.mid(1).split(u'/', Qt::SkipEmptyParts)) {
        part = QUrl::fromPercentEncoding(part.toUtf8());
        part.replace(u"~1"_s, u"/"_s).replace(u"~0"_s, u"~"_s);
        bool isIndex = false;
        const int index = part.toInt(&isIndex);
        if (at.isObject() && at.toObject().contains(part))
            at = at.toObject().value(part);
        else if (at.isArray() && isIndex && index >= 0 && index < at.toArray().size())
            at = at.toArray().at(index);
        else
            return std::nullopt;
    }
    return at;
}

QString jsonText(const QJsonValue &value)
{
    if (value.isString())
        return u'"' + value.toString() + u'"';
    if (value.isArray())
        return QString::fromUtf8(QJsonDocument(value.toArray()).toJson(QJsonDocument::Compact));
    if (value.isObject())
        return QString::fromUtf8(QJsonDocument(value.toObject()).toJson(QJsonDocument::Compact));
    return QJsonDocument(QJsonArray{value}).toJson(QJsonDocument::Compact).mid(1).chopped(1);
}

std::optional<QJsonValue> dataAsJson(const QString &kind, const QString &text, QString *error)
{
    if (kind == u"json") {
        QJsonParseError parsed;
        const QJsonDocument document = QJsonDocument::fromJson(text.toUtf8(), &parsed);
        if (parsed.error != QJsonParseError::NoError) {
            if (error)
                *error = parsed.errorString();
            return std::nullopt;
        }
        return document.isArray() ? QJsonValue(document.array()) : QJsonValue(document.object());
    }
    if (kind == u"yaml")
        return DataTable::yamlAsJson(text, error);
    if (kind == u"csv" || kind == u"tsv") {
        const std::optional<DataTable> table = DataTable::read(kind, text, error);
        if (!table)
            return std::nullopt;
        QJsonArray rows;
        for (int row = 0; row < table->rowCount(); ++row) {
            QJsonObject fields;
            for (int column = 0; column < table->columns().size(); ++column) {
                const DataValue value = table->value(row, column);
                if (value.type != DataValue::Type::Missing && !value.text.isEmpty())
                    fields.insert(table->columns().at(column), csvValue(value.text));
            }
            rows.append(fields);
        }
        return rows;
    }
    if (error)
        *error = QObject::tr("not a data file");
    return std::nullopt;
}

QString schemaPathFor(const QString &path, const QJsonValue &data)
{
    const QFileInfo file(path);
    const QDir folder = file.dir();
    if (const QString named = data.toObject().value(u"$schema"_s).toString();
        !named.isEmpty() && !named.contains(u"://"_s)) {
        const QString candidate = QDir::cleanPath(folder.absoluteFilePath(named));
        if (QFileInfo(candidate).isFile())
            return candidate;
    }
    const QString beside = folder.absoluteFilePath(file.completeBaseName() + u".schema.json"_s);
    return QFileInfo(beside).isFile() ? beside : QString();
}

QList<DataProblem> checkDataFile(const QString &path, const QString &text)
{
    const QString kind = dataKindOf(path);
    if (const DataProblem problem = checkData(kind, text); !problem.ok())
        return {problem};
    QString error;
    const std::optional<QJsonValue> data = dataAsJson(kind, text, &error);
    if (!data)
        return {problemAt(text, 0, error)};
    const QString schemaPath = schemaPathFor(path, *data);
    if (schemaPath.isEmpty())
        return {};
    QFile file(schemaPath);
    if (!file.open(QIODevice::ReadOnly))
        return {problemAt(text, 0, QObject::tr("could not read its schema %1").arg(schemaPath))};
    QJsonParseError parsed;
    const QJsonDocument schema = QJsonDocument::fromJson(file.readAll(), &parsed);
    if (parsed.error != QJsonParseError::NoError)
        return {problemAt(text, 0,
                          QObject::tr("its schema %1 is not JSON: %2")
                              .arg(QFileInfo(schemaPath).fileName(), parsed.errorString()))};

    // Each problem where its record and field are, when the file is a list of records.
    const std::optional<DataTable> table = DataTable::read(kind, text);
    QList<DataProblem> problems;
    const QJsonValue root =
        schema.isObject() ? QJsonValue(schema.object()) : QJsonValue(schema.array());
    for (const SchemaProblem &each : checkSchema(root, *data)) {
        qsizetype at = 0;
        QStringList rest = each.path;
        if (table && !each.path.isEmpty()) {
            bool isRow = false;
            const int row = each.path.at(0).toInt(&isRow);
            if (isRow) {
                at = table->positionOf(row, each.path.value(1));
                rest.removeFirst(); // the line says which record
            }
        }
        const QString where = rest.isEmpty() ? QString() : rest.join(u'.') + u": "_s;
        problems.append(problemAt(text, at, where + each.message));
    }
    return problems;
}

} // namespace jot
