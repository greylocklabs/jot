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
    // A moment as written in `done:`, "2026-10-29T14:00:00Z", or a day as after @done.
    const QDateTime exact = QDateTime::fromString(done.trimmed(), Qt::ISODate);
    if (exact.isValid() && done.size() > 10)
        return exact.toLocalTime();
    const QDate day = QDate::fromString(done.left(10), Qt::ISODate);
    return day.isValid() ? QDateTime(day, QTime(0, 0)) : QDateTime();
}

std::optional<ToDoSort> toDoSort(const QString &name)
{
    if (name == u"due")
        return ToDoSort::Due;
    if (name == u"created")
        return ToDoSort::Created;
    if (name == u"title")
        return ToDoSort::Title;
    if (name == u"priority")
        return ToDoSort::Priority;
    return std::nullopt;
}

void sortToDos(QList<ToDo> &list, ToDoSort sort, bool descending)
{
    // Whether `a` goes before `b` the way round asked for; those with nothing to sort by, no
    // day or no priority, go last either way.
    const auto before = [sort, descending](const ToDo &a, const ToDo &b) {
        const auto ordered = [descending](const auto &x, const auto &y) {
            return descending ? y < x : x < y;
        };
        switch (sort) {
        case ToDoSort::Due: {
            const QDate aDay = a.dueDate();
            const QDate bDay = b.dueDate();
            if (aDay.isValid() != bDay.isValid())
                return aDay.isValid();
            if (!aDay.isValid())
                return false;
            // By the clock too, whether written "T14:00" or " 14:00"; untimed first in a day.
            const auto aAt = std::pair{aDay, a.due.mid(11, 5)};
            const auto bAt = std::pair{bDay, b.due.mid(11, 5)};
            return aAt != bAt && ordered(aAt, bAt);
        }
        case ToDoSort::Created: return a.created != b.created && ordered(a.created, b.created);
        case ToDoSort::Title: {
            const int compared = naturalCompare(a.title, b.title);
            return compared != 0 && (descending ? compared > 0 : compared < 0);
        }
        case ToDoSort::Priority:
            if ((a.priority == 0) != (b.priority == 0))
                return a.priority != 0;
            return a.priority != b.priority && ordered(a.priority, b.priority);
        }
        return false;
    };
    std::ranges::stable_sort(list, before);
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

QList<ToDo> toDos(const VaultIndex &index, ToDoView view, const QDate &today, const QString &tag,
                  std::optional<int> priority)
{
    const bool done = view == ToDoView::Done;
    // When a note was made: its own word for it, or else when its file last changed.
    const auto createdOf = [](const NoteInfo &note) {
        const QDateTime written = QDateTime::fromString(note.created.trimmed(), Qt::ISODate);
        return written.isValid() ? written
                                 : QDateTime::fromMSecsSinceEpoch(note.modified / 1'000'000);
    };
    QList<ToDo> all;
    for (const NoteInfo &note : done ? index.doneTaskNotes() : index.openTaskNotes())
        all.append({u"note"_s,
                    note.path,
                    0,
                    note.title,
                    {},
                    note.due.trimmed(),
                    note.repeat.trimmed(),
                    note.priority,
                    note.tags,
                    {},
                    createdOf(note),
                    note.done.trimmed()});
    QHash<QString, std::pair<QString, QDateTime>> notes;
    for (const NoteTask &task : done ? index.doneTasks() : index.openTasks()) {
        if (!notes.contains(task.path)) {
            const std::optional<NoteInfo> info = index.note(task.path);
            notes.insert(task.path, info ? std::pair{info->title, createdOf(*info)}
                                         : std::pair{task.path, QDateTime()});
        }
        const auto &[title, created] = notes.value(task.path);
        all.append({u"task"_s, task.path, task.line, task.parts.title, title, task.parts.due,
                    task.parts.repeat, task.parts.priority, scanNote(task.parts.title).tags,
                    task.text, created, task.parts.done});
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
        if (priority && each.priority != *priority)
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
    sortToDos(shown, ToDoSort::Due, false);
    return shown;
}

} // namespace jot
