#pragma once

#include "jot/Vault.h"

#include <QAbstractListModel>
#include <QHash>
#include <QQmlEngine>
#include <QSet>

#include <functional>

namespace jot {

class Settings;

/// The sidebar's notebooks: a tree shown as a list, with "All notes" first and folded
/// notebooks hiding the ones inside them. Remembers what is folded in Settings.
class NotebookModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Given by Vault")

public:
    enum Role {
        PathRole = Qt::UserRole + 1,
        NameRole,
        DepthRole,
        HasChildrenRole,
        ExpandedRole,
        CountRole,
    };

    explicit NotebookModel(Settings &settings, QObject *parent = nullptr);

    /// Reads the folders again, keeping what is folded.
    /// Lists `vault`'s notebooks, apart from those `hidden` says are kept out of view.
    void reload(const Vault *vault, const QHash<QString, int> &counts, int total,
                const std::function<bool(const QString &)> &hidden = {});

    /// Folds a notebook shut, or opens it.
    Q_INVOKABLE void toggle(const QString &path);

    [[nodiscard]] int rowCount(const QModelIndex &parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

private:
    struct Row
    {
        QString path;
        QString name;
        int depth = 0;
        bool hasChildren = false;
        int count = 0;
    };
    void rebuild();

    Settings &m_settings;
    /// Every notebook, parents first, whether shown or not.
    QList<Row> m_all;
    QList<Row> m_shown;
};

} // namespace jot
