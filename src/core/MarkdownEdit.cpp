#include "jot/MarkdownEdit.h"

#include "jot/FrontMatter.h"

#include <QList>
#include <QObject>
#include <QRegularExpression>

#include <algorithm>
#include <ranges>

using namespace Qt::StringLiterals;

namespace jot::markdownedit {

namespace {

qsizetype lineStartOf(const QString &text, qsizetype position)
{
    return position <= 0 ? 0 : text.lastIndexOf(u'\n', position - 1) + 1;
}

qsizetype lineEndOf(const QString &text, qsizetype position)
{
    const qsizetype end = text.indexOf(u'\n', position);
    return end < 0 ? text.size() : end;
}

bool isWordChar(QChar c)
{
    return c.isLetterOrNumber() || c == u'_' || c == u'\'';
}

qsizetype runBefore(const QString &text, qsizetype position, QChar c, qsizetype limit)
{
    qsizetype n = 0;
    while (position - n - 1 >= limit && text.at(position - n - 1) == c)
        ++n;
    return n;
}

qsizetype runAfter(const QString &text, qsizetype position, QChar c, qsizetype limit)
{
    qsizetype n = 0;
    while (position + n < limit && text.at(position + n) == c)
        ++n;
    return n;
}

/// Whether runs of `n` asterisks on both sides already make `marker`: odd runs are
/// italic and runs of two or more are bold, so `***` is both.
bool starsMake(qsizetype n, const QString &marker)
{
    return marker.size() == 1 ? n % 2 == 1 : n >= 2;
}

/// Where `marker` already is: around the text from `s` to `e`, inside its ends, or neither.
enum class Placement { None, Around, Inside };

Placement placement(const QString &text, qsizetype s, qsizetype e, const QString &marker)
{
    const qsizetype m = marker.size();
    if (marker.startsWith(u'*')) {
        const qsizetype around =
            std::min(runBefore(text, s, u'*', 0), runAfter(text, e, u'*', text.size()));
        if (around > 0 && starsMake(around, marker))
            return Placement::Around;
        const qsizetype middle = s + (e - s) / 2;
        const qsizetype inside =
            std::min(runAfter(text, s, u'*', middle), runBefore(text, e, u'*', middle));
        if (inside > 0 && starsMake(inside, marker) && e - s > 2 * inside)
            return Placement::Inside;
        return Placement::None;
    }
    if (s >= m && text.mid(s - m, m) == marker && text.mid(e, m) == marker)
        return Placement::Around;
    if (e - s > 2 * m && text.mid(s, m) == marker && text.mid(e - m, m) == marker)
        return Placement::Inside;
    return Placement::None;
}

struct Wrapped
{
    QString text;
    qsizetype start;
    qsizetype end;
};

/// Adds or removes `marker` at the text from `s` to `e`, giving where that text is after.
Wrapped toggle(QString text, qsizetype s, qsizetype e, const QString &marker, bool add)
{
    const qsizetype m = marker.size();
    const Placement at = placement(text, s, e, marker);
    if (!add && at == Placement::Around) {
        text.remove(e, m);
        text.remove(s - m, m);
        return {text, s - m, e - m};
    }
    if (!add && at == Placement::Inside) {
        text.remove(e - m, m);
        text.remove(s, m);
        return {text, s, e - 2 * m};
    }
    if (add && at == Placement::None) {
        text.insert(e, marker);
        text.insert(s, marker);
        return {text, s + m, e + m};
    }
    return {text, s, e};
}

/// The part of a selection on one line, without the spaces at its ends.
struct Segment
{
    qsizetype start;
    qsizetype end;
};

Edit wrapLines(const QString &text, qsizetype from, qsizetype to, const QString &marker)
{
    QList<Segment> segments;
    for (qsizetype start = from; start < to;) {
        const qsizetype end = std::min(lineEndOf(text, start), to);
        qsizetype s = start, e = end;
        while (s < e && text.at(s).isSpace())
            ++s;
        while (e > s && text.at(e - 1).isSpace())
            --e;
        if (s < e)
            segments.append({s, e});
        start = end + 1;
    }
    const bool all = std::ranges::all_of(segments, [&](const Segment &segment) {
        return placement(text, segment.start, segment.end, marker) != Placement::None;
    });
    QString changed = text;
    // From the end, so the segments before keep their places.
    for (const Segment &segment : std::views::reverse(segments))
        changed = toggle(changed, segment.start, segment.end, marker, !all).text;
    return difference(text, changed, from, to + changed.size() - text.size());
}

/// A line split into the parts a line style changes.
struct Line
{
    QString indent;
    QString quote;
    /// A list marker or heading marks, with the space after.
    QString block;
    QString content;

    enum Kind { Plain, Bullet, Numbered, Task, Heading };
    Kind kind = Plain;
    int level = 0;

    [[nodiscard]] bool blank() const { return block.isEmpty() && content.trimmed().isEmpty(); }
    [[nodiscard]] qsizetype prefix() const { return indent.size() + quote.size() + block.size(); }
    [[nodiscard]] QString joined() const { return indent + quote + block + content; }
};

Line parse(const QString &text)
{
    static const QRegularExpression parts(
        uR"(^([ \t]*)(> ?)?((#{1,6})(?: |$)|[-*+] \[[ xX]\] |[-*+] |\d+[.)] )?(.*)$)"_s);
    const QRegularExpressionMatch match = parts.match(text);
    Line line{match.captured(1), match.captured(2), match.captured(3), match.captured(5)};
    if (!match.captured(4).isEmpty()) {
        line.kind = Line::Heading;
        line.level = int(match.captured(4).size());
    } else if (line.block.contains(u'[')) {
        line.kind = Line::Task;
    } else if (!line.block.isEmpty()) {
        line.kind = line.block.front().isDigit() ? Line::Numbered : Line::Bullet;
    }
    return line;
}

/// How far a nested item is indented: under the text of the item above it.
qsizetype nestUnit(const Line &line)
{
    return line.kind == Line::Numbered ? line.block.size() : 2;
}

bool has(const Line &line, LineStyle style)
{
    switch (style) {
    case LineStyle::Bullet: return line.kind == Line::Bullet;
    case LineStyle::Numbered: return line.kind == Line::Numbered;
    case LineStyle::Task: return line.kind == Line::Task;
    case LineStyle::Quote: return !line.quote.isEmpty();
    case LineStyle::Heading1: return line.kind == Line::Heading && line.level == 1;
    case LineStyle::Heading2: return line.kind == Line::Heading && line.level == 2;
    case LineStyle::Heading3: return line.kind == Line::Heading && line.level == 3;
    }
    return false;
}

void apply(Line &line, LineStyle style, bool remove, int number)
{
    if (style == LineStyle::Quote) {
        line.quote = remove ? QString() : u"> "_s;
        return;
    }
    if (remove) {
        line.block.clear();
        line.kind = Line::Plain;
        return;
    }
    switch (style) {
    case LineStyle::Bullet: line.block = u"- "_s; break;
    case LineStyle::Numbered: line.block = QString::number(number) + u". "_s; break;
    case LineStyle::Task: line.block = u"- [ ] "_s; break;
    case LineStyle::Heading1:
    case LineStyle::Heading2:
    case LineStyle::Heading3:
        line.block = QString(int(style) - int(LineStyle::Heading1) + 1, u'#') + u' ';
        break;
    case LineStyle::Quote: break;
    }
}

bool isFence(const QString &text, qsizetype start)
{
    return text.mid(start, lineEndOf(text, start) - start).trimmed().startsWith(u"```"_s);
}

/// Where the code block that the line at `start` sits inside opens, or -1. Fences pair up
/// from the top of the note, so it depends on how many come before the line, not on its
/// neighbors alone.
qsizetype openFenceBefore(const QString &text, qsizetype start)
{
    qsizetype open = -1;
    for (qsizetype line = 0; line < start; line = lineEndOf(text, line) + 1) {
        if (isFence(text, line))
            open = open < 0 ? line : -1;
    }
    return open;
}

/// Whether the line at `start` is code: a fence, or inside a fenced block.
bool inCode(const QString &text, qsizetype start)
{
    return isFence(text, start) || openFenceBefore(text, start) >= 0;
}

/// A cell of a table row: the text between its pipes, and its text without the spaces
/// around it, as positions in the line.
struct Cell
{
    qsizetype start;
    qsizetype end;
    qsizetype textStart;
    qsizetype textEnd;
};

/// The cells of a table row. Pipes in backticks or after a backslash belong to the text.
QList<Cell> cellsOf(const QString &line)
{
    QList<qsizetype> cuts;
    bool code = false;
    for (qsizetype i = 0; i < line.size(); ++i) {
        const QChar c = line.at(i);
        if (c == u'\\') {
            ++i;
        } else if (c == u'`') {
            code = !code;
        } else if (c == u'|' && !code) {
            cuts.append(i);
        }
    }
    qsizetype indent = 0;
    while (indent < line.size() && line.at(indent).isSpace())
        ++indent;
    qsizetype last = line.size() - 1;
    while (last > indent && line.at(last).isSpace())
        --last;
    // Outer pipes are optional; without them the row runs to the line's ends. An escaped
    // pipe at the end is the last cell's text, not its closing pipe.
    if (cuts.isEmpty() || cuts.first() != indent)
        cuts.prepend(indent - 1);
    if (cuts.size() < 2 || cuts.last() != last)
        cuts.append(line.size());
    QList<Cell> cells;
    for (qsizetype k = 0; k + 1 < cuts.size(); ++k) {
        Cell cell{cuts[k] + 1, cuts[k + 1], cuts[k] + 1, cuts[k + 1]};
        while (cell.textStart < cell.textEnd && line.at(cell.textStart).isSpace())
            ++cell.textStart;
        while (cell.textEnd > cell.textStart && line.at(cell.textEnd - 1).isSpace())
            --cell.textEnd;
        cells.append(cell);
    }
    return cells;
}

bool isDelimiterRow(const QString &line)
{
    static const QRegularExpression delimiter(uR"(^\s*\|?\s*:?-+:?\s*(\|\s*:?-+:?\s*)*\|?\s*$)"_s);
    return line.contains(u'-') && delimiter.match(line).hasMatch();
}

/// The table a position is in: its lines, from the heading row down, and where they are.
struct Table
{
    qsizetype start = 0;
    qsizetype end = 0;
    QString indent;
    QStringList lines;
    /// Each row's cells as text, the delimiter row's included.
    QList<QStringList> rows;
    /// The row and column the position is in, and how far into the cell's text.
    qsizetype row = 0;
    qsizetype column = 0;
    qsizetype offset = 0;
};

std::optional<Table> tableAt(const QString &text, qsizetype position)
{
    const auto isRow = [&](qsizetype start) {
        const QString line = text.mid(start, lineEndOf(text, start) - start);
        return line.contains(u'|') && !line.trimmed().isEmpty() && !inCode(text, start);
    };
    const qsizetype here = lineStartOf(text, position);
    if (!isRow(here))
        return std::nullopt;
    Table table;
    table.start = here;
    while (table.start > 0 && isRow(lineStartOf(text, table.start - 1)))
        table.start = lineStartOf(text, table.start - 1);
    table.end = lineEndOf(text, here);
    while (table.end < text.size() && isRow(table.end + 1))
        table.end = lineEndOf(text, table.end + 1);
    table.lines = text.mid(table.start, table.end - table.start).split(u'\n');
    if (table.lines.size() < 2 || !isDelimiterRow(table.lines[1]))
        return std::nullopt;

    const QString &first = table.lines.front();
    table.indent = first.left(first.indexOf(first.trimmed()));
    qsizetype lineStart = table.start;
    for (qsizetype r = 0; r < table.lines.size(); ++r) {
        const QString &line = table.lines[r];
        const QList<Cell> cells = cellsOf(line);
        QStringList row;
        for (const Cell &cell : cells)
            row.append(line.mid(cell.textStart, cell.textEnd - cell.textStart));
        table.rows.append(row);
        if (here == lineStart) {
            table.row = r;
            const qsizetype at = position - lineStart;
            for (qsizetype c = 0; c < cells.size(); ++c) {
                if (at <= cells[c].end || c + 1 == cells.size()) {
                    table.column = c;
                    table.offset = std::clamp(at - cells[c].textStart, qsizetype(0),
                                              cells[c].textEnd - cells[c].textStart);
                    break;
                }
            }
        }
        lineStart += line.size() + 1;
    }
    return table;
}

/// A table's lines, padded so the pipes align, and where each cell's text starts in them,
/// measured from the table's start.
struct Laid
{
    QString text;
    QList<QList<qsizetype>> cells;
};

Laid layOut(const QString &indent, const QList<QStringList> &rows)
{
    qsizetype columns = 0;
    for (const QStringList &row : rows)
        columns = std::max(columns, row.size());
    QList<qsizetype> widths(columns, 3);
    for (qsizetype r = 0; r < rows.size(); ++r) {
        if (r == 1)
            continue;
        for (qsizetype c = 0; c < rows[r].size(); ++c)
            widths[c] = std::max(widths[c], rows[r][c].size());
    }
    // A colon at either end of a delimiter cell sets its column's alignment.
    QList<QString> aligns(columns);
    for (qsizetype c = 0; c < rows[1].size(); ++c) {
        const QString &cell = rows[1][c];
        aligns[c] = QString(cell.startsWith(u':') ? u"l"_s : u""_s) +
                    QString(cell.endsWith(u':') ? u"r"_s : u""_s);
    }

    Laid laid;
    QStringList lines;
    qsizetype at = 0;
    for (qsizetype r = 0; r < rows.size(); ++r) {
        QString line = indent + u'|';
        QList<qsizetype> starts;
        for (qsizetype c = 0; c < columns; ++c) {
            const qsizetype width = widths[c];
            QString cell;
            qsizetype lead = 0;
            if (r == 1) {
                const bool left = aligns[c].contains(u'l');
                const bool right = aligns[c].contains(u'r');
                cell = (left ? u":"_s : QString()) +
                       QString(width - (left ? 1 : 0) - (right ? 1 : 0), u'-') +
                       (right ? u":"_s : QString());
            } else {
                const QString value = rows[r].value(c);
                const qsizetype room = width - value.size();
                lead = aligns[c] == u"r" ? room : aligns[c] == u"lr" ? room / 2 : 0;
                cell = QString(lead, u' ') + value + QString(room - lead, u' ');
            }
            line += u' ';
            starts.append(at + line.size() + lead);
            line += cell + u" |"_s;
        }
        laid.cells.append(starts);
        lines.append(line);
        at += line.size() + 1;
    }
    laid.text = lines.join(u'\n');
    return laid;
}

} // namespace

Edit difference(const QString &before, const QString &after, qsizetype selectionStart,
                qsizetype selectionEnd)
{
    const qsizetype shortest = std::min(before.size(), after.size());
    qsizetype prefix = 0;
    while (prefix < shortest && before.at(prefix) == after.at(prefix))
        ++prefix;
    qsizetype suffix = 0;
    while (suffix < shortest - prefix &&
           before.at(before.size() - suffix - 1) == after.at(after.size() - suffix - 1))
        ++suffix;
    return {prefix, before.size() - suffix, after.mid(prefix, after.size() - suffix - prefix),
            selectionStart, selectionEnd};
}

Edit wrap(const QString &text, qsizetype from, qsizetype to, const QString &marker)
{
    from = std::clamp<qsizetype>(from, 0, text.size());
    to = std::clamp<qsizetype>(to, 0, text.size());
    if (from > to)
        std::swap(from, to);
    const qsizetype m = marker.size();
    if (text.mid(from, to - from).contains(u'\n'))
        return wrapLines(text, from, to, marker);

    if (from == to) {
        qsizetype s = from, e = to;
        while (s > 0 && isWordChar(text.at(s - 1)))
            --s;
        while (e < text.size() && isWordChar(text.at(e)))
            ++e;
        if (s == e) {
            // A pair with nothing in it is taken away again; otherwise one is made to type in.
            if (from >= m && text.mid(from - m, m) == marker && text.mid(from, m) == marker) {
                QString changed = text;
                changed.remove(from - m, 2 * m);
                return difference(text, changed, from - m, from - m);
            }
            return difference(text, text.left(from) + marker + marker + text.mid(from), from + m,
                              from + m);
        }
        const bool add = placement(text, s, e, marker) == Placement::None;
        const Wrapped done = toggle(text, s, e, marker, add);
        const qsizetype cursor = done.start + (from - s);
        return difference(text, done.text, cursor, cursor);
    }

    qsizetype s = from, e = to;
    while (s < e && text.at(s).isSpace())
        ++s;
    while (e > s && text.at(e - 1).isSpace())
        --e;
    if (s == e)
        return difference(text, text, from, to);
    const bool add = placement(text, s, e, marker) == Placement::None;
    const Wrapped done = toggle(text, s, e, marker, add);
    return difference(text, done.text, done.start, done.end);
}

Edit styleLines(const QString &text, qsizetype from, qsizetype to, LineStyle style)
{
    from = std::clamp<qsizetype>(from, 0, text.size());
    to = std::clamp<qsizetype>(to, 0, text.size());
    if (from > to)
        std::swap(from, to);
    // A selection that ends at the start of a line leaves that line alone.
    const qsizetype last = to > from && to == lineStartOf(text, to) ? to - 1 : to;
    const qsizetype first = lineStartOf(text, from);
    const qsizetype end = lineEndOf(text, last);

    const QStringList raw = text.mid(first, end - first).split(u'\n');
    QList<Line> lines;
    for (const QString &line : raw)
        lines.append(parse(line));
    const bool allBlank = std::ranges::all_of(lines, &Line::blank);
    const auto touched = [allBlank](const Line &line) { return allBlank || !line.blank(); };
    const bool remove = std::ranges::all_of(
        lines, [&](const Line &line) { return !touched(line) || has(line, style); });

    QStringList out;
    QList<qsizetype> oldStarts, newStarts, oldPrefixes, newPrefixes, indents;
    qsizetype oldAt = first, newAt = first;
    int number = 1;
    for (Line line : std::as_const(lines)) {
        oldStarts.append(oldAt);
        newStarts.append(newAt);
        oldPrefixes.append(line.prefix());
        indents.append(line.indent.size());
        oldAt += line.joined().size() + 1;
        if (touched(line))
            apply(line, style, remove, number++);
        newPrefixes.append(line.prefix());
        out.append(line.joined());
        newAt += out.last().size() + 1;
    }
    const QString changed = text.left(first) + out.join(u'\n') + text.mid(end);

    const auto map = [&](qsizetype position) {
        if (position < first)
            return position;
        if (position > end)
            return position + changed.size() - text.size();
        qsizetype i = oldStarts.size() - 1;
        while (i > 0 && oldStarts[i] > position)
            --i;
        const qsizetype offset = position - oldStarts[i];
        if (offset < indents[i])
            return newStarts[i] + offset;
        return newStarts[i] + newPrefixes[i] + std::max<qsizetype>(0, offset - oldPrefixes[i]);
    };
    // A selection from the start of its first line keeps the marks it gains.
    const qsizetype start = from < to && from == first ? from : map(from);
    return difference(text, changed, start, map(to));
}

Edit link(const QString &text, qsizetype from, qsizetype to)
{
    from = std::clamp<qsizetype>(from, 0, text.size());
    to = std::clamp<qsizetype>(to, 0, text.size());
    if (from > to)
        std::swap(from, to);
    static const QRegularExpression url(uR"(^(?:[a-z][a-z0-9+.-]*://|mailto:)\S+$)"_s,
                                        QRegularExpression::CaseInsensitiveOption);
    const QString selected = text.mid(from, to - from);
    const bool isUrl = url.match(selected).hasMatch();
    const QString made = isUrl ? u"[]("_s + selected + u')' : u'[' + selected + u"]()"_s;
    // The cursor goes where the half still missing is typed: the words for a URL, otherwise the
    // target, where notes are offered.
    const qsizetype cursor = isUrl ? from + 1 : from + selected.size() + 3;
    return difference(text, text.left(from) + made + text.mid(to), cursor, cursor);
}

Edit codeBlock(const QString &text, qsizetype from, qsizetype to)
{
    from = std::clamp<qsizetype>(from, 0, text.size());
    to = std::clamp<qsizetype>(to, 0, text.size());
    if (from > to)
        std::swap(from, to);
    const qsizetype last = to > from && to == lineStartOf(text, to) ? to - 1 : to;
    const qsizetype first = lineStartOf(text, from);
    const qsizetype end = lineEndOf(text, last);
    const auto fence = [&](qsizetype start) { return isFence(text, start); };
    qsizetype open = openFenceBefore(text, first);
    if (open < 0 && fence(first))
        open = first;
    if (open >= 0) {
        qsizetype close = -1;
        for (qsizetype line = open == first ? lineEndOf(text, first) + 1 : first;
             line < text.size(); line = lineEndOf(text, line) + 1) {
            if (fence(line)) {
                close = line;
                break;
            }
        }
        const qsizetype opening = std::min(lineEndOf(text, open) + 1, text.size()) - open;
        QString changed = text;
        // The closing fence goes with the line break before it; an unclosed block loses only
        // its opening.
        if (close > 0)
            changed.remove(close - 1, lineEndOf(text, close) - close + 1);
        changed.remove(open, opening);
        const auto map = [&](qsizetype position) {
            return std::max(open, position >= open + opening ? position - opening : open);
        };
        return difference(text, changed, std::min(map(from), qsizetype(changed.size())),
                          std::min(map(to), qsizetype(changed.size())));
    }
    const QString changed =
        text.left(first) + u"```\n"_s + text.mid(first, end - first) + u"\n```"_s + text.mid(end);
    return difference(text, changed, from + 4, to + 4);
}

std::optional<Edit> continueList(const QString &text, qsizetype from, qsizetype to)
{
    from = std::clamp<qsizetype>(from, 0, text.size());
    to = std::clamp<qsizetype>(to, 0, text.size());
    if (from > to)
        std::swap(from, to);
    const qsizetype start = lineStartOf(text, from);
    // A selection over several lines is replaced as a plain line break would replace it.
    if (to > lineEndOf(text, from))
        return std::nullopt;
    if (inCode(text, start))
        return std::nullopt;
    const Line line = parse(text.mid(start, lineEndOf(text, from) - start));
    const bool listed =
        line.kind == Line::Bullet || line.kind == Line::Numbered || line.kind == Line::Task;
    if ((!listed && line.quote.isEmpty()) || from - start < line.prefix())
        return std::nullopt;

    if (line.content.trimmed().isEmpty()) {
        // An empty item ends the list, or steps out of a nested one first.
        Line ended = line;
        if (!ended.indent.isEmpty() && listed) {
            // As far as Tab nested it, so one Return undoes one Tab.
            const qsizetype unit = ended.indent.startsWith(u'\t') ? 1 : nestUnit(ended);
            ended.indent.remove(0, std::min(unit, ended.indent.size()));
        } else if (listed) {
            ended.block.clear();
        } else {
            ended.quote.clear();
        }
        const qsizetype end = lineEndOf(text, from);
        const QString changed = text.left(start) + ended.joined() + text.mid(end);
        const qsizetype cursor = start + ended.prefix();
        return difference(text, changed, cursor, cursor);
    }

    // Before another item's marker on the same line, the line only breaks: that item keeps
    // its own marker rather than gaining a second.
    const qsizetype end = lineEndOf(text, from);
    if (const Line rest = parse(text.mid(to, end - to).trimmed());
        listed &&
        (rest.kind == Line::Bullet || rest.kind == Line::Numbered || rest.kind == Line::Task)) {
        const QString broken = u'\n' + line.indent + line.quote;
        const qsizetype marker =
            to + (text.mid(to, end - to).size() - text.mid(to, end - to).trimmed().size());
        const QString changed = text.left(from) + broken + text.mid(marker);
        const qsizetype cursor = from + broken.size();
        return difference(text, changed, cursor, cursor);
    }

    // A quoted heading carries on as quoted text, not as another heading.
    QString block = listed ? line.block : QString();
    if (line.kind == Line::Task) {
        block = block.left(2) + u"[ ] "_s;
    } else if (line.kind == Line::Numbered) {
        const qsizetype digits = block.indexOf(QRegularExpression(u"[.)]"_s));
        block = QString::number(block.left(digits).toInt() + 1) + block.mid(digits);
    }
    const QString next = u'\n' + line.indent + line.quote + block;
    const QString changed = text.left(from) + next + text.mid(to);
    const qsizetype cursor = from + next.size();
    return difference(text, changed, cursor, cursor);
}

std::optional<Edit> indentList(const QString &text, qsizetype from, qsizetype to, bool outdent)
{
    from = std::clamp<qsizetype>(from, 0, text.size());
    to = std::clamp<qsizetype>(to, 0, text.size());
    if (from > to)
        std::swap(from, to);
    const qsizetype last = to > from && to == lineStartOf(text, to) ? to - 1 : to;
    const qsizetype first = lineStartOf(text, from);
    const qsizetype end = lineEndOf(text, last);

    QStringList out;
    // How far each line's start moved, to carry the selection along.
    QList<qsizetype> starts, shifts;
    qsizetype at = first;
    if (inCode(text, first))
        return std::nullopt;
    for (const QString &raw : text.mid(first, end - first).split(u'\n')) {
        Line line = parse(raw);
        // Nesting inside a quote would need its indent after the marker, which the line
        // parser does not keep, so quoted items are left to Tab's usual behavior.
        if ((line.kind != Line::Bullet && line.kind != Line::Numbered && line.kind != Line::Task) ||
            !line.quote.isEmpty())
            return std::nullopt;
        const qsizetype unit = nestUnit(line);
        const qsizetype before = line.indent.size();
        if (!outdent) {
            line.indent += QString(unit, u' ');
        } else if (line.indent.startsWith(u'\t')) {
            line.indent.remove(0, 1);
        } else {
            qsizetype spaces = 0;
            while (spaces < std::min(unit, line.indent.size()) && line.indent.at(spaces) == u' ')
                ++spaces;
            line.indent.remove(0, spaces);
        }
        starts.append(at);
        shifts.append(line.indent.size() - before);
        at += raw.size() + 1;
        out.append(line.joined());
    }
    const QString changed = text.left(first) + out.join(u'\n') + text.mid(end);
    const auto map = [&](qsizetype position) {
        qsizetype i = starts.size() - 1;
        while (i > 0 && starts[i] > position)
            --i;
        qsizetype moved = 0;
        for (qsizetype j = 0; j < i; ++j)
            moved += shifts[j];
        // Indenting carries the whole line along; outdenting pulls a position inside the
        // removed indent back to the line's start.
        const qsizetype shift = shifts[i];
        return position + moved + (shift >= 0 ? shift : std::max(shift, starts[i] - position));
    };
    // A selection from the start of its first line keeps the indent it gains.
    const qsizetype start = from < to && from == first ? from : map(from);
    return difference(text, changed, start, map(to));
}

std::optional<Edit> formatTable(const QString &text, qsizetype from, qsizetype to)
{
    from = std::clamp<qsizetype>(from, 0, text.size());
    to = std::clamp<qsizetype>(to, 0, text.size());
    if (from > to)
        std::swap(from, to);
    const std::optional<Table> table = tableAt(text, from);
    if (!table)
        return std::nullopt;
    const Laid laid = layOut(table->indent, table->rows);
    const QString changed = text.left(table->start) + laid.text + text.mid(table->end);
    const auto place = [&](const Table &at) {
        return table->start + laid.cells[at.row][at.column] + at.offset;
    };
    const qsizetype start = place(*table);
    // A selection that leaves the table ends at the cursor.
    const std::optional<Table> other = to <= table->end ? tableAt(text, to) : std::nullopt;
    return difference(text, changed, start, other ? place(*other) : start);
}

std::optional<Edit> moveInTable(const QString &text, qsizetype from, bool backwards)
{
    from = std::clamp<qsizetype>(from, 0, text.size());
    const std::optional<Table> table = tableAt(text, from);
    if (!table)
        return std::nullopt;
    QList<QStringList> rows = table->rows;
    qsizetype columns = 0;
    for (const QStringList &row : rows)
        columns = std::max(columns, row.size());
    qsizetype row = table->row, column = table->column;
    if (backwards) {
        if (--column < 0) {
            column = columns - 1;
            row = row == 2 ? 0 : row - 1;
        }
        if (row < 0) {
            row = 0;
            column = 0;
        }
    } else {
        if (++column >= columns) {
            column = 0;
            row = row == 0 ? 2 : row + 1;
        }
        if (row >= rows.size())
            rows.append(QStringList(columns, QString()));
    }
    if (row == 1)
        row = backwards ? 0 : 2;
    const Laid laid = layOut(table->indent, rows);
    const QString changed = text.left(table->start) + laid.text + text.mid(table->end);
    const qsizetype start = table->start + laid.cells[row][column];
    return difference(text, changed, start, start + rows[row].value(column).size());
}

std::optional<Edit> newTableRow(const QString &text, qsizetype from)
{
    from = std::clamp<qsizetype>(from, 0, text.size());
    const std::optional<Table> table = tableAt(text, from);
    if (!table)
        return std::nullopt;
    QList<QStringList> rows = table->rows;
    const bool last = table->row == rows.size() - 1;
    const bool empty = table->row >= 2 && std::ranges::all_of(rows[table->row], &QString::isEmpty);
    if (last && empty) {
        // An empty last row ends the table, becoming the blank line after it.
        rows.removeLast();
        const Laid laid = layOut(table->indent, rows);
        const QString changed = text.left(table->start) + laid.text + u'\n' + text.mid(table->end);
        const qsizetype cursor = table->start + laid.text.size() + 1;
        return difference(text, changed, cursor, cursor);
    }
    qsizetype columns = 0;
    for (const QStringList &row : std::as_const(rows))
        columns = std::max(columns, row.size());
    // Under the heading, the new row goes below the delimiter.
    const qsizetype at = std::max<qsizetype>(table->row + 1, 2);
    rows.insert(at, QStringList(columns, QString()));
    const Laid laid = layOut(table->indent, rows);
    const QString changed = text.left(table->start) + laid.text + text.mid(table->end);
    const qsizetype cursor = table->start + laid.cells[at][0];
    return difference(text, changed, cursor, cursor);
}

Edit insertTable(const QString &text, qsizetype from, qsizetype to)
{
    from = std::clamp<qsizetype>(from, 0, text.size());
    to = std::clamp<qsizetype>(to, 0, text.size());
    if (from > to)
        std::swap(from, to);
    const auto blankLine = [&](qsizetype lineStart) {
        return text.mid(lineStart, lineEndOf(text, lineStart) - lineStart).trimmed().isEmpty();
    };
    const QString heading = QObject::tr("Column");
    const Laid laid = layOut({}, {{heading, heading}, {u"---"_s, u"---"_s}, {{}, {}}});

    // On a blank line the table takes its place; otherwise it goes after the line. Either
    // way a blank line keeps it apart from the text around it.
    const qsizetype start = lineStartOf(text, from);
    const qsizetype end = lineEndOf(text, to);
    // Only a selection that stays on one blank line gives that line up to the table.
    const bool blank = lineStartOf(text, to) == start && blankLine(start);
    const qsizetype at = blank ? start : end;
    const bool textBefore = blank ? start > 0 && !blankLine(lineStartOf(text, start - 1)) : true;
    const bool textAfter = end < text.size() && !blankLine(end + 1);
    const QString before = !textBefore ? QString() : blank ? u"\n"_s : u"\n\n"_s;
    const QString changed =
        text.left(at) + before + laid.text + (textAfter ? u"\n"_s : QString()) + text.mid(end);
    const qsizetype selection = at + before.size() + laid.cells[0][0];
    return difference(text, changed, selection, selection + heading.size());
}

std::optional<LinkTargetQuery> linkTargetAt(const QString &text, qsizetype cursor)
{
    cursor = std::clamp<qsizetype>(cursor, 0, text.size());
    const qsizetype line = lineStartOf(text, cursor);
    if (inCode(text, line))
        return std::nullopt;
    const qsizetype open = text.lastIndexOf(u"]("_s, cursor - 1);
    if (open < line || open + 2 > cursor)
        return std::nullopt;
    const QString typed = text.mid(open + 2, cursor - open - 2);
    static const QRegularExpression ends(uR"([()#]|\]\(|^[A-Za-z][A-Za-z0-9+.-]*:)"_s);
    if (typed.contains(ends))
        return std::nullopt;
    // The words' opening bracket, on the same line; an image's is no note link.
    const qsizetype bracket = text.lastIndexOf(u'[', open);
    if (bracket < line || text.mid(bracket + 1, open - bracket - 1).contains(u']') ||
        (bracket > line && text.at(bracket - 1) == u'!'))
        return std::nullopt;
    // Backticks before it on the line, unpaired, put it in inline code.
    if (text.mid(line, bracket - line).count(u'`') % 2 == 1)
        return std::nullopt;
    return LinkTargetQuery{open + 2, typed, bracket + 1, open};
}

std::optional<EmojiQuery> emojiAt(const QString &text, qsizetype cursor)
{
    cursor = std::clamp<qsizetype>(cursor, 0, text.size());
    const qsizetype line = lineStartOf(text, cursor);
    qsizetype colon = cursor;
    const auto namePart = [](QChar c) {
        return c.isLetterOrNumber() || c == u'_' || c == u'+' || c == u'-';
    };
    while (colon > line && namePart(text.at(colon - 1)))
        --colon;
    if (colon == line || text.at(colon - 1) != u':' || cursor - colon < 2)
        return std::nullopt;
    --colon;
    static const QString openers = u"([{\"'"_s;
    if (colon > line && !text.at(colon - 1).isSpace() && !openers.contains(text.at(colon - 1)))
        return std::nullopt;
    if (inCode(text, line) || text.mid(line, colon - line).count(u'`') % 2 == 1)
        return std::nullopt;
    return EmojiQuery{colon, text.mid(colon + 1, cursor - colon - 1).replace(u'_', u' ')};
}

std::optional<EmojiQuery> markerAt(const QString &text, qsizetype cursor)
{
    cursor = std::clamp<qsizetype>(cursor, 0, text.size());
    const qsizetype line = lineStartOf(text, cursor);
    qsizetype at = cursor;
    while (at > line && text.at(at - 1).isLetterOrNumber())
        --at;
    if (at == line || text.at(at - 1) != u'@')
        return std::nullopt;
    --at;
    static const QRegularExpression box(uR"(^\s*(?:[-*+]|\d+[.)])\s+\[[ xX]\]\s)"_s);
    const QRegularExpressionMatch m = box.match(text.mid(line, at - line + 1));
    if (!m.hasMatch() || at < line + m.capturedLength() ||
        (at > line && !text.at(at - 1).isSpace()))
        return std::nullopt;
    if (inCode(text, line) || text.mid(line, at - line).count(u'`') % 2 == 1)
        return std::nullopt;
    return EmojiQuery{at, text.mid(at + 1, cursor - at - 1)};
}

std::optional<Edit> completeLinkTarget(const QString &text, qsizetype cursor, const QString &target,
                                       const QString &words)
{
    const std::optional<LinkTargetQuery> query = linkTargetAt(text, cursor);
    if (!query)
        return std::nullopt;
    const qsizetype lineEnd = lineEndOf(text, cursor);
    // A `)` ends this target only with nothing between that a target never holds, so a later
    // link's on the line is left to it.
    const qsizetype close = text.indexOf(u')', cursor);
    static const QRegularExpression foreign(uR"([\[\s])"_s);
    const bool closed =
        close >= 0 && close <= lineEnd && !text.mid(cursor, close - cursor).contains(foreign);
    // In a closed link, the rest of the target after the cursor is replaced too, but not a
    // `#heading` after it. An open one is closed where the cursor is.
    qsizetype end = cursor;
    if (closed) {
        while (end < close && text.at(end) != u'#')
            ++end;
    }
    QString label;
    if (query->labelStart == query->labelEnd) {
        label = words;
        label.replace(u'[', u"\\["_s).replace(u']', u"\\]"_s);
    }
    const QString changed = text.left(query->labelStart) + label +
                            text.mid(query->labelStart, query->start - query->labelStart) + target +
                            (closed ? QString() : u")"_s) + text.mid(end);
    // The cursor goes after the whole link, its heading included.
    const qsizetype after =
        label.size() + (closed ? close + 1 - (end - query->start) + target.size()
                               : query->start + target.size() + 1);
    return difference(text, changed, after, after);
}

Edit setKey(const QString &text, qsizetype from, qsizetype to, const QString &key,
            const QVariant &value)
{
    const QString changed = setFrontMatterKey(text, key, value);
    const Edit edit = difference(text, changed, 0, 0);
    const qsizetype delta = changed.size() - text.size();
    const auto map = [&](qsizetype position) {
        if (position <= edit.start)
            return position;
        if (position >= edit.end)
            return position + delta;
        return edit.start + edit.text.size();
    };
    return difference(text, changed, map(from), map(to));
}

} // namespace jot::markdownedit
