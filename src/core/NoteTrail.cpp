#include "jot/NoteTrail.h"

#include <cstdlib>

namespace jot {

void NoteTrail::visit(const QString &path, int cursor)
{
    if (path.isEmpty() || (m_at >= 0 && m_visits.at(m_at).path == path))
        return;
    if (m_at >= 0)
        m_visits[m_at].cursor = cursor;
    m_visits.resize(m_at + 1);
    m_visits.append({path, 0});
    if (m_visits.size() > kLimit)
        m_visits.removeFirst();
    m_at = m_visits.size() - 1;
}

std::optional<NoteTrail::Visit> NoteTrail::back(int cursor,
                                                const std::function<bool(const QString &)> &exists)
{
    return go(-1, cursor, exists);
}

std::optional<NoteTrail::Visit>
NoteTrail::forward(int cursor, const std::function<bool(const QString &)> &exists)
{
    return go(1, cursor, exists);
}

std::optional<NoteTrail::Visit> NoteTrail::go(int by, int cursor,
                                              const std::function<bool(const QString &)> &exists)
{
    if (m_at < 0 || by == 0)
        return std::nullopt;
    m_visits[m_at].cursor = cursor;
    // Notes gone since go, and so does this note met again, as there would be nothing to see.
    const QString here = m_visits.at(m_at).path;
    const int direction = by < 0 ? -1 : 1;
    qsizetype to = m_at;
    for (int left = std::abs(by); left > 0; --left) {
        to += direction;
        while (to >= 0 && to < m_visits.size() &&
               (m_visits.at(to).path == here || !exists(m_visits.at(to).path))) {
            m_visits.removeAt(to);
            if (direction < 0) {
                --m_at;
                --to;
            }
        }
        if (to < 0 || to >= m_visits.size())
            return std::nullopt;
    }
    m_at = to;
    return m_visits.at(m_at);
}

QList<NoteTrail::Visit> NoteTrail::behind(const std::function<bool(const QString &)> &exists) const
{
    return reachable(-1, exists);
}

QList<NoteTrail::Visit> NoteTrail::ahead(const std::function<bool(const QString &)> &exists) const
{
    return reachable(1, exists);
}

QList<NoteTrail::Visit>
NoteTrail::reachable(int direction, const std::function<bool(const QString &)> &exists) const
{
    QList<Visit> found;
    if (m_at < 0)
        return found;
    const QString here = m_visits.at(m_at).path;
    for (qsizetype i = m_at + direction; i >= 0 && i < m_visits.size(); i += direction) {
        if (m_visits.at(i).path != here && exists(m_visits.at(i).path))
            found.append(m_visits.at(i));
    }
    return found;
}

void NoteTrail::moved(const QString &from, const QString &to)
{
    for (Visit &each : m_visits) {
        if (each.path == from)
            each.path = to;
        else if (each.path.startsWith(from + u'/'))
            each.path = to + each.path.mid(from.size());
    }
}

void NoteTrail::clear()
{
    m_visits.clear();
    m_at = -1;
}

} // namespace jot
