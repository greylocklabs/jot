#pragma once

#include <QList>
#include <QString>
#include <QStringList>

#include <optional>

namespace jot {

/// A value in a table cell, as the file holds it.
struct DataValue
{
    enum class Type {
        /// The record has no such field.
        Missing,
        String,
        Number,
        Bool,
        Null,
        /// A nested object or array, shown and edited as JSON text.
        Nested,
    };
    Type type = Type::Missing;
    /// The string itself, a number or literal as written, or nested JSON on one line.
    QString text;

    bool operator==(const DataValue &) const = default;
};

/// A data file holding a list of records, seen as a table: one row per record and one column
/// per field. The file's text stays the truth: each change rewrites only the part of the text it
/// touches, so everything else keeps its formatting, key order, quoting and line endings.
class DataTable
{
public:
    /// Reads `text` as `kind`: "json" for an array of objects, "csv" or "tsv" with a header row.
    /// Nothing, with `why`, for text that does not read or is not a list of records.
    static std::optional<DataTable> read(const QString &kind, const QString &text,
                                         QString *why = nullptr);

    [[nodiscard]] QString kind() const { return m_kind; }
    [[nodiscard]] QString text() const { return m_text; }
    /// Every field, in the order records first have them.
    [[nodiscard]] QStringList columns() const { return m_columns; }
    [[nodiscard]] int rowCount() const { return int(m_rows.size()); }
    [[nodiscard]] DataValue value(int row, int column) const;
    /// Whether the cell's type differs from the type most of its column has, as a number
    /// among strings would. Missing and null cells never do.
    [[nodiscard]] bool stray(int row, int column) const;

    /// Sets a cell, adding the field to the record if it lacks it. A string, number, boolean or
    /// null as `value.type` says; nested text must be a JSON object or array. In CSV and TSV
    /// every value is text.
    bool setValue(int row, int column, const DataValue &value, QString *error = nullptr);
    /// Takes the field out of the record, leaving the cell missing. CSV and TSV empty it.
    bool clearValue(int row, int column);
    /// Adds an empty record before `row`, or at the end for `rowCount()`.
    bool insertRow(int row);
    bool removeRow(int row);
    /// Adds a field after the last column. In JSON it appears in a record once it has a value.
    bool addColumn(const QString &name, QString *error = nullptr);
    /// Takes the field out of every record.
    bool removeColumn(int column);
    bool renameColumn(int column, const QString &name, QString *error = nullptr);

    /// A value as typed into a cell: a number, true, false or null where `like` is one,
    /// otherwise a string. Nested text stays nested.
    [[nodiscard]] static DataValue typed(const QString &text, DataValue::Type like);

private:
    struct Span
    {
        qsizetype start = 0;
        qsizetype end = 0;
    };
    /// A field of a record: in JSON a member, its key's span and its value's span; in CSV a
    /// field's span.
    struct Field
    {
        QString name;
        Span key;
        Span value;
        DataValue data;
        bool quoted = false;
    };
    struct Row
    {
        Span span;
        QList<Field> fields;
    };

    bool parse(QString *why);
    bool parseJson(QString *why);
    bool parseDelimited(QString *why);
    /// Puts `replacement` over `span` and reads the text again.
    bool patch(Span span, const QString &replacement);
    bool patches(QList<std::pair<Span, QString>> edits);
    [[nodiscard]] const Field *field(int row, int column) const;
    [[nodiscard]] QString jsonValue(const DataValue &value, int depth) const;
    [[nodiscard]] QString csvField(const QString &text, bool quoted) const;
    [[nodiscard]] QString lineIndent(qsizetype at) const;
    [[nodiscard]] QString newline() const;
    [[nodiscard]] QChar delimiter() const { return m_kind == u"tsv" ? u'\t' : u','; }

    QString m_kind;
    QString m_text;
    QStringList m_columns;
    /// Fields named in addColumn() that no record has yet.
    QStringList m_pending;
    QList<Row> m_rows;
    /// CSV and TSV: the header line's fields.
    Row m_header;
    /// JSON: the array holding the records.
    Span m_array;
    /// JSON: one level of indentation as the file writes it, empty for a file on one line.
    QString m_indent;
};

} // namespace jot
