#include "TagModel.h"

#include "jot/Settings.h"

using namespace Qt::StringLiterals;

namespace jot {

TagModel::TagModel(Settings &settings, QObject *parent)
    : QAbstractListModel(parent), m_settings(settings)
{}

void TagModel::reload(const QList<TagCount> &tags)
{
    m_all = tags;
    rebuild();
}

bool TagModel::folded(const QString &tag) const
{
    return m_settings.collapsedTags().contains(tag, Qt::CaseInsensitive);
}

void TagModel::rebuild()
{
    const int before = int(m_shown.size());
    beginResetModel();
    m_shown.clear();
    for (const TagCount &each : std::as_const(m_all)) {
        // Hidden when any tag above it is folded.
        bool hidden = false;
        for (qsizetype slash = each.tag.indexOf(u'/'); slash > 0 && !hidden;
             slash = each.tag.indexOf(u'/', slash + 1))
            hidden = folded(each.tag.first(slash));
        if (!hidden)
            m_shown.append(each);
    }
    endResetModel();
    if (before != m_shown.size())
        Q_EMIT countChanged();
}

void TagModel::toggle(const QString &tag)
{
    // Tags match whatever their case, and the spelling shown can change as notes do.
    QStringList collapsed = m_settings.collapsedTags();
    if (collapsed.removeIf([&tag](const QString &folded) {
            return folded.compare(tag, Qt::CaseInsensitive) == 0;
        }) == 0)
        collapsed.append(tag);
    collapsed.sort(Qt::CaseInsensitive);
    m_settings.setCollapsedTags(collapsed);
    rebuild();
}

int TagModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_shown.size());
}

QVariant TagModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_shown.size())
        return {};
    const TagCount &row = m_shown.at(index.row());
    switch (role) {
    case TagRole: return row.tag;
    case NameRole: return row.tag.section(u'/', -1);
    case DepthRole: return int(row.tag.count(u'/'));
    case HasChildrenRole: {
        const qsizetype next = m_all.indexOf(row) + 1;
        return next > 0 && next < m_all.size() &&
               m_all.at(next).tag.startsWith(row.tag + u'/', Qt::CaseInsensitive);
    }
    case ExpandedRole: return !folded(row.tag);
    case CountRole: return row.notes;
    }
    return {};
}

QHash<int, QByteArray> TagModel::roleNames() const
{
    return {{TagRole, "tag"},           {NameRole, "name"},
            {DepthRole, "depth"},       {HasChildrenRole, "hasChildren"},
            {ExpandedRole, "expanded"}, {CountRole, "count"}};
}

} // namespace jot
