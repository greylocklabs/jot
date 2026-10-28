#include "TaskModel.h"

#include <QDateTime>
#include <QLocale>

using namespace Qt::StringLiterals;

namespace jot {

TaskModel::TaskModel(QObject *parent) : QAbstractListModel(parent) {}

void TaskModel::reload(const VaultIndex *index)
{
    m_index = index;
    refill();
}

void TaskModel::setView(const QString &view)
{
    if (m_view == view || !toDoView(view))
        return;
    m_view = view;
    Q_EMIT filterChanged();
    refill();
}

void TaskModel::setLabel(const QString &label)
{
    if (m_label == label)
        return;
    m_label = label;
    Q_EMIT filterChanged();
    refill();
}

void TaskModel::setTag(const QString &tag)
{
    if (m_tag == tag)
        return;
    m_tag = tag;
    Q_EMIT filterChanged();
    refill();
}

void TaskModel::refill()
{
    beginResetModel();
    m_today = QDate::currentDate();
    m_rows = m_index ? toDos(*m_index, toDoView(m_view).value_or(ToDoView::Today), m_today, m_label,
                             m_tag)
                     : QList<ToDo>();
    m_todayCount = m_index ? int(toDos(*m_index, ToDoView::Today, m_today).size()) : 0;
    endResetModel();
    Q_EMIT countChanged();
}

int TaskModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_rows.size());
}

QVariant TaskModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_rows.size())
        return {};
    const ToDo &row = m_rows.at(index.row());
    const QDate due = row.dueDate();
    const QLocale locale;
    switch (role) {
    case KindRole: return row.kind;
    case PathRole: return row.path;
    case TitleRole: return row.title;
    case NoteRole: return row.note;
    case LineRole: return row.line;
    case DueRole: {
        if (!due.isValid())
            return QString();
        const QTime time = QTime::fromString(row.due.mid(11, 5), u"HH:mm"_s);
        // Under a day's heading the day goes without saying, and only a time is worth adding.
        const bool byDay = m_view != u"anytime";
        if (byDay && due >= m_today)
            return time.isValid() ? locale.toString(time, QLocale::ShortFormat) : QString();
        const QString day = locale.toString(due, u"MMM d"_s);
        return time.isValid() ? day + u' ' + locale.toString(time, QLocale::ShortFormat) : day;
    }
    case OverdueRole: return due.isValid() && due < m_today;
    case LabelsRole: return row.labels;
    case RepeatsRole: return !row.repeat.isEmpty();
    case GroupRole:
        if (due.isValid() && due < m_today)
            return tr("Overdue");
        if (due == m_today)
            return tr("Today");
        if (due == m_today.addDays(1))
            return tr("Tomorrow");
        if (due.isValid())
            return locale.toString(due, due.year() == m_today.year() ? u"dddd, MMMM d"_s
                                                                     : u"dddd, MMMM d, yyyy"_s);
        return row.kind == u"note" ? tr("Task notes") : row.note;
    }
    return {};
}

QHash<int, QByteArray> TaskModel::roleNames() const
{
    return {{KindRole, "kind"},       {PathRole, "path"},     {TitleRole, "title"},
            {NoteRole, "note"},       {LineRole, "line"},     {DueRole, "due"},
            {OverdueRole, "overdue"}, {LabelsRole, "labels"}, {RepeatsRole, "repeats"},
            {GroupRole, "group"}};
}

} // namespace jot
