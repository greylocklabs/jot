#include "DataTableModel.h"

#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>

#include <algorithm>

using namespace Qt::StringLiterals;

namespace jot {

namespace {

QString typeName(DataValue::Type type)
{
    switch (type) {
    case DataValue::Type::String: return u"string"_s;
    case DataValue::Type::Number: return u"number"_s;
    case DataValue::Type::Bool: return u"bool"_s;
    case DataValue::Type::Null: return u"null"_s;
    case DataValue::Type::Nested: return u"nested"_s;
    case DataValue::Type::Missing: return u"missing"_s;
    }
    return {};
}

std::optional<DataValue::Type> typeNamed(const QString &name)
{
    for (const DataValue::Type type :
         {DataValue::Type::String, DataValue::Type::Number, DataValue::Type::Bool,
          DataValue::Type::Null, DataValue::Type::Nested}) {
        if (typeName(type) == name)
            return type;
    }
    return std::nullopt;
}

} // namespace

DataTableModel::DataTableModel(QObject *parent) : QAbstractTableModel(parent) {}

int DataTableModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_order.size());
}

int DataTableModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() || !m_table ? 0 : int(m_table->columns().size());
}

QStringList DataTableModel::columns() const
{
    return m_table ? m_table->columns() : QStringList();
}

QString DataTableModel::text() const
{
    return m_table ? m_table->text() : m_text;
}

int DataTableModel::record(int row) const
{
    return row >= 0 && row < m_order.size() ? m_order.at(row) : -1;
}

QVariant DataTableModel::data(const QModelIndex &index, int role) const
{
    if (!m_table || !index.isValid())
        return {};
    const int at = record(index.row());
    if (at < 0)
        return {};
    switch (role) {
    case Qt::DisplayRole: return m_table->value(at, index.column()).text;
    case TypeRole: return typeName(m_table->value(at, index.column()).type);
    case StrayRole: return m_table->stray(at, index.column());
    case RecordRole: return at;
    case ProblemRole: return m_cellProblems.value({at, m_table->columns().value(index.column())});
    default: return {};
    }
}

QVariant DataTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole || !m_table)
        return {};
    if (orientation == Qt::Horizontal)
        return section < m_table->columns().size() ? m_table->columns().at(section) : QString();
    return record(section) + 1;
}

QHash<int, QByteArray> DataTableModel::roleNames() const
{
    QHash<int, QByteArray> names = QAbstractTableModel::roleNames();
    names.insert(TypeRole, "type");
    names.insert(StrayRole, "stray");
    names.insert(RecordRole, "record");
    names.insert(ProblemRole, "problem");
    return names;
}

void DataTableModel::setKind(const QString &kind)
{
    if (m_kind == kind)
        return;
    m_kind = kind;
    m_undo.clear();
    m_redo.clear();
    reload(m_text);
    Q_EMIT historyChanged();
}

void DataTableModel::setText(const QString &text)
{
    // An edit made here comes back from the editor as the same text.
    if (m_table && text == m_table->text())
        return;
    m_undo.clear();
    m_redo.clear();
    reload(text);
    Q_EMIT historyChanged();
}

void DataTableModel::reload(const QString &text)
{
    beginResetModel();
    m_text = text;
    m_why.clear();
    m_table = DataTable::read(m_kind, text, &m_why);
    arrange();
    checkSchema();
    endResetModel();
    Q_EMIT loaded();
}

void DataTableModel::setFilter(const QString &filter)
{
    if (m_filter == filter)
        return;
    m_filter = filter;
    beginResetModel();
    arrange();
    endResetModel();
    Q_EMIT arranged();
}

void DataTableModel::sortBy(int column)
{
    if (column != m_sortColumn) {
        m_sortColumn = column;
        m_descending = false;
    } else if (!m_descending) {
        m_descending = true;
    } else {
        m_sortColumn = -1;
        m_descending = false;
    }
    beginResetModel();
    arrange();
    endResetModel();
    Q_EMIT arranged();
}

void DataTableModel::arrange()
{
    m_order.clear();
    if (!m_table)
        return;
    const QStringList words = m_filter.split(u' ', Qt::SkipEmptyParts);
    const int columns = int(m_table->columns().size());
    for (int at = 0; at < m_table->rowCount(); ++at) {
        const bool shown = std::ranges::all_of(words, [&](const QString &word) {
            for (int c = 0; c < columns; ++c) {
                if (m_table->value(at, c).text.contains(word, Qt::CaseInsensitive))
                    return true;
            }
            return false;
        });
        if (shown)
            m_order.append(at);
    }
    if (m_sortColumn < 0 || m_sortColumn >= columns)
        return;
    // Numbers by their size, the rest by their text; missing cells last either way.
    std::ranges::stable_sort(m_order, [this](int a, int b) {
        const DataValue x = m_table->value(a, m_sortColumn);
        const DataValue y = m_table->value(b, m_sortColumn);
        const bool xMissing = x.type == DataValue::Type::Missing || x.text.isEmpty();
        const bool yMissing = y.type == DataValue::Type::Missing || y.text.isEmpty();
        if (xMissing != yMissing)
            return yMissing;
        if (xMissing)
            return false;
        bool xNumber = false;
        bool yNumber = false;
        const double xValue = x.text.toDouble(&xNumber);
        const double yValue = y.text.toDouble(&yNumber);
        int order = 0;
        if (xNumber && yNumber)
            order = xValue < yValue ? -1 : xValue > yValue ? 1 : 0;
        else
            order = QString::localeAwareCompare(x.text, y.text);
        return m_descending ? order > 0 : order < 0;
    });
}

bool DataTableModel::change(const std::function<bool(DataTable &)> &edit)
{
    if (!m_table)
        return false;
    m_error.clear();
    DataTable next = *m_table;
    if (!edit(next))
        return false;
    if (next.text() == m_table->text() && next.columns() == m_table->columns())
        return true;
    const QString before = m_table->text();
    const bool sameShape =
        next.rowCount() == m_table->rowCount() && next.columns() == m_table->columns();
    if (sameShape) {
        // The rows stay where they are, so the cell being edited does not jump away.
        m_table = std::move(next);
        checkSchema();
        if (!m_order.isEmpty() && columnCount() > 0)
            Q_EMIT dataChanged(index(0, 0), index(rowCount() - 1, columnCount() - 1));
    } else {
        beginResetModel();
        m_table = std::move(next);
        arrange();
        checkSchema();
        endResetModel();
        Q_EMIT loaded();
    }
    m_text = m_table->text();
    if (m_text != before) {
        m_undo.append(before);
        m_redo.clear();
        Q_EMIT historyChanged();
        Q_EMIT edited(m_text);
    }
    return true;
}

bool DataTableModel::setCell(int row, int column, const QString &typed)
{
    const int at = record(row);
    if (at < 0)
        return false;
    DataValue::Type like = m_table->value(at, column).type;
    if (like == DataValue::Type::Missing || like == DataValue::Type::Null) {
        // A new value takes the type most of its column has.
        QHash<DataValue::Type, int> counts;
        for (int r = 0; r < m_table->rowCount(); ++r) {
            const DataValue::Type each = m_table->value(r, column).type;
            if (each != DataValue::Type::Missing && each != DataValue::Type::Null)
                ++counts[each];
        }
        DataValue::Type most = DataValue::Type::String;
        for (auto it = counts.cbegin(); it != counts.cend(); ++it) {
            if (it.value() > counts.value(most))
                most = it.key();
        }
        if (like == DataValue::Type::Null && typed.trimmed() == u"null")
            most = DataValue::Type::Null;
        like = most;
    }
    // CSV and TSV hold only text; JSON and YAML keep their types.
    if (m_kind == u"csv" || m_kind == u"tsv")
        like = DataValue::Type::String;
    const DataValue value = DataTable::typed(typed, like);
    return change([&](DataTable &table) { return table.setValue(at, column, value, &m_error); });
}

bool DataTableModel::setCellAs(int row, int column, const QString &typed, const QString &type)
{
    const int at = record(row);
    const std::optional<DataValue::Type> wanted = typeNamed(type);
    if (at < 0 || !wanted)
        return false;
    DataValue value = DataTable::typed(typed, *wanted);
    if (*wanted == DataValue::Type::String)
        value = {DataValue::Type::String, typed};
    if (value.type != *wanted) {
        m_error = tr("%1 is not a %2").arg(typed, type);
        return false;
    }
    return change([&](DataTable &table) { return table.setValue(at, column, value, &m_error); });
}

bool DataTableModel::clearCell(int row, int column)
{
    const int at = record(row);
    return at >= 0 && change([&](DataTable &table) { return table.clearValue(at, column); });
}

int DataTableModel::addRowAfter(int row)
{
    const int at = row < 0 ? 0 : record(row) + 1;
    if (at < 0 || !m_table)
        return -1;
    const int before = m_table->rowCount();
    // A new record is empty, so a filter would hide it.
    if (!m_filter.isEmpty()) {
        m_filter.clear();
        Q_EMIT arranged();
    }
    if (!change([at](DataTable &table) { return table.insertRow(at); }) ||
        m_table->rowCount() != before + 1)
        return -1;
    // Where it shows, which a sort may have put anywhere.
    return int(m_order.indexOf(at));
}

QString DataTableModel::nestedText(int row, int column) const
{
    const int at = record(row);
    return at < 0 ? QString() : m_table->nestedText(at, column);
}

bool DataTableModel::removeRow(int row)
{
    const int at = record(row);
    return at >= 0 && change([at](DataTable &table) { return table.removeRow(at); });
}

bool DataTableModel::addColumn(const QString &name)
{
    return change([&](DataTable &table) { return table.addColumn(name.trimmed(), &m_error); });
}

bool DataTableModel::removeColumn(int column)
{
    if (column == m_sortColumn) {
        m_sortColumn = -1;
        Q_EMIT arranged();
    }
    return change([column](DataTable &table) { return table.removeColumn(column); });
}

bool DataTableModel::renameColumn(int column, const QString &name)
{
    const bool renamed = change(
        [&](DataTable &table) { return table.renameColumn(column, name.trimmed(), &m_error); });
    if (renamed)
        Q_EMIT headerDataChanged(Qt::Horizontal, column, column);
    return renamed;
}

void DataTableModel::setPath(const QString &path)
{
    if (m_path == path)
        return;
    m_path = path;
    beginResetModel();
    checkSchema();
    endResetModel();
}

void DataTableModel::checkSchema()
{
    const QString before = m_schemaFile;
    const QStringList problemsBefore = m_schemaProblems;
    const QString errorBefore = m_schemaError;
    m_schemaFile.clear();
    m_schemaError.clear();
    m_schema = QJsonValue();
    m_schemaProblems.clear();
    m_cellProblems.clear();
    const std::optional<QJsonValue> data =
        m_table && !m_path.isEmpty() ? dataAsJson(m_kind, m_table->text()) : std::nullopt;
    if (data)
        m_schemaFile = schemaPathFor(m_path, *data);
    QFile file(m_schemaFile);
    const QString name = QFileInfo(m_schemaFile).fileName();
    if (!m_schemaFile.isEmpty() && !file.open(QIODevice::ReadOnly)) {
        m_schemaError = tr("%1 could not be read: %2").arg(name, file.errorString());
    } else if (!m_schemaFile.isEmpty()) {
        QJsonParseError error;
        const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &error);
        if (error.error != QJsonParseError::NoError)
            m_schemaError = tr("%1 is not JSON: %2").arg(name, error.errorString());
        else
            m_schema =
                document.isObject() ? QJsonValue(document.object()) : QJsonValue(document.array());
    }
    if (data && !m_schema.isUndefined()) {
        for (const SchemaProblem &problem : jot::checkSchema(m_schema, *data)) {
            bool isRow = false;
            const int record = problem.path.value(0).toInt(&isRow);
            if (isRow && problem.path.size() >= 2) {
                const QString field = problem.path.at(1);
                const QString where = problem.path.mid(2).join(u'.');
                const QString message =
                    where.isEmpty() ? problem.message : where + u": "_s + problem.message;
                QString &cell = m_cellProblems[{record, field}];
                cell = cell.isEmpty() ? message : cell + u"; "_s + message;
                m_schemaProblems.append(tr("Row %1, %2: %3").arg(record + 1).arg(field, message));
            } else if (isRow) {
                m_schemaProblems.append(tr("Row %1: %2").arg(record + 1).arg(problem.message));
            } else {
                m_schemaProblems.append(problem.path.isEmpty()
                                            ? problem.message
                                            : problem.path.join(u'.') + u": "_s + problem.message);
            }
        }
    }
    if (before != m_schemaFile || problemsBefore != m_schemaProblems ||
        errorBefore != m_schemaError)
        Q_EMIT schemaChanged();
}

QString DataTableModel::schemaSummary() const
{
    if (!m_schemaError.isEmpty())
        return m_schemaError;
    if (m_schemaProblems.isEmpty())
        return {};
    return tr("%n value(s) break %1: %2", "", int(m_schemaProblems.size()))
        .arg(QFileInfo(m_schemaFile).fileName(), m_schemaProblems.first());
}

QJsonValue DataTableModel::fieldRules(const QString &name) const
{
    const auto resolved = [this](QJsonValue value) {
        for (int depth = 0; depth < 16 && value.isObject() &&
                            value.toObject().value(u"$ref"_s).toString().startsWith(u'#');
             ++depth) {
            QJsonValue at = m_schema;
            const QString ref = value.toObject().value(u"$ref"_s).toString();
            for (QString part : ref.mid(1).split(u'/', Qt::SkipEmptyParts)) {
                part = QUrl::fromPercentEncoding(part.toUtf8());
                part.replace(u"~1"_s, u"/"_s).replace(u"~0"_s, u"~"_s);
                at = at.toObject().value(part);
            }
            value = at;
        }
        return value;
    };
    const QJsonValue items = resolved(resolved(m_schema).toObject().value(u"items"_s));
    return resolved(items.toObject().value(u"properties"_s).toObject().value(name));
}

QStringList DataTableModel::choices(int column) const
{
    QStringList values;
    if (!m_table || column < 0 || column >= m_table->columns().size())
        return values;
    const QJsonObject rules = fieldRules(m_table->columns().at(column)).toObject();
    for (const QJsonValue &each : rules.value(u"enum"_s).toArray()) {
        values.append(
            each.isString()
                ? each.toString()
                : QString::fromUtf8(QJsonDocument(QJsonArray{each}).toJson(QJsonDocument::Compact))
                      .mid(1)
                      .chopped(1));
    }
    if (values.isEmpty() && rules.value(u"type"_s).toString() == u"boolean")
        values = {u"true"_s, u"false"_s};
    return values;
}

void DataTableModel::undo()
{
    if (m_undo.isEmpty())
        return;
    m_redo.append(text());
    const QString before = m_undo.takeLast();
    reload(before);
    Q_EMIT historyChanged();
    Q_EMIT edited(before);
}

void DataTableModel::redo()
{
    if (m_redo.isEmpty())
        return;
    m_undo.append(text());
    const QString after = m_redo.takeLast();
    reload(after);
    Q_EMIT historyChanged();
    Q_EMIT edited(after);
}

} // namespace jot
