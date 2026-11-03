#include "jot/NoteTrail.h"

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
    return step(-1, cursor, exists);
}

std::optional<NoteTrail::Visit>
NoteTrail::forward(int cursor, const std::function<bool(const QString &)> &exists)
{
    return step(1, cursor, exists);
}

std::optional<NoteTrail::Visit> NoteTrail::step(int by, int cursor,
                                                const std::function<bool(const QString &)> &exists)
{
    if (m_at < 0)
        return std::nullopt;
    m_visits[m_at].cursor = cursor;
    // Notes gone since go, and so does this note met again, as there would be nothing to see.
    const QString here = m_visits.at(m_at).path;
    qsizetype to = m_at + by;
    while (to >= 0 && to < m_visits.size() &&
           (m_visits.at(to).path == here || !exists(m_visits.at(to).path))) {
        m_visits.removeAt(to);
        if (by < 0) {
            --m_at;
            --to;
        }
    }
    if (to < 0 || to >= m_visits.size())
        return std::nullopt;
    m_at = to;
    return m_visits.at(m_at);
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
