#include "SwitcherModel.h"

#include "jot/Fuzzy.h"
#include "jot/Vault.h"

#include <algorithm>

using namespace Qt::StringLiterals;

namespace jot {

SwitcherModel::SwitcherModel(QObject *parent) : QAbstractListModel(parent) {}

void SwitcherModel::reloadLater(const VaultIndex *index)
{
    m_staleIndex = index;
    m_stale = true;
}

void SwitcherModel::reload(const VaultIndex *index)
{
    m_stale = false;
    m_notes = index ? index->shownIn({}) : QList<NoteInfo>();
    // Recently changed notes first while nothing is typed.
    std::ranges::sort(m_notes,
                      [](const NoteInfo &a, const NoteInfo &b) { return a.modified > b.modified; });
    refill();
}

void SwitcherModel::setQuery(const QString &query)
{
    // Asked again, as it is each time it opens, it takes the notes it was told of.
    if (m_stale) {
        const bool changed = m_query != query;
        m_query = query;
        reload(m_staleIndex);
        if (changed)
            Q_EMIT queryChanged();
        return;
    }
    if (m_query == query)
        return;
    m_query = query;
    Q_EMIT queryChanged();
    refill();
}

void SwitcherModel::refill()
{
    const int before = int(m_rows.size());
    beginResetModel();
    m_rows.clear();
    const QString pattern = m_query.simplified().remove(u' ');
    if (pattern.isEmpty()) {
        for (const NoteInfo &note : std::as_const(m_notes)) {
            if (m_rows.size() == kLimit)
                break;
            m_rows.append({note.path, note.title, {}, note.type});
        }
    } else {
        struct Scored
        {
            int score;
            Row row;
        };
        QList<Scored> scored;
        for (const NoteInfo &note : std::as_const(m_notes)) {
            std::optional<int> best = fuzzyScore(pattern, note.title);
            QString alias;
            for (const QString &each : note.aliases) {
                const std::optional<int> score = fuzzyScore(pattern, each);
                if (score && (!best || *score > *best)) {
                    best = score;
                    alias = each;
                }
            }
            // The path counts for less, so a title match beats a folder name.
            if (const std::optional<int> score = fuzzyScore(pattern, note.path);
                score && (!best || *score - 20 > *best)) {
                best = *score - 20;
                alias.clear();
            }
            if (best)
                scored.append({*best, {note.path, note.title, alias, note.type}});
        }
        std::ranges::stable_sort(
            scored, [](const Scored &a, const Scored &b) { return a.score > b.score; });
        for (const Scored &each : std::as_const(scored)) {
            if (m_rows.size() == kLimit)
                break;
            m_rows.append(each.row);
        }
    }
    endResetModel();
    if (before != m_rows.size())
        Q_EMIT countChanged();
}

QString SwitcherModel::pathAt(int row) const
{
    return row >= 0 && row < m_rows.size() ? m_rows.at(row).path : QString();
}

QVariantList SwitcherModel::suggestions(int limit) const
{
    QVariantList items;
    for (const Row &row : std::as_const(m_rows)) {
        if (items.size() == limit)
            break;
        const QString notebook =
            row.path.contains(u'/') ? row.path.section(u'/', 0, -2) : QString();
        items.append(QVariantMap{
            {u"text"_s, row.title},
            {u"detail"_s, row.alias.isEmpty() ? notebook : row.alias},
            {u"path"_s, row.path},
            {u"type"_s, row.type},
        });
    }
    return items;
}

int SwitcherModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_rows.size());
}

QVariant SwitcherModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_rows.size())
        return {};
    const Row &row = m_rows.at(index.row());
    switch (role) {
    case PathRole: return row.path;
    case TitleRole: return row.title;
    case NotebookRole: return row.path.contains(u'/') ? row.path.section(u'/', 0, -2) : QString();
    case AliasRole: return row.alias;
    }
    return {};
}

QHash<int, QByteArray> SwitcherModel::roleNames() const
{
    return {
        {PathRole, "path"}, {TitleRole, "title"}, {NotebookRole, "notebook"}, {AliasRole, "alias"}};
}

} // namespace jot
