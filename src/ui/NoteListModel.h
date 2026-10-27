#pragma once

#include "jot/VaultIndex.h"

#include <QAbstractListModel>
#include <QDateTime>
#include <QQmlEngine>

namespace jot {

/// The notes in the middle column: a notebook's, a tag's, or what a search found in
/// either, ordered as the user chose. Searching ranks by relevance instead.
class NoteListModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Given by Vault")
    /// The notebook shown, by vault path; empty for every note.
    Q_PROPERTY(QString notebook READ notebook WRITE setNotebook NOTIFY scopeChanged)
    /// The tag shown, instead of a notebook; empty for none.
    Q_PROPERTY(QString tag READ tag WRITE setTag NOTIFY scopeChanged)
    Q_PROPERTY(QString query READ query WRITE setQuery NOTIFY queryChanged)
    /// "updated", "created" or "title".
    Q_PROPERTY(QString order READ order WRITE setOrder NOTIFY orderChanged)
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    enum Role {
        PathRole = Qt::UserRole + 1,
        TitleRole,
        NotebookRole,
        ModifiedRole,
        SnippetRole,
        PinnedRole,
    };

    explicit NoteListModel(QObject *parent = nullptr);

    /// Reads the notes again from `index`, or empties the list without one. Those in
    /// `pinned` come first.
    void reload(const VaultIndex *index, const QStringList &pinned = {});

    [[nodiscard]] QString notebook() const { return m_notebook; }
    void setNotebook(const QString &notebook);
    [[nodiscard]] QString tag() const { return m_tag; }
    void setTag(const QString &tag);
    [[nodiscard]] QString query() const { return m_query; }
    void setQuery(const QString &query);
    [[nodiscard]] QString order() const { return m_order; }
    void setOrder(const QString &order);

    /// The row of the note at `path`, or -1.
    Q_INVOKABLE int rowOf(const QString &path) const;
    /// The note at `row`, or empty.
    Q_INVOKABLE QString pathAt(int row) const;

    [[nodiscard]] int rowCount(const QModelIndex &parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

Q_SIGNALS:
    void scopeChanged();
    void queryChanged();
    void orderChanged();
    void countChanged();

private:
    struct Row
    {
        QString path;
        QString title;
        qint64 modified = 0;
        QString snippet;
        bool pinned = false;
    };
    void refill();

    const VaultIndex *m_index = nullptr;
    QString m_notebook;
    QString m_tag;
    QString m_query;
    QStringList m_pinned;
    QString m_order = QStringLiteral("updated");
    QList<Row> m_rows;
};

} // namespace jot
