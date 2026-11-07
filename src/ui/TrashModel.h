#pragma once

#include "jot/VaultTrash.h"

#include <QAbstractListModel>
#include <QQmlEngine>

#include <optional>

namespace jot {

/// What Trash lists: the notes and data files in the vault's trash, newest first, narrowed to
/// those that hold `query` when it is set.
class TrashModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Given by Vault")
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    /// How many are in the trash, listed or not.
    Q_PROPERTY(int total READ total NOTIFY countChanged)
    /// Words every note listed holds, in its title, folder or text; empty lists them all.
    Q_PROPERTY(QString query READ query WRITE setQuery NOTIFY queryChanged)

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
    /// Both at once, so the trash is searched once.
    void setItems(QList<TrashedNote> items, const QString &query);
    [[nodiscard]] int total() const { return int(m_all.size()); }
    [[nodiscard]] QString query() const { return m_query; }
    void setQuery(const QString &query);
    /// The item at `location`, if it is in the trash, listed or not.
    [[nodiscard]] std::optional<TrashedNote> item(const QString &location) const;
    /// Whether the item at `location` is still in the trash.
    Q_INVOKABLE bool contains(const QString &location) const { return item(location).has_value(); }

Q_SIGNALS:
    void countChanged();
    void queryChanged();

private:
    void refill();

    /// Everything in the trash, and what of it is listed.
    QList<TrashedNote> m_all;
    QList<TrashedNote> m_items;
    QString m_query;
};

} // namespace jot
