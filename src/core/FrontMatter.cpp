#include "jot/FrontMatter.h"

#include <QRegularExpression>

using namespace Qt::StringLiterals;

namespace jot {

namespace {

constexpr char16_t kByteOrderMark = 0xfeff;

// The line starting at `from`, without its line break, and where the next line starts.
QStringView lineAt(QStringView text, qsizetype from, qsizetype *next)
{
    qsizetype end = text.indexOf(u'\n', from);
    *next = end < 0 ? text.size() : end + 1;
    if (end < 0)
        end = text.size();
    if (end > from && text[end - 1] == u'\r')
        --end;
    return text.sliced(from, end - from);
}

bool isFence(QStringView line, bool closing)
{
    const QStringView trimmed = line.trimmed();
    return trimmed == u"---" || (closing && trimmed == u"...");
}

/// Where the front matter's YAML starts and ends in `note`, or -1 for a note without any.
struct Span
{
    qsizetype start = -1;
    qsizetype end = -1;
    qsizetype bodyStart = 0;
};

Span frontMatterSpan(QStringView note)
{
    const qsizetype offset = note.startsWith(kByteOrderMark) ? 1 : 0;
    qsizetype next = 0;
    if (!note.sliced(offset).startsWith(u"---") || !isFence(lineAt(note, offset, &next), false))
        return {-1, -1, offset};

    const qsizetype yamlStart = next;
    while (next < note.size()) {
        const qsizetype lineStart = next;
        if (isFence(lineAt(note, lineStart, &next), true))
            return {yamlStart, lineStart, next};
    }
    return {-1, -1, offset};
}

bool isIndented(QStringView line)
{
    return line.startsWith(u' ') || line.startsWith(u'\t');
}

bool isListItem(QStringView line)
{
    return line == u"-" || line.startsWith(u"- ");
}

/// The key a top-level `key: value` line sets, or empty for anything else.
QString keyOf(QStringView line)
{
    if (line.isEmpty() || isIndented(line) || line.startsWith(u'#') || isListItem(line))
        return {};
    const qsizetype colon = line.indexOf(u':');
    if (colon <= 0 ||
        (colon + 1 < line.size() && line[colon + 1] != u' ' && line[colon + 1] != u'\t'))
        return {};
    QString key = line.first(colon).trimmed().toString();
    if (key.size() >= 2 && (key.startsWith(u'"') || key.startsWith(u'\'')) &&
        key.endsWith(key.front()))
        key = key.mid(1, key.size() - 2);
    return key;
}

/// A scalar as written after a key or a list dash, comments and quotes removed.
QString readScalar(QStringView text)
{
    text = text.trimmed();
    if (text.startsWith(u'"')) {
        QString value;
        for (qsizetype i = 1; i < text.size(); ++i) {
            const QChar c = text[i];
            if (c == u'"')
                return value;
            if (c != u'\\' || i + 1 >= text.size()) {
                value += c;
                continue;
            }
            const QChar escaped = text[++i];
            switch (escaped.unicode()) {
            case 'n': value += u'\n'; break;
            case 't': value += u'\t'; break;
            case 'r': value += u'\r'; break;
            case '0': value += QChar(0); break;
            case 'u':
                if (i + 4 < text.size()) {
                    value += QChar(text.sliced(i + 1, 4).toUShort(nullptr, 16));
                    i += 4;
                }
                break;
            default: value += escaped;
            }
        }
        return value;
    }
    if (text.startsWith(u'\'')) {
        QString value;
        for (qsizetype i = 1; i < text.size(); ++i) {
            if (text[i] != u'\'') {
                value += text[i];
            } else if (i + 1 < text.size() && text[i + 1] == u'\'') {
                value += u'\'';
                ++i;
            } else {
                break;
            }
        }
        return value;
    }
    // A plain scalar ends at a comment, which needs a space before its #.
    const qsizetype comment = text.indexOf(u" #");
    return (comment < 0 ? text : text.first(comment)).trimmed().toString();
}

/// The items of a flow sequence such as `[a, "b, c"]`.
QStringList readFlowList(QStringView text)
{
    text = text.trimmed();
    const qsizetype close = text.lastIndexOf(u']');
    text = text.sliced(1, (close > 0 ? close : text.size()) - 1);
    QStringList items;
    qsizetype start = 0;
    QChar quote;
    for (qsizetype i = 0; i <= text.size(); ++i) {
        const QChar c = i < text.size() ? text[i] : QChar(u',');
        if (!quote.isNull()) {
            if (c == quote)
                quote = QChar();
            else if (c == u'\\' && quote == u'"')
                ++i;
        } else if (c == u'"' || c == u'\'') {
            quote = c;
        } else if (c == u',') {
            const QString item = readScalar(text.sliced(start, i - start));
            if (!item.isEmpty() || i < text.size())
                items.append(item);
            start = i + 1;
        }
    }
    if (!items.isEmpty() && items.last().isEmpty())
        items.removeLast();
    return items;
}

QStringList linesOf(QStringView text)
{
    QStringList lines;
    qsizetype next = 0;
    while (next < text.size()) {
        const qsizetype start = next;
        lines.append(lineAt(text, start, &next).toString());
    }
    return lines;
}

bool needsQuotes(const QString &text)
{
    static const QRegularExpression special(u"^(true|false|yes|no|on|off|null|~)$"_s,
                                            QRegularExpression::CaseInsensitiveOption);
    static const QString indicators = u"-?:,[]{}#&*!|>'\"%@`"_s;
    return text.isEmpty() || indicators.contains(text.front()) || text.front().isSpace() ||
           text.back().isSpace() || text.contains(u": "_s) || text.contains(u" #"_s) ||
           text.endsWith(u':') || special.match(text).hasMatch();
}

} // namespace

QString readYamlScalar(QStringView text)
{
    return readScalar(text);
}

NoteParts splitFrontMatter(QStringView note)
{
    const Span span = frontMatterSpan(note);
    if (span.start < 0)
        return {{}, note.sliced(span.bodyStart), false};
    return {note.sliced(span.start, span.end - span.start), note.sliced(span.bodyStart), true};
}

QVariantMap readFrontMatter(QStringView yaml)
{
    QVariantMap values;
    const QStringList lines = linesOf(yaml);
    for (qsizetype i = 0; i < lines.size(); ++i) {
        const QString key = keyOf(lines.at(i));
        if (key.isEmpty())
            continue;
        const QStringView rest = QStringView(lines.at(i)).sliced(lines.at(i).indexOf(u':') + 1);
        const QStringView value = rest.trimmed();
        if (value.startsWith(u'[')) {
            values.insert(key, readFlowList(value));
            continue;
        }
        if (!value.isEmpty() && !value.startsWith(u'#')) {
            values.insert(key, readScalar(value));
            continue;
        }
        // A value on the lines below: a list of scalars, or something kept as text.
        QStringList block;
        while (i + 1 < lines.size() &&
               (isIndented(lines.at(i + 1)) || isListItem(lines.at(i + 1)) ||
                lines.at(i + 1).trimmed().isEmpty())) {
            block.append(lines.at(++i));
        }
        while (!block.isEmpty() && block.last().trimmed().isEmpty())
            block.removeLast();
        QStringList items;
        bool list = !block.isEmpty();
        for (const QString &line : std::as_const(block)) {
            const QString trimmed = line.trimmed();
            if (trimmed.isEmpty() || trimmed.startsWith(u'#'))
                continue;
            if (!isListItem(trimmed) || trimmed.mid(1).trimmed().contains(u": "_s)) {
                list = false;
                break;
            }
            items.append(readScalar(QStringView(trimmed).sliced(1)));
        }
        if (list)
            values.insert(key, items);
        else
            values.insert(key, block.join(u'\n'));
    }
    return values;
}

QString yamlScalar(const QString &text)
{
    const bool control = std::any_of(text.cbegin(), text.cend(), [](QChar c) {
        return c.unicode() < 0x20 || c.unicode() == 0x7f;
    });
    if (!control && !needsQuotes(text))
        return text;
    QString quoted = u"\""_s;
    for (const QChar c : text) {
        switch (c.unicode()) {
        case '"': quoted += u"\\\""_s; break;
        case '\\': quoted += u"\\\\"_s; break;
        case '\n': quoted += u"\\n"_s; break;
        case '\t': quoted += u"\\t"_s; break;
        case '\r': quoted += u"\\r"_s; break;
        default:
            if (c.unicode() < 0x20 || c.unicode() == 0x7f)
                quoted += u"\\u%1"_s.arg(uint(c.unicode()), 4, 16, QChar(u'0'));
            else
                quoted += c;
        }
    }
    return quoted + u'"';
}

QString setFrontMatterKey(const QString &note, const QString &key, const QVariant &value)
{
    const bool remove = !value.isValid() || value.isNull();
    const QString newline = note.contains(u"\r\n"_s) ? u"\r\n"_s : u"\n"_s;
    const QString yamlKey = needsQuotes(key) ? yamlScalar(key) : key;
    const auto valueLines = [&](const QString &indent, bool block) {
        if (value.typeId() != QMetaType::QStringList)
            return QStringList{yamlKey + u": "_s + yamlScalar(value.toString())};
        const QStringList items = value.toStringList();
        if (!block || items.isEmpty()) {
            QStringList quoted;
            for (const QString &item : items) {
                const QString scalar = yamlScalar(item);
                // Inside [ ], a comma or bracket would end the item.
                quoted.append(scalar == item && (item.contains(u',') || item.contains(u']'))
                                  ? u"\""_s + item + u'"'
                                  : scalar);
            }
            return QStringList{yamlKey + u": ["_s + quoted.join(u", "_s) + u']'};
        }
        QStringList lines{yamlKey + u':'};
        for (const QString &item : items)
            lines.append(indent + u"- "_s + yamlScalar(item));
        return lines;
    };

    const Span span = frontMatterSpan(note);
    if (span.start < 0) {
        if (remove)
            return note;
        const QString prefix = note.first(span.bodyStart);
        return prefix + u"---"_s + newline + valueLines({}, false).join(newline) + newline +
               u"---"_s + newline + note.sliced(span.bodyStart);
    }

    QStringList lines = linesOf(QStringView(note).sliced(span.start, span.end - span.start));
    qsizetype at = -1;
    for (qsizetype i = 0; i < lines.size() && at < 0; ++i) {
        if (keyOf(lines.at(i)) == key)
            at = i;
    }
    if (at < 0) {
        if (remove)
            return note;
        lines.append(valueLines({}, false));
    } else {
        // The key's own lines: its value may continue on indented or list lines below.
        qsizetype end = at + 1;
        QString indent;
        bool block = false;
        while (end < lines.size() && (isIndented(lines.at(end)) || isListItem(lines.at(end)))) {
            if (!block && isListItem(lines.at(end).trimmed())) {
                block = true;
                indent = lines.at(end).left(lines.at(end).indexOf(u'-'));
            }
            ++end;
        }
        lines.remove(at, end - at);
        if (!remove) {
            const QStringList replacement = valueLines(indent, block);
            for (qsizetype i = 0; i < replacement.size(); ++i)
                lines.insert(at + i, replacement.at(i));
        }
    }

    QString yaml = lines.join(newline);
    if (!yaml.isEmpty())
        yaml += newline;
    return note.first(span.start) + yaml + note.sliced(span.end);
}

} // namespace jot
