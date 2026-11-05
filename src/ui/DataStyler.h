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

    /// The lines that start between `top` and `bottom` in the text's own layout, each as its
    /// number from 1 and where it starts.
    Q_INVOKABLE QVariantList lineTops(qreal top, qreal bottom) const;
    /// Where the bracket at or just before `position` and its partner are, outside strings;
    /// empty when there is none there or it has no partner.
    Q_INVOKABLE QVariantList brackets(int position) const;
    /// Where line `line`, from 1, starts in the text.
    Q_INVOKABLE int positionOfLine(int line) const;

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
