#include "jot/ToDos.h"

#include "jot/NoteScan.h"

#include <QHash>

#include <algorithm>

using namespace Qt::StringLiterals;

namespace jot {

namespace {

bool hasTag(const QStringList &tags, const QString &tag)
{
    return std::ranges::any_of(tags, [&tag](const QString &each) {
        return each.compare(tag, Qt::CaseInsensitive) == 0 ||
               each.startsWith(tag + u'/', Qt::CaseInsensitive);
    });
}

} // namespace

QDate ToDo::dueDate() const
{
    return QDate::fromString(due.left(10), Qt::ISODate);
}

std::optional<ToDoView> toDoView(const QString &name)
{
    if (name == u"today")
        return ToDoView::Today;
    if (name == u"upcoming")
        return ToDoView::Upcoming;
    if (name == u"anytime")
        return ToDoView::Anytime;
    if (name == u"all")
        return ToDoView::All;
    return std::nullopt;
}

QList<ToDo> toDos(const VaultIndex &index, ToDoView view, const QDate &today, const QString &label,
                  const QString &tag)
{
    QList<ToDo> all;
    for (const NoteInfo &note : index.openTaskNotes())
        all.append({u"note"_s,
                    note.path,
                    0,
                    note.title,
                    {},
                    note.due.trimmed(),
                    {},
                    note.labels,
                    note.tags});
    QHash<QString, QString> titles;
    for (const NoteTask &task : index.openTasks()) {
        if (!titles.contains(task.path)) {
            const std::optional<NoteInfo> info = index.note(task.path);
            titles.insert(task.path, info ? info->title : task.path);
        }
        all.append({u"task"_s, task.path, task.line, task.parts.title, titles.value(task.path),
                    task.parts.due, task.parts.repeat, task.parts.labels,
                    scanNote(task.parts.title).tags});
    }

    QList<ToDo> shown;
    for (const ToDo &each : std::as_const(all)) {
        const QDate due = each.dueDate();
        const bool wanted = view == ToDoView::All ||
                            (view == ToDoView::Today && due.isValid() && due <= today) ||
                            (view == ToDoView::Upcoming && due.isValid() && due > today) ||
                            (view == ToDoView::Anytime && !due.isValid());
        if (!wanted)
            continue;
        if (!label.isEmpty() && !each.labels.contains(label, Qt::CaseInsensitive))
            continue;
        if (!tag.isEmpty() && !hasTag(each.tags, tag))
            continue;
        shown.append(each);
    }
    // Dated ones by day, then by the clock whether written "T14:00" or " 14:00"; the rest keep
    // the index's order.
    std::ranges::stable_sort(shown, [](const ToDo &a, const ToDo &b) {
        const QDate aDay = a.dueDate();
        const QDate bDay = b.dueDate();
        if (aDay.isValid() != bDay.isValid())
            return aDay.isValid();
        if (!aDay.isValid())
            return false;
        return aDay != bDay ? aDay < bDay : a.due.mid(11, 5) < b.due.mid(11, 5);
    });
    return shown;
}

} // namespace jot
