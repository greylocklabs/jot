#pragma once

#include "jot/VaultIndex.h"

#include <QAbstractListModel>
#include <QQmlEngine>

namespace jot {

class Settings;

/// The sidebar's tags: nested tags as a tree shown as a list, folded ones hiding the tags inside
/// them, as the notebooks are. A tag that others are nested in shows even when no note has it
/// itself. Remembers what is folded in Settings.
class TagModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Given by Vault")
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    enum Role {
        TagRole = Qt::UserRole + 1,
        /// The last part of a nested tag: "ch1" for "novel/ch1".
        NameRole,
        DepthRole,
        HasChildrenRole,
        ExpandedRole,
        CountRole,
    };

    explicit TagModel(Settings &settings, QObject *parent = nullptr);

    /// Shows these tags, keeping what is folded.
    void reload(const QList<TagCount> &tags);

    /// Folds a tag shut, or opens it.
    Q_INVOKABLE void toggle(const QString &tag);

    [[nodiscard]] int rowCount(const QModelIndex &parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

Q_SIGNALS:
    void countChanged();

private:
    void rebuild();
    [[nodiscard]] bool folded(const QString &tag) const;

    Settings &m_settings;
    QList<TagCount> m_all;
    QList<TagCount> m_shown;
};

} // namespace jot
