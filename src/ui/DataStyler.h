#pragma once

#include "jot/DataCheck.h"

#include <QObject>
#include <QPointer>
#include <QQuickTextDocument>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

namespace jot {

class DataHighlighter;

/// Colors a data file's text in the editor by its kind, says where it stops reading as that
/// kind, and finds what the editor draws around it: line numbers and matching brackets.
class DataStyler : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QQuickTextDocument *document READ document WRITE setDocument NOTIFY documentChanged)
    /// "json", "yaml", "csv" or "tsv".
    Q_PROPERTY(QString kind READ kind WRITE setKind NOTIFY kindChanged)
    /// Where the text stops reading: line, column and message; empty while it reads.
    Q_PROPERTY(QVariantMap problem READ problem NOTIFY problemChanged)

public:
    explicit DataStyler(QObject *parent = nullptr);
    ~DataStyler() override;

    [[nodiscard]] QQuickTextDocument *document() const { return m_document; }
    void setDocument(QQuickTextDocument *document);
    [[nodiscard]] QString kind() const { return m_kind; }
    void setKind(const QString &kind);
    [[nodiscard]] QVariantMap problem() const;

    /// The lines of `document` that start between `top` and `bottom` in its layout, each as its
    /// number from 1, where it starts, and the height of its first line.
    Q_INVOKABLE static QVariantList lineTops(QQuickTextDocument *document, qreal top, qreal bottom);
    /// Where the bracket at or just before `position` and its partner are, outside strings;
    /// empty when there is none there or it has no partner.
    Q_INVOKABLE QVariantList brackets(int position) const;
    /// Where the problem is in the text, to put the cursor on it.
    Q_INVOKABLE int problemPosition() const { return int(m_problem.position); }

    /// Checks the text now rather than after the pause that follows typing.
    Q_INVOKABLE void checkNow();

Q_SIGNALS:
    void documentChanged();
    void kindChanged();
    void problemChanged();

private:
    void restyle();

    QPointer<QQuickTextDocument> m_document;
    QString m_kind;
    /// Owned by the document it colors, which may go first.
    QPointer<DataHighlighter> m_highlighter;
    DataProblem m_problem;
    QTimer m_check;
};

} // namespace jot
