#include "TrashModel.h"

#include "jot/Vault.h"

#include <QLocale>

using namespace Qt::StringLiterals;

namespace jot {

int TrashModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_items.size());
}

QVariant TrashModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_items.size())
        return {};
    const TrashedNote &item = m_items.at(index.row());
    switch (role) {
    case TitleRole: return Vault::titleFromPath(item.path);
    case FolderRole: return Vault::notebookOf(item.path);
    case DeletedRole:
        return item.deleted.isValid()
                   ? QLocale().toString(item.deleted.toLocalTime(), QLocale::ShortFormat)
                   : QString();
    case LocationRole: return item.location;
    default: return {};
    }
}

QHash<int, QByteArray> TrashModel::roleNames() const
{
    return {{TitleRole, "title"},
            {FolderRole, "folder"},
            {DeletedRole, "deleted"},
            {LocationRole, "location"}};
}

void TrashModel::setItems(QList<TrashedNote> items)
{
    m_all = std::move(items);
    refill();
}

void TrashModel::setItems(QList<TrashedNote> items, const QString &query)
{
    m_all = std::move(items);
    if (m_query != query) {
        m_query = query;
        Q_EMIT queryChanged();
    }
    refill();
}

void TrashModel::setQuery(const QString &query)
{
    if (m_query == query)
        return;
    m_query = query;
    Q_EMIT queryChanged();
    refill();
}

void TrashModel::refill()
{
    beginResetModel();
    m_items = VaultTrash::matching(m_all, m_query);
    endResetModel();
    Q_EMIT countChanged();
}

std::optional<TrashedNote> TrashModel::item(const QString &location) const
{
    for (const TrashedNote &item : m_all) {
        if (item.location == location)
            return item;
    }
    return std::nullopt;
}

} // namespace jot
