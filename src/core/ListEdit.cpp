#include "jot/ListEdit.h"

#include <QCollator>
#include <QList>
#include <QRegularExpression>
#include <QStringList>

#include <algorithm>
#include <functional>

using namespace Qt::StringLiterals;

namespace jot {

namespace {

/// A list item's line: its indent, its marker, a checkbox if any, and its words.
const QRegularExpression &itemPattern()
{
    static const QRegularExpression item(
        uR"(^([ \t]*)([-*+]|(\d+)([.)]))([ \t]+)(\[([ xX])\][ \t]+)?(.*)$)"_s);
    return item;
}

int indentOf(const QString &line)
{
    int width = 0;
    for (const QChar c : line) {
        if (c == u' ')
            ++width;
        else if (c == u'\t')
            width += 4;
        else
            break;
    }
    return width;
}

bool blank(const QString &line)
{
    return line.trimmed().isEmpty();
}

/// An item, as lines `[start, end)` of the text: its own and those under it.
struct Item
{
    int start;
    int end;
};

/// What to do with one list's items, siblings at one indent, in order: their new order, by
/// index, leaving out any to drop. Children are done before their list.
using ListChange = std::function<QList<int>(const QStringList &lines, const QList<Item> &items)>;

class Lists
{
public:
    explicit Lists(const QString &body)
    {
        m_finalBreak = body.endsWith(u'\n');
        m_lines = body.split(u'\n');
        if (m_finalBreak)
            m_lines.removeLast();
        // Code is not lists, however it looks.
        bool fenced = false;
        m_code.resize(m_lines.size());
        for (int i = 0; i < m_lines.size(); ++i) {
            const QString trimmed = m_lines.at(i).trimmed();
            const bool fence = trimmed.startsWith(u"```") || trimmed.startsWith(u"~~~");
            m_code[i] = fenced || fence;
            if (fence)
                fenced = !fenced;
        }
    }

    [[nodiscard]] bool isItem(int line) const
    {
        return line < m_lines.size() && !m_code.at(line) &&
               itemPattern().match(m_lines.at(line)).hasMatch();
    }

    /// Applies `change` to each list in lines `[from, to)`, innermost first.
    QStringList apply(int from, int to, const ListChange &change)
    {
        QStringList out;
        int i = from;
        while (i < to) {
            if (!isItem(i)) {
                out.append(m_lines.at(i));
                ++i;
                continue;
            }
            // The siblings of this item: items at its indent until something less indented.
            const int depth = indentOf(m_lines.at(i));
            QList<Item> items;
            int j = i;
            while (j < to && isItem(j) && indentOf(m_lines.at(j)) == depth) {
                int end = j + 1;
                while (end < to && (blank(m_lines.at(end)) || (indentOf(m_lines.at(end)) > depth)))
                    ++end;
                // Blank lines after the last item belong after the list, not in it.
                int last = end;
                while (last > j + 1 && blank(m_lines.at(last - 1)))
                    --last;
                const bool sibling = end < to && isItem(end) && indentOf(m_lines.at(end)) == depth;
                items.append({j, sibling ? end : last});
                j = sibling ? end : last;
                if (!sibling)
                    break;
            }
            // Each item's own lines, with the lists under it done first.
            QStringList texts;
            QList<Item> local;
            QStringList lines;
            for (const Item &item : std::as_const(items)) {
                const QStringList under = apply(item.start + 1, item.end, change);
                const int at = int(lines.size());
                lines.append(m_lines.at(item.start));
                lines.append(under);
                local.append({at, int(lines.size())});
            }
            // A numbered list keeps the number it starts at, whichever item comes first now.
            const QRegularExpressionMatch firstItem = itemPattern().match(lines.first());
            const int startsAt =
                firstItem.captured(3).isEmpty() ? 1 : firstItem.captured(3).toInt();
            const QList<int> order = change(lines, local);
            QStringList listed;
            for (const int index : order) {
                for (int k = local.at(index).start; k < local.at(index).end; ++k)
                    listed.append(lines.at(k));
            }
            renumber(listed, depth, startsAt);
            out.append(listed);
            i = j;
        }
        return out;
    }

    [[nodiscard]] QString text(const QStringList &lines) const
    {
        if (lines.isEmpty())
            return {};
        return lines.join(u'\n') + (m_finalBreak ? u"\n"_s : QString());
    }

    [[nodiscard]] int lineCount() const { return int(m_lines.size()); }

private:
    /// Numbers a numbered list's items again from its first number, after a change of order.
    static void renumber(QStringList &lines, int depth, int startsAt)
    {
        int number = startsAt;
        for (QString &line : lines) {
            const QRegularExpressionMatch match = itemPattern().match(line);
            if (!match.hasMatch() || indentOf(line) != depth)
                continue;
            if (match.captured(3).isEmpty())
                return;
            line.replace(match.capturedStart(3), match.capturedLength(3), QString::number(number));
            ++number;
        }
    }

    QStringList m_lines;
    QList<bool> m_code;
    bool m_finalBreak = false;
};

bool ticked(const QString &line)
{
    const QRegularExpressionMatch match = itemPattern().match(line);
    return match.hasMatch() && match.captured(7).compare(u"x"_s, Qt::CaseInsensitive) == 0;
}

} // namespace

QString moveTickedToEnd(const QString &body)
{
    Lists lists(body);
    return lists.text(
        lists.apply(0, lists.lineCount(), [](const QStringList &lines, const QList<Item> &items) {
            QList<int> open;
            QList<int> done;
            for (int i = 0; i < items.size(); ++i)
                (ticked(lines.at(items.at(i).start)) ? done : open).append(i);
            return open + done;
        }));
}

QString removeTicked(const QString &body)
{
    Lists lists(body);
    return lists.text(
        lists.apply(0, lists.lineCount(), [](const QStringList &lines, const QList<Item> &items) {
            QList<int> kept;
            for (int i = 0; i < items.size(); ++i) {
                if (!ticked(lines.at(items.at(i).start)))
                    kept.append(i);
            }
            return kept;
        }));
}

QString untickAll(const QString &body)
{
    Lists lists(body);
    QStringList lines = body.split(u'\n');
    for (int i = 0; i < lines.size(); ++i) {
        if (!lists.isItem(i))
            continue;
        const QRegularExpressionMatch match = itemPattern().match(lines.at(i));
        if (ticked(lines.at(i)))
            lines[i].replace(match.capturedStart(7), 1, u' ');
    }
    return lines.join(u'\n');
}

QString sortList(const QString &body, int position)
{
    Lists lists(body);
    const int line =
        int(QStringView(body).first(std::clamp(position, 0, int(body.size()))).count(u'\n'));
    // Lists inside are done first, so once the one holding the line sorts, those around it are
    // left as they are.
    bool sorted = false;
    QCollator collator;
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    collator.setNumericMode(true);
    const QStringList original = body.split(u'\n');
    const auto words = [](const QString &itemLine) {
        return itemPattern().match(itemLine).captured(8).trimmed();
    };
    // The innermost list with an item on the line or over it, known to the change below by its
    // first item's line and indent.
    int first = -1;
    int depth = -1;
    for (int i = std::min(line, lists.lineCount() - 1); i >= 0; --i) {
        if (!lists.isItem(i))
            continue;
        const int d = indentOf(original.at(i));
        // An item at or above the line, with the line on it or indented under it.
        bool holds = i == line;
        if (!holds) {
            holds = true;
            for (int k = i + 1; k <= line; ++k) {
                if (!blank(original.at(k)) && indentOf(original.at(k)) <= d) {
                    holds = false;
                    break;
                }
            }
        }
        if (holds) {
            depth = d;
            // Back to the first of its siblings.
            first = i;
            for (int k = i - 1; k >= 0; --k) {
                if (lists.isItem(k) && indentOf(original.at(k)) == d) {
                    first = k;
                } else if (!blank(original.at(k)) && indentOf(original.at(k)) <= d) {
                    break;
                }
            }
            break;
        }
    }
    if (first < 0)
        return body;
    const QString firstLine = original.at(first);
    return lists.text(
        lists.apply(0, lists.lineCount(), [&](const QStringList &lines, const QList<Item> &items) {
            QList<int> order;
            for (int i = 0; i < items.size(); ++i)
                order.append(i);
            if (sorted || lines.at(items.first().start) != firstLine ||
                indentOf(lines.at(items.first().start)) != depth)
                return order;
            sorted = true;
            std::ranges::stable_sort(order, [&](int a, int b) {
                return collator.compare(words(lines.at(items.at(a).start)),
                                        words(lines.at(items.at(b).start))) < 0;
            });
            return order;
        }));
}

} // namespace jot
