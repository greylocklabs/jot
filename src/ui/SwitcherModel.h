#pragma once

#include "jot/VaultIndex.h"

#include <QAbstractListModel>
#include <QQmlEngine>

namespace jot {

/// The quick switcher's notes, and the notes offered while typing a wikilink: every note,
/// best fuzzy match first, by title, alias or path.
class SwitcherModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Given by Vault")
    Q_PROPERTY(QString query READ query WRITE setQuery NOTIFY queryChanged)
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    enum Role {
        PathRole = Qt::UserRole + 1,
        TitleRole,
        NotebookRole,
        /// The alias that matched, when it was an alias rather than the title.
        AliasRole,
    };

    /// The most matches shown, since nobody reads further down a switcher.
    static constexpr int kLimit = 30;

    explicit SwitcherModel(QObject *parent = nullptr);

    /// Takes the notes from `index`, or none without one.
    void reload(const VaultIndex *index);

    [[nodiscard]] QString query() const { return m_query; }
    void setQuery(const QString &query);
    Q_INVOKABLE QString pathAt(int row) const;
    /// The first `limit` matches as `{text, detail, path}` for a suggestion list.
    Q_INVOKABLE QVariantList suggestions(int limit) const;

    [[nodiscard]] int rowCount(const QModelIndex &parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

Q_SIGNALS:
    void queryChanged();
    void countChanged();

private:
    struct Row
    {
        QString path;
        QString title;
        QString alias;
    };
    void refill();

    QList<NoteInfo> m_notes;
    QString m_query;
    QList<Row> m_rows;
};

} // namespace jot
