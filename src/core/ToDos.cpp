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

QDateTime reminderAt(const ToDo &todo, const QTime &allDay)
{
    const QDate day = todo.dueDate();
    if (!day.isValid())
        return {};
    // A time as Jot writes it, "T14:00", or as YAML may, " 14:00" or "T14:00:00Z".
    const QString written = todo.due.mid(10).trimmed().remove(u'T');
    if (written.endsWith(u'Z') || written.contains(u'+')) {
        const QDateTime exact = QDateTime::fromString(todo.due, Qt::ISODate);
        if (exact.isValid())
            return exact.toLocalTime();
    }
    const QTime time = QTime::fromString(written.left(5), u"HH:mm"_s);
    return QDateTime(day, time.isValid() ? time : allDay);
}

QDateTime ToDo::doneAt() const
{
    if (done.isEmpty())
        return {};
    // A moment as written in `done:`, "2026-10-29T14:00:00Z", or a day as after ✅.
    const QDateTime exact = QDateTime::fromString(done.trimmed(), Qt::ISODate);
    if (exact.isValid() && done.size() > 10)
        return exact.toLocalTime();
    const QDate day = QDate::fromString(done.left(10), Qt::ISODate);
    return day.isValid() ? QDateTime(day, QTime(0, 0)) : QDateTime();
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
    if (name == u"done")
        return ToDoView::Done;
    return std::nullopt;
}

QList<ToDo> toDos(const VaultIndex &index, ToDoView view, const QDate &today, const QString &tag)
{
    const bool done = view == ToDoView::Done;
    QList<ToDo> all;
    for (const NoteInfo &note : done ? index.doneTaskNotes() : index.openTaskNotes())
        all.append({u"note"_s,
                    note.path,
                    0,
                    note.title,
                    {},
                    note.due.trimmed(),
                    {},
                    note.tags,
                    {},
                    note.done.trimmed()});
    QHash<QString, QString> titles;
    for (const NoteTask &task : done ? index.doneTasks() : index.openTasks()) {
        if (!titles.contains(task.path)) {
            const std::optional<NoteInfo> info = index.note(task.path);
            titles.insert(task.path, info ? info->title : task.path);
        }
        all.append({u"task"_s, task.path, task.line, task.parts.title, titles.value(task.path),
                    task.parts.due, task.parts.repeat, scanNote(task.parts.title).tags, task.text,
                    task.parts.done});
    }

    QList<ToDo> shown;
    for (const ToDo &each : std::as_const(all)) {
        const QDate due = each.dueDate();
        const bool wanted = view == ToDoView::All || done ||
                            (view == ToDoView::Today && due.isValid() && due <= today) ||
                            (view == ToDoView::Upcoming && due.isValid() && due > today) ||
                            (view == ToDoView::Anytime && !due.isValid());
        if (!wanted)
            continue;
        if (!tag.isEmpty() && !hasTag(each.tags, tag))
            continue;
        shown.append(each);
    }
    if (done) {
        std::ranges::stable_sort(shown, [](const ToDo &a, const ToDo &b) {
            const QDateTime aAt = a.doneAt();
            const QDateTime bAt = b.doneAt();
            if (aAt.isValid() != bAt.isValid())
                return aAt.isValid();
            return aAt > bAt;
        });
        return shown;
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
