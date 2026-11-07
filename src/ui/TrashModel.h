#pragma once

#include "jot/VaultTrash.h"

#include <QAbstractListModel>
#include <QQmlEngine>

#include <optional>

namespace jot {

/// What Trash lists: the vault's notes and data files in either trash, newest first.
class TrashModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Given by Vault")
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    enum Role {
        TitleRole = Qt::UserRole + 1,
        /// The notebook it was in; empty for the top of the vault.
        FolderRole,
        /// When it was deleted, as the list says it.
        DeletedRole,
        /// Where it is in the trash, which names it to Vault.
        LocationRole,
    };
    Q_ENUM(Role)

    using QAbstractListModel::QAbstractListModel;

    [[nodiscard]] int rowCount(const QModelIndex &parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    void setItems(QList<TrashedNote> items);
    /// The item at `location`, if it is still listed.
    [[nodiscard]] std::optional<TrashedNote> item(const QString &location) const;
    /// Whether the item at `location` is still listed.
    Q_INVOKABLE bool contains(const QString &location) const { return item(location).has_value(); }

Q_SIGNALS:
    void countChanged();

private:
    QList<TrashedNote> m_items;
};

} // namespace jot
