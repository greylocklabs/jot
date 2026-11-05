#include "DataTableModel.h"

#include <QHash>

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
        if (!m_order.isEmpty() && columnCount() > 0)
            Q_EMIT dataChanged(index(0, 0), index(rowCount() - 1, columnCount() - 1));
    } else {
        beginResetModel();
        m_table = std::move(next);
        arrange();
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
