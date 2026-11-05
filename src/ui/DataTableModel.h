#pragma once

#include "jot/DataTable.h"
#include "jot/JsonSchema.h"

#include <QAbstractTableModel>
#include <QJsonValue>
#include <QStringList>
#include <QtQml/qqmlregistration.h>

#include <functional>
#include <optional>

namespace jot {

/// A data file's records as a table to edit in place. Sorting and filtering change only what
/// is shown; every edit rewrites the file's text and hands it on through edited().
class DataTableModel : public QAbstractTableModel
{
    Q_OBJECT
    QML_ELEMENT

    /// "json", "yaml", "csv" or "tsv".
    Q_PROPERTY(QString kind READ kind WRITE setKind NOTIFY loaded)
    /// The file's text. Setting it reads the table again, as when the file changed elsewhere.
    Q_PROPERTY(QString text READ text WRITE setText NOTIFY loaded)
    /// Whether the text reads as a list of records.
    Q_PROPERTY(bool table READ isTable NOTIFY loaded)
    /// Why it does not, when it does not.
    Q_PROPERTY(QString why READ why NOTIFY loaded)
    Q_PROPERTY(QStringList columns READ columns NOTIFY loaded)
    /// The column rows are sorted by, or -1 for the file's own order.
    Q_PROPERTY(int sortColumn READ sortColumn NOTIFY arranged)
    Q_PROPERTY(bool sortDescending READ sortDescending NOTIFY arranged)
    /// Words a row must hold somewhere to be shown.
    Q_PROPERTY(QString filter READ filter WRITE setFilter NOTIFY arranged)
    /// The file's own path, to find the schema beside it.
    Q_PROPERTY(QString path READ path WRITE setPath NOTIFY schemaChanged)
    /// The schema file the records are checked against, or empty.
    Q_PROPERTY(QString schemaFile READ schemaFile NOTIFY schemaChanged)
    /// What breaks the schema, as "where: why".
    Q_PROPERTY(QStringList schemaProblems READ schemaProblems NOTIFY schemaChanged)
    /// One line saying how the records stand against the schema, or why the schema could not be
    /// read; empty when nothing breaks it.
    Q_PROPERTY(QString schemaSummary READ schemaSummary NOTIFY schemaChanged)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY historyChanged)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY historyChanged)

public:
    enum Role {
        /// "string", "number", "bool", "null", "nested" or "missing".
        TypeRole = Qt::UserRole + 1,
        /// Its type differs from the rest of its column.
        StrayRole,
        /// The record's place in the file.
        RecordRole,
        /// Why the value breaks the schema, or empty.
        ProblemRole,
    };
    Q_ENUM(Role)

    explicit DataTableModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    int columnCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    [[nodiscard]] QString kind() const { return m_kind; }
    void setKind(const QString &kind);
    [[nodiscard]] QString text() const;
    void setText(const QString &text);
    [[nodiscard]] bool isTable() const { return m_table.has_value(); }
    [[nodiscard]] QString why() const { return m_why; }
    [[nodiscard]] QStringList columns() const;
    [[nodiscard]] int sortColumn() const { return m_sortColumn; }
    [[nodiscard]] bool sortDescending() const { return m_descending; }
    [[nodiscard]] QString filter() const { return m_filter; }
    void setFilter(const QString &filter);
    [[nodiscard]] QString path() const { return m_path; }
    void setPath(const QString &path);
    [[nodiscard]] QString schemaFile() const { return m_schemaFile; }
    [[nodiscard]] QStringList schemaProblems() const { return m_schemaProblems; }
    [[nodiscard]] QString schemaSummary() const;
    /// The values the schema allows in `column`, when it lists them.
    Q_INVOKABLE QStringList choices(int column) const;
    [[nodiscard]] bool canUndo() const { return !m_undo.isEmpty(); }
    [[nodiscard]] bool canRedo() const { return !m_redo.isEmpty(); }

    /// Sets the cell shown at `row`, `column` to what was typed, keeping its type where the
    /// text still reads as one (a number stays a number). Nested text must be JSON.
    Q_INVOKABLE bool setCell(int row, int column, const QString &typed);
    /// Sets the cell as a string whatever it looks like, or as null.
    Q_INVOKABLE bool setCellAs(int row, int column, const QString &typed, const QString &type);
    Q_INVOKABLE bool clearCell(int row, int column);
    /// Adds an empty record after the row shown at `row`, or first for -1. Gives the row it
    /// shows in, or -1 when none was added.
    Q_INVOKABLE int addRowAfter(int row);
    /// A nested value as JSON over lines, as it is written, to edit.
    Q_INVOKABLE QString nestedText(int row, int column) const;
    Q_INVOKABLE bool removeRow(int row);
    Q_INVOKABLE bool addColumn(const QString &name);
    Q_INVOKABLE bool removeColumn(int column);
    Q_INVOKABLE bool renameColumn(int column, const QString &name);
    /// Sorts by `column`, ascending first, then descending, then back to the file's order.
    Q_INVOKABLE void sortBy(int column);
    Q_INVOKABLE void undo();
    Q_INVOKABLE void redo();
    /// What went wrong with the last edit that did not take.
    Q_INVOKABLE QString lastError() const { return m_error; }

Q_SIGNALS:
    void loaded();
    void arranged();
    void historyChanged();
    /// The file's new text after an edit here.
    void edited(const QString &text);
    void schemaChanged();

private:
    /// Runs `change` on the table and, when it took, keeps the text before it for undo.
    bool change(const std::function<bool(DataTable &)> &change);
    void reload(const QString &text);
    void arrange();
    /// Checks the records against the schema again.
    void checkSchema();
    /// The schema's rules for a record's field `name`.
    [[nodiscard]] QJsonValue fieldRules(const QString &name) const;
    [[nodiscard]] int record(int row) const;

    QString m_kind;
    QString m_text;
    std::optional<DataTable> m_table;
    QString m_why;
    QString m_error;
    /// Records in the order shown.
    QList<int> m_order;
    int m_sortColumn = -1;
    bool m_descending = false;
    QString m_filter;
    QString m_path;
    QString m_schemaFile;
    QJsonValue m_schema;
    QStringList m_schemaProblems;
    /// Why the schema file could not be read.
    QString m_schemaError;
    /// Problems by record and field.
    QHash<std::pair<int, QString>, QString> m_cellProblems;
    QStringList m_undo;
    QStringList m_redo;
};

} // namespace jot
