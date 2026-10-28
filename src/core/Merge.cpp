#include "jot/Merge.h"

#include <QList>
#include <QStringList>

#include <algorithm>

namespace jot {

namespace {

/// Past this many cells the comparison is too slow to run while someone types.
constexpr qsizetype kMaxCells = 4'000'000;

/// A run of `base` lines, from `start` up to `end`, replaced by `lines`.
struct Hunk
{
    qsizetype start = 0;
    qsizetype end = 0;
    QStringList lines;

    bool operator==(const Hunk &) const = default;
};

/// The text's lines, each with its line break, so joining them gives the text back.
QStringList linesOf(const QString &text)
{
    QStringList lines;
    qsizetype from = 0;
    while (from < text.size()) {
        const qsizetype newline = text.indexOf(u'\n', from);
        const qsizetype to = newline < 0 ? text.size() : newline + 1;
        lines.append(text.sliced(from, to - from));
        from = to;
    }
    return lines;
}

/// What changed from `base` to `other`, by a longest common subsequence of lines, or nothing
/// when that would take too long.
std::optional<QList<Hunk>> changes(const QStringList &base, const QStringList &other)
{
    // Lines the two share at the start and the end need no comparing.
    qsizetype head = 0;
    while (head < base.size() && head < other.size() && base[head] == other[head])
        ++head;
    qsizetype tail = 0;
    while (tail < base.size() - head && tail < other.size() - head &&
           base[base.size() - 1 - tail] == other[other.size() - 1 - tail])
        ++tail;
    const qsizetype n = base.size() - head - tail;
    const qsizetype m = other.size() - head - tail;
    if ((n + 1) * (m + 1) > kMaxCells)
        return std::nullopt;

    // lengths[i][j]: the longest common run of base[i..] and other[j..], within the middle.
    QList<int> lengths((n + 1) * (m + 1), 0);
    const auto at = [&](qsizetype i, qsizetype j) -> int & { return lengths[i * (m + 1) + j]; };
    for (qsizetype i = n - 1; i >= 0; --i) {
        for (qsizetype j = m - 1; j >= 0; --j) {
            at(i, j) = base[head + i] == other[head + j] ? at(i + 1, j + 1) + 1
                                                         : std::max(at(i + 1, j), at(i, j + 1));
        }
    }
    QList<Hunk> hunks;
    qsizetype i = 0;
    qsizetype j = 0;
    while (i < n || j < m) {
        if (i < n && j < m && base[head + i] == other[head + j]) {
            ++i;
            ++j;
            continue;
        }
        Hunk hunk{head + i, head + i, {}};
        while ((i < n || j < m) && !(i < n && j < m && base[head + i] == other[head + j])) {
            if (j < m && (i == n || at(i, j + 1) >= at(i + 1, j)))
                hunk.lines.append(other[head + j++]);
            else
                ++i;
        }
        hunk.end = head + i;
        hunks.append(hunk);
    }
    return hunks;
}

/// Whether two changes would touch the same lines, or lines next to each other.
bool clash(const Hunk &a, const Hunk &b)
{
    return a.start <= b.end && b.start <= a.end;
}

} // namespace

std::optional<QString> mergeText(const QString &base, const QString &mine, const QString &theirs)
{
    if (mine == theirs || theirs == base)
        return mine;
    if (mine == base)
        return theirs;
    const QStringList original = linesOf(base);
    const std::optional<QList<Hunk>> ours = changes(original, linesOf(mine));
    const std::optional<QList<Hunk>> others = changes(original, linesOf(theirs));
    if (!ours || !others)
        return std::nullopt;

    QList<Hunk> all = *ours;
    for (const Hunk &other : *others) {
        bool same = false;
        for (const Hunk &own : *ours) {
            if (own == other)
                same = true;
            else if (clash(own, other))
                return std::nullopt;
        }
        if (!same)
            all.append(other);
    }
    std::ranges::stable_sort(all, {}, &Hunk::start);

    QString merged;
    qsizetype line = 0;
    for (const Hunk &hunk : std::as_const(all)) {
        for (; line < hunk.start; ++line)
            merged += original[line];
        merged += hunk.lines.join(QString());
        line = hunk.end;
    }
    for (; line < original.size(); ++line)
        merged += original[line];
    return merged;
}

int movedPosition(const QString &before, const QString &after, int position)
{
    if (before == after)
        return position;
    const QStringList original = linesOf(before);
    const std::optional<QList<Hunk>> hunks = changes(original, linesOf(after));
    if (!hunks)
        return std::min<int>(position, int(after.size()));
    // Where each line of `before` starts, so line changes become character ones.
    QList<qsizetype> starts{0};
    for (const QString &line : original)
        starts.append(starts.last() + line.size());
    qsizetype shift = 0;
    for (const Hunk &hunk : *hunks) {
        const qsizetype from = starts[hunk.start];
        const qsizetype to = starts[hunk.end];
        const qsizetype length = hunk.lines.join(QString()).size();
        if (to <= position) {
            shift += length - (to - from);
        } else if (from < position) {
            // Inside what changed: at the end of what replaced it, before its line break.
            const bool newline = length > 0 && hunk.lines.last().endsWith(u'\n');
            return int(from + shift + length - (newline ? 1 : 0));
        } else {
            break;
        }
    }
    return int(std::clamp<qsizetype>(position + shift, 0, after.size()));
}

} // namespace jot
