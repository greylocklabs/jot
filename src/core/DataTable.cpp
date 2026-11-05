#include "jot/DataTable.h"

#include "jot/FrontMatter.h"

#include <yaml.h>

#include <QHash>
#include <QObject>
#include <QRegularExpression>

#include <algorithm>

using namespace Qt::StringLiterals;

namespace jot {

namespace {

struct JsonMember;

/// A JSON value with where it sits in the text, read in order so keys keep theirs.
struct JsonNode
{
    enum class Kind { String, Number, Bool, Null, Object, Array };
    Kind kind = Kind::Null;
    qsizetype start = 0;
    qsizetype end = 0;
    /// A string's text, or a number or literal as written.
    QString text;
    std::vector<JsonMember> members;
    std::vector<JsonNode> items;
};

struct JsonMember
{
    QString key;
    qsizetype keyStart = 0;
    qsizetype keyEnd = 0;
    JsonNode value;
};

/// Reads JSON as written, keeping every value's place in the text.
class JsonReader
{
public:
    explicit JsonReader(const QString &text) : m_text(text) {}

    std::optional<JsonNode> readDocument()
    {
        skipSpace();
        std::optional<JsonNode> node = readValue(0);
        skipSpace();
        if (node && m_at != m_text.size())
            return fail(QObject::tr("more after the end of the JSON"));
        return node;
    }

    [[nodiscard]] QString error() const { return m_error; }

private:
    std::optional<JsonNode> fail(const QString &why)
    {
        if (m_error.isEmpty())
            m_error = why;
        return std::nullopt;
    }

    void skipSpace()
    {
        while (m_at < m_text.size() && u" \t\r\n"_s.contains(m_text.at(m_at)))
            ++m_at;
    }

    std::optional<JsonNode> readValue(int depth)
    {
        if (depth > 512)
            return fail(QObject::tr("nested too deeply"));
        if (m_at >= m_text.size())
            return fail(QObject::tr("the JSON ends early"));
        const QChar c = m_text.at(m_at);
        JsonNode node;
        node.start = m_at;
        if (c == u'{') {
            node.kind = JsonNode::Kind::Object;
            ++m_at;
            skipSpace();
            if (m_at < m_text.size() && m_text.at(m_at) == u'}') {
                node.end = ++m_at;
                return node;
            }
            for (;;) {
                skipSpace();
                JsonMember member;
                member.keyStart = m_at;
                const std::optional<QString> key = readString();
                if (!key)
                    return fail(QObject::tr("a key that is not a string"));
                member.key = *key;
                member.keyEnd = m_at;
                skipSpace();
                if (m_at >= m_text.size() || m_text.at(m_at) != u':')
                    return fail(QObject::tr("a key without a colon"));
                ++m_at;
                skipSpace();
                std::optional<JsonNode> value = readValue(depth + 1);
                if (!value)
                    return std::nullopt;
                member.value = std::move(*value);
                node.members.push_back(std::move(member));
                skipSpace();
                if (m_at < m_text.size() && m_text.at(m_at) == u',') {
                    ++m_at;
                    continue;
                }
                if (m_at < m_text.size() && m_text.at(m_at) == u'}') {
                    node.end = ++m_at;
                    return node;
                }
                return fail(QObject::tr("an object that is not closed"));
            }
        }
        if (c == u'[') {
            node.kind = JsonNode::Kind::Array;
            ++m_at;
            skipSpace();
            if (m_at < m_text.size() && m_text.at(m_at) == u']') {
                node.end = ++m_at;
                return node;
            }
            for (;;) {
                skipSpace();
                std::optional<JsonNode> item = readValue(depth + 1);
                if (!item)
                    return std::nullopt;
                node.items.push_back(std::move(*item));
                skipSpace();
                if (m_at < m_text.size() && m_text.at(m_at) == u',') {
                    ++m_at;
                    continue;
                }
                if (m_at < m_text.size() && m_text.at(m_at) == u']') {
                    node.end = ++m_at;
                    return node;
                }
                return fail(QObject::tr("an array that is not closed"));
            }
        }
        if (c == u'"') {
            const std::optional<QString> text = readString();
            if (!text)
                return fail(QObject::tr("a string that is not closed"));
            node.kind = JsonNode::Kind::String;
            node.text = *text;
            node.end = m_at;
            return node;
        }
        for (const auto &[word, kind] : {std::pair{u"true"_s, JsonNode::Kind::Bool},
                                         std::pair{u"false"_s, JsonNode::Kind::Bool},
                                         std::pair{u"null"_s, JsonNode::Kind::Null}}) {
            if (QStringView(m_text).sliced(m_at).startsWith(word)) {
                node.kind = kind;
                node.text = word;
                m_at += word.size();
                node.end = m_at;
                return node;
            }
        }
        static const QRegularExpression number(uR"(-?(?:0|[1-9]\d*)(?:\.\d+)?(?:[eE][+-]?\d+)?)"_s);
        const QRegularExpressionMatch match =
            number.matchView(m_text, m_at, QRegularExpression::NormalMatch,
                             QRegularExpression::AnchorAtOffsetMatchOption);
        if (match.hasMatch() && match.capturedLength() > 0) {
            node.kind = JsonNode::Kind::Number;
            node.text = match.captured();
            m_at += match.capturedLength();
            node.end = m_at;
            return node;
        }
        return fail(QObject::tr("something that is not a JSON value"));
    }

    std::optional<QString> readString()
    {
        if (m_at >= m_text.size() || m_text.at(m_at) != u'"')
            return std::nullopt;
        ++m_at;
        QString out;
        while (m_at < m_text.size()) {
            const QChar c = m_text.at(m_at++);
            if (c == u'"')
                return out;
            if (c != u'\\') {
                out += c;
                continue;
            }
            if (m_at >= m_text.size())
                break;
            const QChar e = m_text.at(m_at++);
            switch (e.unicode()) {
            case 'n': out += u'\n'; break;
            case 't': out += u'\t'; break;
            case 'r': out += u'\r'; break;
            case 'b': out += u'\b'; break;
            case 'f': out += u'\f'; break;
            case 'u': {
                bool ok = false;
                const ushort code = m_text.mid(m_at, 4).toUShort(&ok, 16);
                if (!ok)
                    return std::nullopt;
                out += QChar(code);
                m_at += 4;
                break;
            }
            default: out += e;
            }
        }
        return std::nullopt;
    }

    const QString &m_text;
    qsizetype m_at = 0;
    QString m_error;
};

QString quoted(const QString &text)
{
    QString out = u"\""_s;
    for (const QChar c : text) {
        switch (c.unicode()) {
        case '"': out += u"\\\""_s; break;
        case '\\': out += u"\\\\"_s; break;
        case '\n': out += u"\\n"_s; break;
        case '\r': out += u"\\r"_s; break;
        case '\t': out += u"\\t"_s; break;
        case '\b': out += u"\\b"_s; break;
        case '\f': out += u"\\f"_s; break;
        default:
            if (c.unicode() < 0x20)
                out += u"\\u%1"_s.arg(uint(c.unicode()), 4, 16, QChar(u'0'));
            else
                out += c;
        }
    }
    return out + u'"';
}

/// A nested value on one line, for showing in a cell.
QString compact(const JsonNode &node)
{
    switch (node.kind) {
    case JsonNode::Kind::String: return quoted(node.text);
    case JsonNode::Kind::Object: {
        QStringList parts;
        for (const JsonMember &member : node.members)
            parts.append(quoted(member.key) + u": "_s + compact(member.value));
        return u'{' + parts.join(u", "_s) + u'}';
    }
    case JsonNode::Kind::Array: {
        QStringList parts;
        for (const JsonNode &item : node.items)
            parts.append(compact(item));
        return u'[' + parts.join(u", "_s) + u']';
    }
    default: return node.text;
    }
}

/// A value written as JSON.stringify writes it with `indent`, its inner lines `depth` levels
/// in; all on one line when `indent` is empty.
QString pretty(const JsonNode &node, const QString &indent, int depth, const QString &colon,
               const QString &newline = u"\n"_s)
{
    const bool flat = indent.isEmpty();
    const QString inner = newline + indent.repeated(depth + 1);
    const QString outer = newline + indent.repeated(depth);
    switch (node.kind) {
    case JsonNode::Kind::String: return quoted(node.text);
    case JsonNode::Kind::Object: {
        if (node.members.empty())
            return u"{}"_s;
        QStringList parts;
        for (const JsonMember &member : node.members)
            parts.append(quoted(member.key) + colon +
                         pretty(member.value, indent, depth + 1, colon, newline));
        return flat ? u'{' + parts.join(u',') + u'}'
                    : u'{' + inner + parts.join(u',' + inner) + outer + u'}';
    }
    case JsonNode::Kind::Array: {
        if (node.items.empty())
            return u"[]"_s;
        QStringList parts;
        for (const JsonNode &item : node.items)
            parts.append(pretty(item, indent, depth + 1, colon, newline));
        return flat ? u'[' + parts.join(u',') + u']'
                    : u'[' + inner + parts.join(u',' + inner) + outer + u']';
    }
    default: return node.text;
    }
}

DataValue valueOf(const JsonNode &node)
{
    switch (node.kind) {
    case JsonNode::Kind::String: return {DataValue::Type::String, node.text};
    case JsonNode::Kind::Number: return {DataValue::Type::Number, node.text};
    case JsonNode::Kind::Bool: return {DataValue::Type::Bool, node.text};
    case JsonNode::Kind::Null: return {DataValue::Type::Null, node.text};
    default: return {DataValue::Type::Nested, compact(node)};
    }
}

/// A plain YAML scalar as its type, as YAML's core schema reads it.
DataValue::Type yamlType(const QString &text)
{
    static const QStringList nulls = {u""_s, u"~"_s, u"null"_s, u"Null"_s, u"NULL"_s};
    static const QStringList bools = {u"true"_s,  u"false"_s, u"True"_s,
                                      u"False"_s, u"TRUE"_s,  u"FALSE"_s};
    static const QRegularExpression number(
        uR"(^(?:[-+]?(?:\d[\d_]*(?:\.\d*)?|\.\d+)(?:[eE][-+]?\d+)?|0x[0-9a-fA-F]+|0o[0-7]+|[-+]?\.(?:inf|Inf|INF)|\.(?:nan|NaN|NAN))$)"_s);
    if (nulls.contains(text))
        return DataValue::Type::Null;
    if (bools.contains(text))
        return DataValue::Type::Bool;
    if (number.match(text).hasMatch())
        return DataValue::Type::Number;
    return DataValue::Type::String;
}

bool isNumber(const QString &text)
{
    static const QRegularExpression number(uR"(^-?(?:0|[1-9]\d*)(?:\.\d+)?(?:[eE][+-]?\d+)?$)"_s);
    return number.match(text).hasMatch();
}

} // namespace

std::optional<DataTable> DataTable::read(const QString &kind, const QString &text, QString *why)
{
    DataTable table;
    table.m_kind = kind;
    table.m_text = text;
    QString reason;
    if (!table.parse(&reason)) {
        if (why)
            *why = reason;
        return std::nullopt;
    }
    return table;
}

bool DataTable::parse(QString *why)
{
    m_rows.clear();
    m_columns.clear();
    m_header = {};
    if (m_kind == u"json")
        return parseJson(why);
    if (m_kind == u"csv" || m_kind == u"tsv")
        return parseDelimited(why);
    if (m_kind == u"yaml")
        return parseYaml(why);
    *why = QObject::tr("not a kind of file that holds a table");
    return false;
}

bool DataTable::parseJson(QString *why)
{
    JsonReader reader(m_text);
    const std::optional<JsonNode> root = reader.readDocument();
    if (!root) {
        *why = reader.error();
        return false;
    }
    if (root->kind != JsonNode::Kind::Array ||
        !std::ranges::all_of(root->items, [](const JsonNode &item) {
            return item.kind == JsonNode::Kind::Object;
        })) {
        *why = QObject::tr("not a list of records");
        return false;
    }
    m_array = {root->start, root->end};
    for (const JsonNode &item : root->items) {
        Row row{{item.start, item.end}, {}};
        for (const JsonMember &member : item.members) {
            Field field;
            field.name = member.key;
            field.key = {member.keyStart, member.keyEnd};
            field.value = {member.value.start, member.value.end};
            field.data = valueOf(member.value);
            row.fields.append(field);
            if (!m_columns.contains(member.key))
                m_columns.append(member.key);
        }
        m_rows.append(row);
    }
    // One level of indentation, from the first record's line; none for a file on one line.
    m_indent.clear();
    if (!m_rows.isEmpty() &&
        m_text.sliced(m_array.start, m_array.end - m_array.start).contains(u'\n'))
        m_indent = lineIndent(m_rows.first().span.start);
    if (m_indent.isEmpty() && !m_rows.isEmpty() &&
        m_text.sliced(m_array.start, m_array.end - m_array.start).contains(u'\n'))
        m_indent = u"  "_s;
    return true;
}

bool DataTable::parseDelimited(QString *why)
{
    const QChar split = delimiter();
    QList<Row> records;
    Row record;
    Field field;
    bool quoted = false;
    bool fieldStart = true;
    QString value;
    const auto endField = [&](qsizetype at) {
        field.value.end = at;
        field.data = {DataValue::Type::String, value};
        record.fields.append(field);
        field = {};
        value.clear();
    };
    for (qsizetype i = 0; i <= m_text.size(); ++i) {
        const bool atEnd = i == m_text.size();
        const QChar c = atEnd ? u'\n' : m_text.at(i);
        if (fieldStart) {
            field.value.start = i;
            field.quoted = false;
            fieldStart = false;
            if (!atEnd && c == u'"') {
                field.quoted = true;
                quoted = true;
                continue;
            }
        }
        if (quoted && !atEnd) {
            if (c == u'"') {
                if (i + 1 < m_text.size() && m_text.at(i + 1) == u'"') {
                    value += u'"';
                    ++i;
                } else {
                    quoted = false;
                }
            } else {
                value += c;
            }
            continue;
        }
        if (c == split) {
            endField(i);
            fieldStart = true;
        } else if (c == u'\n') {
            const qsizetype end = (!atEnd && i > 0 && m_text.at(i - 1) == u'\r') ? i - 1 : i;
            if (end < i)
                value.chop(value.endsWith(u'\r') ? 1 : 0);
            endField(end);
            record.span.end = end;
            // The newline that ends the file starts no record.
            if (!(atEnd && record.fields.size() == 1 && record.span.start == m_text.size()))
                records.append(record);
            record = {};
            record.span.start = i + 1;
            fieldStart = true;
        } else {
            value += c;
        }
    }
    if (quoted) {
        *why = QObject::tr("a quote that is never closed");
        return false;
    }
    if (records.isEmpty()) {
        *why = QObject::tr("no header row");
        return false;
    }
    m_header = records.takeFirst();
    for (Field &each : m_header.fields) {
        each.name = each.data.text;
        m_columns.append(each.name);
    }
    for (Row &row : records) {
        for (qsizetype i = 0; i < row.fields.size(); ++i)
            row.fields[i].name = i < m_columns.size() ? m_columns.at(i) : QString();
    }
    m_rows = records;
    return true;
}

const DataTable::Field *DataTable::field(int row, int column) const
{
    if (row < 0 || row >= m_rows.size() || column < 0 || column >= m_columns.size())
        return nullptr;
    const Row &record = m_rows.at(row);
    if (m_kind == u"csv" || m_kind == u"tsv")
        return column < record.fields.size() ? &record.fields.at(column) : nullptr;
    const QString &name = m_columns.at(column);
    for (const Field &each : record.fields) {
        if (each.name == name)
            return &each;
    }
    return nullptr;
}

DataValue DataTable::value(int row, int column) const
{
    const Field *found = field(row, column);
    return found ? found->data : DataValue();
}

QString DataTable::nestedText(int row, int column) const
{
    const DataValue nested = value(row, column);
    if (nested.type != DataValue::Type::Nested)
        return {};
    JsonReader reader(nested.text);
    const std::optional<JsonNode> node = reader.readDocument();
    return node ? pretty(*node, u"  "_s, 0, u": "_s) : nested.text;
}

bool DataTable::stray(int row, int column) const
{
    const auto kindOf = [this](const DataValue &value) {
        if ((m_kind == u"csv" || m_kind == u"tsv") && value.type == DataValue::Type::String)
            return value.text.isEmpty()   ? DataValue::Type::Missing
                   : isNumber(value.text) ? DataValue::Type::Number
                                          : DataValue::Type::String;
        return value.type;
    };
    const DataValue::Type mine = kindOf(value(row, column));
    if (mine == DataValue::Type::Missing || mine == DataValue::Type::Null)
        return false;
    QHash<DataValue::Type, int> counts;
    for (int i = 0; i < rowCount(); ++i) {
        const DataValue::Type each = kindOf(value(i, column));
        if (each != DataValue::Type::Missing && each != DataValue::Type::Null)
            ++counts[each];
    }
    const int most = std::ranges::max(counts.values());
    return counts.value(mine) < most;
}

QString DataTable::lineIndent(qsizetype at) const
{
    const qsizetype lineStart = at <= 0 ? 0 : m_text.lastIndexOf(u'\n', at - 1) + 1;
    qsizetype end = lineStart;
    while (end < at && (m_text.at(end) == u' ' || m_text.at(end) == u'\t'))
        ++end;
    return m_text.sliced(lineStart, end - lineStart);
}

QString DataTable::newline() const
{
    return m_text.contains(u"\r\n"_s) ? u"\r\n"_s : u"\n"_s;
}

QString DataTable::jsonValue(const DataValue &value, int depth) const
{
    switch (value.type) {
    case DataValue::Type::String: return quoted(value.text);
    case DataValue::Type::Missing:
    case DataValue::Type::Null: return u"null"_s;
    case DataValue::Type::Nested: {
        JsonReader reader(value.text);
        const std::optional<JsonNode> node = reader.readDocument();
        return node ? pretty(*node, m_indent, depth, m_indent.isEmpty() ? u":"_s : u": "_s,
                             newline())
                    : value.text;
    }
    default: return value.text;
    }
}

QString DataTable::csvField(const QString &text, bool quote) const
{
    if (quote || text.contains(delimiter()) || text.contains(u'"') || text.contains(u'\n') ||
        text.contains(u'\r'))
        return u'"' + QString(text).replace(u"\""_s, u"\"\""_s) + u'"';
    return text;
}

bool DataTable::patch(Span span, const QString &replacement)
{
    return patches({{span, replacement}});
}

bool DataTable::patches(QList<std::pair<Span, QString>> edits)
{
    // From the end back, so each span is still where it was read.
    std::ranges::sort(edits,
                      [](const auto &a, const auto &b) { return a.first.start > b.first.start; });
    QString text = m_text;
    for (const auto &[span, replacement] : std::as_const(edits))
        text.replace(span.start, span.end - span.start, replacement);
    const QString before = m_text;
    const QStringList order = m_columns;
    m_text = text;
    QString why;
    if (!parse(&why)) {
        m_text = before;
        parse(&why);
        return false;
    }
    if (m_kind == u"json" || m_kind == u"yaml") {
        // Columns stay where they were; new ones join at the end.
        QStringList columns;
        for (const QString &name : order) {
            if (m_columns.contains(name) || m_pending.contains(name))
                columns.append(name);
        }
        for (const QString &name : std::as_const(m_columns)) {
            if (!columns.contains(name))
                columns.append(name);
        }
        m_columns = columns;
    }
    return true;
}

DataValue DataTable::typed(const QString &text, DataValue::Type like)
{
    switch (like) {
    case DataValue::Type::Nested: return {DataValue::Type::Nested, text};
    case DataValue::Type::Number:
        if (isNumber(text.trimmed()))
            return {DataValue::Type::Number, text.trimmed()};
        break;
    case DataValue::Type::Bool:
        if (text.trimmed() == u"true" || text.trimmed() == u"false")
            return {DataValue::Type::Bool, text.trimmed()};
        break;
    case DataValue::Type::Null:
        if (text.trimmed() == u"null")
            return {DataValue::Type::Null, u"null"_s};
        break;
    default: break;
    }
    return {DataValue::Type::String, text};
}

bool DataTable::setValue(int row, int column, const DataValue &value, QString *error)
{
    if (row < 0 || row >= m_rows.size() || column < 0 || column >= m_columns.size())
        return false;
    if (m_kind == u"yaml") {
        if (value.type == DataValue::Type::Nested) {
            JsonReader reader(value.text);
            const std::optional<JsonNode> node = reader.readDocument();
            if (!node ||
                (node->kind != JsonNode::Kind::Object && node->kind != JsonNode::Kind::Array)) {
                if (error)
                    *error = node ? QObject::tr("not an object or a list") : reader.error();
                return false;
            }
        }
        return yamlSetValue(row, column, value, error);
    }
    if (m_kind != u"json") {
        const Row &record = m_rows.at(row);
        if (const Field *found = field(row, column))
            return patch(found->value, csvField(value.text, found->quoted));
        // A short row gets the empty fields before this one.
        const qsizetype missing = column - record.fields.size();
        QString empties;
        for (qsizetype i = 0; i < missing; ++i)
            empties += delimiter();
        return patch({record.span.end, record.span.end},
                     empties + delimiter() + csvField(value.text, false));
    }
    if (value.type == DataValue::Type::Nested) {
        JsonReader reader(value.text);
        const std::optional<JsonNode> node = reader.readDocument();
        if (!node ||
            (node->kind != JsonNode::Kind::Object && node->kind != JsonNode::Kind::Array)) {
            if (error)
                *error = node ? QObject::tr("not an object or a list") : reader.error();
            return false;
        }
    }
    if (const Field *found = field(row, column))
        return patch(found->value, jsonValue(value, 2));

    // A new member goes after the field before it in column order, or first.
    const Row &record = m_rows.at(row);
    const QString colon = m_indent.isEmpty() ? u":"_s : u": "_s;
    const QString member = quoted(m_columns.at(column)) + colon + jsonValue(value, 2);
    // A record on one line stays on one line; one over lines gets a line like its others.
    const bool oneLine =
        !m_text.sliced(record.span.start, record.span.end - record.span.start).contains(u'\n');
    QString gap;
    if (m_indent.isEmpty())
        gap = QString();
    else if (oneLine && !record.fields.isEmpty())
        gap = u" "_s;
    else
        gap = newline() + (record.fields.isEmpty() ? lineIndent(record.span.start) + m_indent
                                                   : lineIndent(record.fields.first().key.start));
    if (record.fields.isEmpty()) {
        const QString close =
            m_indent.isEmpty() ? QString() : newline() + lineIndent(record.span.start);
        return patch(record.span, u'{' + gap + member + close + u'}');
    }
    const Field *before = nullptr;
    for (int c = column - 1; c >= 0 && !before; --c)
        before = field(row, c);
    if (before)
        return patch({before->value.end, before->value.end}, u',' + gap + member);
    const qsizetype first = record.fields.first().key.start;
    return patch({first, first}, member + u',' + gap);
}

bool DataTable::clearValue(int row, int column)
{
    const Field *found = field(row, column);
    if (!found)
        return true;
    if (m_kind == u"csv" || m_kind == u"tsv")
        return setValue(row, column, {DataValue::Type::String, {}});
    // The last value in a column leaves the column in place.
    const QString name = m_columns.at(column);
    const bool last = std::ranges::count_if(m_rows, [&name](const Row &each) {
                          return std::ranges::any_of(
                              each.fields, [&name](const Field &f) { return f.name == name; });
                      }) == 1;
    if (last && !m_pending.contains(name))
        m_pending.append(name);
    const Row &record = m_rows.at(row);
    const qsizetype at = found - record.fields.constData();
    if (m_kind == u"yaml") {
        if (found->locked)
            return false;
        const auto removal = yamlRemoval(record, at);
        return removal && patch(removal->first, removal->second);
    }
    if (record.fields.size() == 1)
        return patch(record.span, u"{}"_s);
    if (at + 1 < record.fields.size())
        return patch({found->key.start, record.fields.at(at + 1).key.start}, {});
    return patch({record.fields.at(at - 1).value.end, found->value.end}, {});
}

bool DataTable::insertRow(int row)
{
    if (row < 0 || row > m_rows.size())
        return false;
    const QString nl = newline();
    if (m_kind == u"yaml") {
        // An empty mapping is written "{}" until it has a field.
        if (m_rows.isEmpty())
            return patch(m_array, u"- {}"_s);
        const Row &near = m_rows.at(std::min<qsizetype>(row, m_rows.size() - 1));
        const QString indent =
            m_text.sliced(lineStart(near.dash), near.dash - lineStart(near.dash));
        if (row < m_rows.size())
            return patch({near.dash, near.dash}, u"- {}"_s + nl + indent);
        const qsizetype end = lineEnd(near.span.end);
        return patch({end, end}, nl + indent + u"- {}"_s);
    }
    if (m_kind != u"json") {
        const QString line =
            QString(delimiter()).repeated(std::max<qsizetype>(m_columns.size() - 1, 0));
        if (row < m_rows.size())
            return patch({m_rows.at(row).span.start, m_rows.at(row).span.start}, line + nl);
        const bool endsInNewline = m_text.endsWith(u'\n');
        // An empty line after a final newline would read as no record, so it gets one too.
        if (!endsInNewline && line.isEmpty())
            return patch({m_text.size(), m_text.size()}, nl + nl);
        return patch({m_text.size(), m_text.size()}, endsInNewline ? line + nl : nl + line);
    }
    const QString gap = m_indent.isEmpty() ? QString() : nl + m_indent;
    if (m_rows.isEmpty()) {
        const QString close = m_indent.isEmpty() ? QString() : nl + lineIndent(m_array.start);
        return patch(m_array, u'[' + gap + u"{}"_s + close + u']');
    }
    if (row < m_rows.size())
        return patch({m_rows.at(row).span.start, m_rows.at(row).span.start}, u"{}"_s + u',' + gap);
    return patch({m_rows.last().span.end, m_rows.last().span.end}, u',' + gap + u"{}"_s);
}

bool DataTable::removeRow(int row)
{
    if (row < 0 || row >= m_rows.size())
        return false;
    const Row &record = m_rows.at(row);
    if (m_kind == u"yaml") {
        if (m_rows.size() == 1)
            return patch(m_array, u"[]"_s);
        if (row + 1 < m_rows.size())
            return patch({lineStart(record.dash), lineStart(m_rows.at(row + 1).dash)}, {});
        return patch({lineEnd(m_rows.at(row - 1).span.end), lineEnd(record.span.end)}, {});
    }
    if (m_kind != u"json") {
        const qsizetype next = row + 1 < m_rows.size()
                                   ? m_rows.at(row + 1).span.start
                                   : std::min(m_text.size(), record.span.end + newline().size());
        if (row + 1 >= m_rows.size() && !m_text.sliced(record.span.end).startsWith(u'\n') &&
            !m_text.sliced(record.span.end).startsWith(u"\r\n"_s)) {
            // The last line, with no newline after it, takes the one before it.
            const qsizetype previous = row > 0 ? m_rows.at(row - 1).span.end : m_header.span.end;
            return patch({previous, record.span.end}, {});
        }
        return patch({record.span.start, next}, {});
    }
    if (m_rows.size() == 1)
        return patch(m_array, u"[]"_s);
    if (row + 1 < m_rows.size())
        return patch({record.span.start, m_rows.at(row + 1).span.start}, {});
    return patch({m_rows.at(row - 1).span.end, record.span.end}, {});
}

bool DataTable::addColumn(const QString &name, QString *error)
{
    if (name.isEmpty() || m_columns.contains(name)) {
        if (error)
            *error = name.isEmpty() ? QObject::tr("a column needs a name")
                                    : QObject::tr("there is a column called %1").arg(name);
        return false;
    }
    if (m_kind == u"json" || m_kind == u"yaml") {
        m_pending.append(name);
        m_columns.append(name);
        return true;
    }
    QList<std::pair<Span, QString>> edits;
    const qsizetype width = m_columns.size();
    edits.append({{m_header.span.end, m_header.span.end}, delimiter() + csvField(name, false)});
    for (const Row &record : std::as_const(m_rows)) {
        if (record.fields.size() == width)
            edits.append({{record.span.end, record.span.end}, QString(delimiter())});
    }
    return patches(edits);
}

bool DataTable::removeColumn(int column)
{
    if (column < 0 || column >= m_columns.size())
        return false;
    const QString name = m_columns.at(column);
    m_pending.removeAll(name);
    QList<std::pair<Span, QString>> edits;
    if (m_kind == u"json" || m_kind == u"yaml") {
        for (int row = 0; row < rowCount(); ++row) {
            const Field *found = field(row, column);
            if (!found)
                continue;
            const Row &record = m_rows.at(row);
            const qsizetype at = found - record.fields.constData();
            if (m_kind == u"yaml") {
                if (const auto removal = yamlRemoval(record, at))
                    edits.append(*removal);
            } else if (record.fields.size() == 1)
                edits.append({record.span, u"{}"_s});
            else if (at + 1 < record.fields.size())
                edits.append({{found->key.start, record.fields.at(at + 1).key.start}, {}});
            else
                edits.append({{record.fields.at(at - 1).value.end, found->value.end}, {}});
        }
        if (edits.isEmpty()) {
            m_columns.removeAt(column);
            return true;
        }
        return patches(edits);
    }
    const auto cut = [&edits, column](const Row &record) {
        if (column >= record.fields.size())
            return;
        const Field &here = record.fields.at(column);
        if (record.fields.size() == 1)
            edits.append({here.value, {}});
        else if (column == 0)
            edits.append({{here.value.start, record.fields.at(1).value.start}, {}});
        else
            edits.append({{record.fields.at(column - 1).value.end, here.value.end}, {}});
    };
    cut(m_header);
    for (const Row &record : std::as_const(m_rows))
        cut(record);
    return patches(edits);
}

bool DataTable::renameColumn(int column, const QString &name, QString *error)
{
    if (column < 0 || column >= m_columns.size())
        return false;
    if (name == m_columns.at(column))
        return true;
    if (name.isEmpty() || m_columns.contains(name)) {
        if (error)
            *error = name.isEmpty() ? QObject::tr("a column needs a name")
                                    : QObject::tr("there is a column called %1").arg(name);
        return false;
    }
    const QString old = m_columns.at(column);
    if (const qsizetype pending = m_pending.indexOf(old); pending >= 0)
        m_pending[pending] = name;
    QList<std::pair<Span, QString>> edits;
    if (m_kind == u"json" || m_kind == u"yaml") {
        for (int row = 0; row < rowCount(); ++row) {
            if (const Field *found = field(row, column))
                edits.append({found->key, m_kind == u"yaml" ? yamlScalar(name) : quoted(name)});
        }
        // Renamed where it stands, so the column keeps its place.
        m_columns[column] = name;
        if (edits.isEmpty())
            return true;
    } else if (column < m_header.fields.size()) {
        const Field &header = m_header.fields.at(column);
        edits.append({header.value, csvField(name, header.quoted)});
    }
    return patches(edits);
}

qsizetype DataTable::lineStart(qsizetype at) const
{
    return at <= 0 ? 0 : m_text.lastIndexOf(u'\n', at - 1) + 1;
}

qsizetype DataTable::lineEnd(qsizetype at) const
{
    qsizetype end = m_text.indexOf(u'\n', at);
    if (end < 0)
        end = m_text.size();
    if (end > at && m_text.at(end - 1) == u'\r')
        --end;
    return end;
}

bool DataTable::parseYaml(QString *why)
{
    const QByteArray bytes = m_text.toUtf8();
    // libyaml counts characters, where QString counts UTF-16 units.
    const bool astral = std::ranges::any_of(m_text, [](QChar c) { return c.isSurrogate(); });
    QList<qsizetype> units;
    if (astral) {
        for (qsizetype i = 0; i < m_text.size(); ++i) {
            if (!(m_text.at(i).isLowSurrogate() && i > 0 && m_text.at(i - 1).isHighSurrogate()))
                units.append(i);
        }
        units.append(m_text.size());
    }
    const auto at = [&](const yaml_mark_t &mark) {
        const auto index = qsizetype(mark.index);
        return astral ? units.value(index, m_text.size()) : std::min(index, m_text.size());
    };
    // A value over lines of its own ends at its last word, before blank and comment lines.
    const auto trimmed = [this](qsizetype end) {
        for (;;) {
            while (end > 0 && m_text.at(end - 1).isSpace())
                --end;
            const qsizetype start = lineStart(end);
            if (end == 0 || !m_text.sliced(start, end - start).trimmed().startsWith(u'#'))
                return end;
            end = start;
        }
    };

    yaml_parser_t parser;
    if (!yaml_parser_initialize(&parser)) {
        *why = QObject::tr("YAML could not be read");
        return false;
    }
    yaml_parser_set_input_string(&parser, reinterpret_cast<const unsigned char *>(bytes.data()),
                                 size_t(bytes.size()));
    const auto fail = [&](const QString &reason) {
        *why = reason;
        yaml_parser_delete(&parser);
        return false;
    };
    const QString notRecords = QObject::tr("not a list of records");

    int depth = 0;
    int documents = 0;
    bool root = false;
    bool rootFlow = false;
    bool key = true;
    Row row;
    Field field;
    // A nested value, built as JSON so it can show and be edited as JSON.
    QList<JsonNode> building;
    QList<QString> buildingKeys;
    qsizetype nestedStart = 0;

    const auto scalarNode = [](const yaml_event_t &event) {
        JsonNode node;
        const QString text =
            QString::fromUtf8(reinterpret_cast<const char *>(event.data.scalar.value),
                              qsizetype(event.data.scalar.length));
        const bool plain = event.data.scalar.style == YAML_PLAIN_SCALAR_STYLE;
        switch (plain ? yamlType(text) : DataValue::Type::String) {
        case DataValue::Type::Null:
            node.kind = JsonNode::Kind::Null;
            node.text = u"null"_s;
            break;
        case DataValue::Type::Bool:
            node.kind = JsonNode::Kind::Bool;
            node.text = text.toLower();
            break;
        case DataValue::Type::Number:
            node.kind = JsonNode::Kind::Number;
            node.text = text;
            break;
        default: node.kind = JsonNode::Kind::String; node.text = text;
        }
        return node;
    };
    // Adds a finished nested value to the one around it, under the key waiting there.
    const auto addNested = [&](JsonNode node) {
        JsonNode &parent = building.last();
        if (parent.kind == JsonNode::Kind::Object) {
            if (buildingKeys.last().isNull()) {
                buildingKeys.last() = node.text;
                return;
            }
            parent.members.push_back({buildingKeys.last(), 0, 0, std::move(node)});
            buildingKeys.last() = QString();
        } else {
            parent.items.push_back(std::move(node));
        }
    };

    for (;;) {
        yaml_event_t event;
        if (!yaml_parser_parse(&parser, &event))
            return fail(QString::fromUtf8(parser.problem ? parser.problem : "not valid YAML"));
        const yaml_event_type_t type = event.type;
        const qsizetype start = at(event.start_mark);
        const qsizetype end = at(event.end_mark);
        const bool flow = (type == YAML_MAPPING_START_EVENT &&
                           event.data.mapping_start.style == YAML_FLOW_MAPPING_STYLE) ||
                          (type == YAML_SEQUENCE_START_EVENT &&
                           event.data.sequence_start.style == YAML_FLOW_SEQUENCE_STYLE);
        JsonNode scalar;
        if (type == YAML_SCALAR_EVENT)
            scalar = scalarNode(event);
        QChar quote;
        if (type == YAML_SCALAR_EVENT) {
            if (event.data.scalar.style == YAML_SINGLE_QUOTED_SCALAR_STYLE)
                quote = u'\'';
            else if (event.data.scalar.style == YAML_DOUBLE_QUOTED_SCALAR_STYLE)
                quote = u'"';
        }
        const bool plainScalar =
            type == YAML_SCALAR_EVENT && event.data.scalar.style == YAML_PLAIN_SCALAR_STYLE;
        const bool blockScalar =
            type == YAML_SCALAR_EVENT && (event.data.scalar.style == YAML_LITERAL_SCALAR_STYLE ||
                                          event.data.scalar.style == YAML_FOLDED_SCALAR_STYLE);
        // An anchor or an explicit tag, which an edit of the value alone would lose or break.
        const bool marked =
            (type == YAML_SCALAR_EVENT &&
             (event.data.scalar.anchor ||
              (event.data.scalar.tag && !event.data.scalar.plain_implicit &&
               !event.data.scalar.quoted_implicit))) ||
            (type == YAML_MAPPING_START_EVENT &&
             (event.data.mapping_start.anchor || !event.data.mapping_start.implicit)) ||
            (type == YAML_SEQUENCE_START_EVENT &&
             (event.data.sequence_start.anchor || !event.data.sequence_start.implicit));
        yaml_event_delete(&event);

        switch (type) {
        case YAML_STREAM_END_EVENT:
            yaml_parser_delete(&parser);
            if (!root)
                return fail(notRecords);
            if (rootFlow && !m_rows.isEmpty())
                return fail(QObject::tr("a list written on one line, to edit as text"));
            return true;
        case YAML_DOCUMENT_START_EVENT:
            if (++documents > 1)
                return fail(QObject::tr("more than one YAML document"));
            break;
        case YAML_SEQUENCE_START_EVENT:
        case YAML_MAPPING_START_EVENT: {
            const bool mapping = type == YAML_MAPPING_START_EVENT;
            if (depth == 0) {
                if (mapping)
                    return fail(notRecords);
                root = true;
                rootFlow = flow;
                m_array = {start, start};
            } else if (depth == 1) {
                if (!mapping)
                    return fail(notRecords);
                row = {};
                row.flow = flow;
                row.span.start = start;
                // The item's dash, before the mapping on its line.
                qsizetype dash = start - 1;
                while (dash >= 0 && m_text.at(dash) == u' ')
                    --dash;
                row.dash = dash >= 0 && m_text.at(dash) == u'-' ? dash : start;
                key = true;
            } else if (depth == 2) {
                if (key)
                    return fail(QObject::tr("a key that is not plain text"));
                nestedStart = start;
                field.block = !flow;
                field.locked = marked;
                building.clear();
                buildingKeys.clear();
            }
            if (depth >= 2) {
                JsonNode node;
                node.kind = mapping ? JsonNode::Kind::Object : JsonNode::Kind::Array;
                building.append(node);
                buildingKeys.append(QString());
            }
            ++depth;
            break;
        }
        case YAML_SEQUENCE_END_EVENT:
        case YAML_MAPPING_END_EVENT:
            --depth;
            if (depth >= 2) {
                JsonNode done = building.takeLast();
                buildingKeys.removeLast();
                if (depth > 2) {
                    addNested(std::move(done));
                    break;
                }
                field.value = {nestedStart, field.block ? trimmed(start) : end};
                field.data = {DataValue::Type::Nested, compact(done)};
                row.fields.append(field);
                if (!m_columns.contains(field.name))
                    m_columns.append(field.name);
                key = true;
            } else if (depth == 1) {
                row.span.end = row.flow               ? end
                               : row.fields.isEmpty() ? row.span.start
                                                      : row.fields.last().value.end;
                m_rows.append(row);
            } else if (depth == 0) {
                m_array.end = rootFlow ? end : trimmed(start);
            }
            break;
        case YAML_SCALAR_EVENT:
        case YAML_ALIAS_EVENT:
            if (depth < 2)
                return fail(notRecords);
            if (depth > 2) {
                addNested(type == YAML_ALIAS_EVENT ? JsonNode{} : scalar);
                break;
            }
            if (key) {
                if (type == YAML_ALIAS_EVENT)
                    return fail(QObject::tr("a key that is not plain text"));
                field = {};
                field.name = scalar.text;
                field.key = {start, end};
                key = false;
                break;
            }
            // A | or > scalar's end runs on to the next line, which is not its.
            field.value = {start, blockScalar ? trimmed(end) : end};
            field.quote = quote;
            field.block = blockScalar;
            field.locked = marked || type == YAML_ALIAS_EVENT;
            if (type == YAML_ALIAS_EVENT)
                field.data = {DataValue::Type::String, m_text.sliced(start, end - start)};
            else if (!plainScalar)
                field.data = {DataValue::Type::String, scalar.text};
            else
                field.data = {yamlType(m_text.sliced(start, end - start)),
                              m_text.sliced(start, end - start)};
            row.fields.append(field);
            if (!m_columns.contains(field.name))
                m_columns.append(field.name);
            key = true;
            break;
        default: break;
        }
    }
}

QString DataTable::yamlValue(const DataValue &value, const Field *was) const
{
    switch (value.type) {
    case DataValue::Type::String:
        if (was && was->quote == u'\'')
            return u'\'' + QString(value.text).replace(u"'"_s, u"''"_s) + u'\'';
        // Quoted wherever plain text would read as a number, a boolean or null.
        if ((was && was->quote == u'"') || yamlType(value.text) != DataValue::Type::String)
            return quoted(value.text);
        return yamlScalar(value.text);
    case DataValue::Type::Missing:
    case DataValue::Type::Null: return u"null"_s;
    case DataValue::Type::Nested: {
        JsonReader reader(value.text);
        const std::optional<JsonNode> node = reader.readDocument();
        return node ? compact(*node) : value.text;
    }
    default: return value.text;
    }
}

bool DataTable::yamlSetValue(int row, int column, const DataValue &value, QString *error)
{
    const Row &record = m_rows.at(row);
    const QString text = yamlValue(value, field(row, column));
    if (const Field *found = field(row, column)) {
        if (found->locked) {
            if (error)
                *error = QObject::tr("it has an anchor, a tag or an alias; edit it as text");
            return false;
        }
        // A value over lines of its own, or none written, is replaced from its colon on.
        if (value.type == DataValue::Type::Nested || found->block ||
            found->value.start == found->value.end)
            return patch({found->key.end, found->value.end}, u": "_s + text);
        return patch(found->value, text);
    }
    const QString member = yamlScalar(m_columns.at(column)) + u": "_s + text;
    if (record.fields.isEmpty()) {
        // An empty "- {}" takes the field as its first line.
        return patch(record.span, member);
    }
    const Field *before = nullptr;
    for (int c = column - 1; c >= 0 && !before; --c)
        before = field(row, c);
    if (record.flow) {
        if (before)
            return patch({before->value.end, before->value.end}, u", "_s + member);
        const qsizetype first = record.fields.first().key.start;
        return patch({first, first}, member + u", "_s);
    }
    // Indented as the record's keys are, the dash taken for a space.
    const qsizetype firstKey = record.fields.first().key.start;
    QString indent = m_text.sliced(lineStart(firstKey), firstKey - lineStart(firstKey));
    indent.replace(u'-', u' ');
    if (before) {
        const qsizetype end = lineEnd(before->value.end);
        return patch({end, end}, newline() + indent + member);
    }
    return patch({firstKey, firstKey}, member + newline() + indent);
}

std::optional<std::pair<DataTable::Span, QString>> DataTable::yamlRemoval(const Row &record,
                                                                          qsizetype at) const
{
    const Field &here = record.fields.at(at);
    if (record.fields.size() == 1)
        return std::pair{Span{here.key.start, here.value.end}, record.flow ? QString() : u"{}"_s};
    if (record.flow) {
        if (at + 1 < record.fields.size())
            return std::pair{Span{here.key.start, record.fields.at(at + 1).key.start}, QString()};
        return std::pair{Span{record.fields.at(at - 1).value.end, here.value.end}, QString()};
    }
    // The first field shares its line with the dash, so the next one moves up onto it.
    if (at == 0)
        return std::pair{Span{here.key.start, record.fields.at(1).key.start}, QString()};
    return std::pair{Span{lineEnd(record.fields.at(at - 1).value.end), lineEnd(here.value.end)},
                     QString()};
}

} // namespace jot
