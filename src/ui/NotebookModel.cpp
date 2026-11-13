#include "NotebookModel.h"

#include "jot/Settings.h"

using namespace Qt::StringLiterals;

namespace jot {

NotebookModel::NotebookModel(Settings &settings, QObject *parent)
    : QAbstractListModel(parent), m_settings(settings)
{}

void NotebookModel::reload(const Vault *vault, const QHash<QString, int> &counts, int total,
                           const std::function<bool(const QString &)> &hidden)
{
    m_all.clear();
    if (vault) {
        m_all.append({QString(), tr("All notes"), 0, false, total});
        QList<VaultEntry> entries = vault->entries();
        if (hidden)
            entries.removeIf([&hidden](const VaultEntry &entry) { return hidden(entry.path); });
        for (qsizetype i = 0; i < entries.size(); ++i) {
            const VaultEntry &entry = entries.at(i);
            if (!entry.folder)
                continue;
            const bool hasChildren = std::any_of(
                entries.cbegin() + i + 1, entries.cend(), [&entry](const VaultEntry &other) {
                    return other.folder && other.path.startsWith(entry.path + u'/');
                });
            m_all.append({entry.path, entry.path.section(u'/', -1), int(entry.path.count(u'/')),
                          hasChildren, counts.value(entry.path)});
        }
    }
    rebuild();
}

void NotebookModel::rebuild()
{
    const QStringList collapsed = m_settings.collapsedNotebooks();
    beginResetModel();
    m_shown.clear();
    for (const Row &row : std::as_const(m_all)) {
        // Hidden when any folder above it is folded.
        const bool hidden =
            std::any_of(collapsed.cbegin(), collapsed.cend(), [&row](const QString &folded) {
                return row.path.startsWith(folded + u'/');
            });
        if (!hidden)
            m_shown.append(row);
    }
    endResetModel();
}

void NotebookModel::toggle(const QString &path)
{
    QStringList collapsed = m_settings.collapsedNotebooks();
    if (!collapsed.removeOne(path))
        collapsed.append(path);
    collapsed.sort();
    m_settings.setCollapsedNotebooks(collapsed);
    rebuild();
}

int NotebookModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_shown.size());
}

QVariant NotebookModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_shown.size())
        return {};
    const Row &row = m_shown.at(index.row());
    switch (role) {
    case PathRole: return row.path;
    case NameRole: return row.name;
    case DepthRole: return row.depth;
    case HasChildrenRole: return row.hasChildren;
    case ExpandedRole: return !m_settings.collapsedNotebooks().contains(row.path);
    case CountRole: return row.count;
    }
    return {};
}

QHash<int, QByteArray> NotebookModel::roleNames() const
{
    return {{PathRole, "path"},         {NameRole, "name"},
            {DepthRole, "depth"},       {HasChildrenRole, "hasChildren"},
            {ExpandedRole, "expanded"}, {CountRole, "count"}};
}

} // namespace jot
