#include "jot/TextObjects.h"

#include <QList>

using namespace Qt::StringLiterals;

namespace jot {

namespace {

using Range = std::optional<std::pair<int, int>>;

bool blankSpace(QChar c)
{
    return c == u' ' || c == u'\t';
}

int lineStart(const QString &text, int position)
{
    return position <= 0 ? 0 : int(text.lastIndexOf(u'\n', position - 1)) + 1;
}

int lineEnd(const QString &text, int position)
{
    const qsizetype end = text.indexOf(u'\n', position);
    return end < 0 ? int(text.size()) : int(end);
}

/// Takes in the spaces after [start, end) on its line, or those before it when there are none
/// after, as vim's `a` does for a word or quoted text.
std::pair<int, int> withSpace(const QString &text, int start, int end)
{
    int after = end;
    while (after < text.size() && blankSpace(text.at(after)))
        ++after;
    if (after > end)
        return {start, after};
    int before = start;
    while (before > lineStart(text, start) && blankSpace(text.at(before - 1)))
        --before;
    return {before, end};
}

Range word(const QString &text, int position, bool big, bool around)
{
    if (text.isEmpty())
        return std::nullopt;
    position = std::min(position, int(text.size()) - 1);
    if (text.at(position) == u'\n')
        return std::nullopt;
    // 0 for space, 1 for a word's letters, 2 for other marks; WORDs are anything but space.
    const auto kind = [big](QChar c) {
        if (blankSpace(c))
            return 0;
        if (big)
            return 1;
        return c.isLetterOrNumber() || c == u'_' ? 1 : 2;
    };
    const int here = kind(text.at(position));
    const int from = lineStart(text, position);
    const int to = lineEnd(text, position);
    int start = position;
    while (start > from && kind(text.at(start - 1)) == here)
        --start;
    int end = position + 1;
    while (end < to && kind(text.at(end)) == here)
        ++end;
    if (!around)
        return std::pair{start, end};
    if (here == 0) {
        // Space, and the word after it.
        if (end < to) {
            const int next = kind(text.at(end));
            while (end < to && kind(text.at(end)) == next)
                ++end;
        }
        return std::pair{start, end};
    }
    return withSpace(text, start, end);
}

Range quoted(const QString &text, int position, QChar quote, bool around)
{
    const int from = lineStart(text, position);
    const int to = lineEnd(text, position);
    QList<int> quotes;
    for (int i = from; i < to; ++i) {
        if (text.at(i) == quote && (i == from || text.at(i - 1) != u'\\'))
            quotes.append(i);
    }
    // The pair the cursor is on or in, or else the first pair after it.
    int open = -1;
    for (qsizetype i = 0; i + 1 < quotes.size(); i += 2) {
        if (position >= quotes.at(i) && position <= quotes.at(i + 1)) {
            open = int(i);
            break;
        }
        if (quotes.at(i) > position) {
            open = int(i);
            break;
        }
    }
    if (open < 0)
        return std::nullopt;
    const int start = quotes.at(open);
    const int end = quotes.at(open + 1);
    if (!around)
        return std::pair{start + 1, end};
    return withSpace(text, start, end + 1);
}

Range bracketed(const QString &text, int position, QChar open, QChar close, bool around)
{
    if (text.isEmpty())
        return std::nullopt;
    position = std::min(position, int(text.size()) - 1);
    // The innermost open bracket around the cursor, or the one it is on.
    int start = -1;
    if (text.at(position) == open) {
        start = position;
    } else {
        int depth = 0;
        for (int i = text.at(position) == close ? position - 1 : position; i >= 0; --i) {
            if (text.at(i) == close) {
                ++depth;
            } else if (text.at(i) == open) {
                if (depth == 0) {
                    start = i;
                    break;
                }
                --depth;
            }
        }
    }
    if (start < 0)
        return std::nullopt;
    int end = -1;
    int depth = 0;
    for (int i = start + 1; i < text.size(); ++i) {
        if (text.at(i) == open) {
            ++depth;
        } else if (text.at(i) == close) {
            if (depth == 0) {
                end = i;
                break;
            }
            --depth;
        }
    }
    if (end < 0)
        return std::nullopt;
    if (around)
        return std::pair{start, end + 1};
    // Inside a pair on lines of their own, the lines between, not the line breaks around them.
    int inner = start + 1;
    int last = end;
    if (inner < last && text.at(inner) == u'\n')
        ++inner;
    const int closeLine = lineStart(text, end);
    if (closeLine > inner && text.mid(closeLine, end - closeLine).trimmed().isEmpty())
        last = closeLine;
    return std::pair{inner, std::max(inner, last)};
}

Range paragraph(const QString &text, int position, bool around)
{
    if (text.isEmpty())
        return std::nullopt;
    position = std::min(position, int(text.size()) - 1);
    const auto blank = [&text](int lineAt) {
        return text.mid(lineAt, lineEnd(text, lineAt) - lineAt).trimmed().isEmpty();
    };
    // The line after the one at `lineAt`, or the text's end.
    const auto next = [&text](int lineAt) {
        const int end = lineEnd(text, lineAt);
        return end < text.size() ? end + 1 : int(text.size());
    };
    const int here = lineStart(text, position);
    const bool empty = blank(here);
    int start = here;
    while (start > 0 && blank(lineStart(text, start - 1)) == empty)
        start = lineStart(text, start - 1);
    int end = next(here);
    while (end < text.size() && blank(end) == empty)
        end = next(end);
    if (!around)
        return std::pair{start, end};
    // The blank lines after it, or for blank lines, the paragraph after them; with none
    // after, the blank lines before.
    const int stop = end;
    while (end < text.size() && blank(end) != empty)
        end = next(end);
    if (end == stop && !empty) {
        while (start > 0 && blank(lineStart(text, start - 1)))
            start = lineStart(text, start - 1);
    }
    return std::pair{start, end};
}

} // namespace

std::optional<std::pair<int, int>> textObject(const QString &text, int position, QChar kind,
                                              bool around)
{
    if (position < 0 || position > text.size())
        return std::nullopt;
    switch (kind.unicode()) {
    case 'w': return word(text, position, false, around);
    case 'W': return word(text, position, true, around);
    case '"':
    case '\'':
    case '`': return quoted(text, position, kind, around);
    case '(':
    case ')':
    case 'b': return bracketed(text, position, u'(', u')', around);
    case '[':
    case ']': return bracketed(text, position, u'[', u']', around);
    case '{':
    case '}':
    case 'B': return bracketed(text, position, u'{', u'}', around);
    case '<':
    case '>': return bracketed(text, position, u'<', u'>', around);
    case 'p': return paragraph(text, position, around);
    default: return std::nullopt;
    }
}

} // namespace jot
