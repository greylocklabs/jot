#pragma once

#include <QList>
#include <QString>

#include <functional>
#include <optional>

namespace jot {

/// The notes opened, in order, to step back and forward through as a browser does, with where
/// the cursor was in each when it was left.
class NoteTrail
{
public:
    struct Visit
    {
        QString path;
        int cursor = 0;

        bool operator==(const Visit &) const = default;
    };

    /// `path` was opened, leaving the note before it with the cursor at `cursor`. Anything
    /// ahead of where the trail was is dropped, as after going back in a browser.
    void visit(const QString &path, int cursor);
    /// The note before this one that still `exists`, leaving this one with the cursor at
    /// `cursor`. Notes gone since are dropped on the way.
    std::optional<Visit> back(int cursor, const std::function<bool(const QString &)> &exists);
    /// The note after this one that still `exists`, as back() does the other way.
    std::optional<Visit> forward(int cursor, const std::function<bool(const QString &)> &exists);
    /// The note `by` places on, back when negative, counting only those behind() and ahead()
    /// list, and leaving this one with the cursor at `cursor`.
    std::optional<Visit> go(int by, int cursor, const std::function<bool(const QString &)> &exists);
    /// The notes go() can reach, nearest first: the one at index i is `-(i + 1)` or `i + 1` away.
    [[nodiscard]] QList<Visit> behind(const std::function<bool(const QString &)> &exists) const;
    [[nodiscard]] QList<Visit> ahead(const std::function<bool(const QString &)> &exists) const;
    [[nodiscard]] bool canGoBack() const { return m_at > 0; }
    [[nodiscard]] bool canGoForward() const { return m_at + 1 < m_visits.size(); }
    /// A note, or a notebook and everything in it, moved from `from` to `to`.
    void moved(const QString &from, const QString &to);
    void clear();

    [[nodiscard]] const QList<Visit> &visits() const { return m_visits; }

    /// The most kept; the oldest go first.
    static constexpr qsizetype kLimit = 100;

private:
    [[nodiscard]] QList<Visit> reachable(int direction,
                                         const std::function<bool(const QString &)> &exists) const;

    QList<Visit> m_visits;
    qsizetype m_at = -1;
};

} // namespace jot
