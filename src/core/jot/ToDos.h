#pragma once

#include "jot/VaultIndex.h"

#include <QDate>
#include <QDateTime>
#include <QList>
#include <QString>
#include <QStringList>

#include <optional>

namespace jot {

/// A to-do: a checkbox in a note, or a note that is a task.
struct ToDo
{
    /// "note" for a task note, "task" for a checkbox.
    QString kind;
    QString path;
    /// Its line in the note, counted from 1; 0 for a task note.
    int line = 0;
    /// Its words: the checkbox's without its markers, or the task note's title.
    QString title;
    /// The title of the note a checkbox is in; empty for a task note.
    QString note;
    /// "2026-10-29", or "2026-10-29T14:00" with a time; empty when it is not due.
    QString due;
    /// How it repeats, as written, such as "every week".
    QString repeat;
    /// 1 urgent, 2 high, 3 medium, 4 low; 0 for none.
    int priority = 0;
    /// A checkbox's own #tags, or a task note's.
    QStringList tags;
    /// A checkbox's text as written, markers and all, which ticking it looks for; empty for a
    /// task note.
    QString text;
    /// When it was made: its note's `created:`, or the file's last change when that is not
    /// written. A checkbox goes by the note it is in.
    QDateTime created;
    /// When it was done, from a checkbox's @done or a task note's `done:`; empty when it is open or
    /// that was not written.
    QString done;

    /// The day it is due, invalid when it is not.
    [[nodiscard]] QDate dueDate() const;
    /// When it was done, here; invalid when it is open or that was not written. A day alone is
    /// that day's midnight.
    [[nodiscard]] QDateTime doneAt() const;

    bool operator==(const ToDo &) const = default;
};

/// Which to-dos a list shows, as Todoist's views do.
enum class ToDoView {
    /// Overdue, and due today.
    Today,
    /// Due after today, soonest first.
    Upcoming,
    /// Not due on any day.
    Anytime,
    /// All of them.
    All,
    /// Those done, most recently first.
    Done,
};

/// What a list of to-dos is ordered by.
enum class ToDoSort {
    /// Soonest first, a day's untimed ones before its timed; those with no day after.
    Due,
    /// Oldest first, then by their place in the note.
    Created,
    /// A to Z, numbers in order.
    Title,
    /// Urgent first; those with none after.
    Priority,
};

/// The order a name gives, "due", "created", "title" or "priority", or nothing for another.
[[nodiscard]] std::optional<ToDoSort> toDoSort(const QString &name);

/// `list` ordered by `sort`, the other way round when `descending`, though those with no day or
/// priority stay last. Ties keep their order.
void sortToDos(QList<ToDo> &list, ToDoSort sort, bool descending);

/// When a reminder about `todo` comes: when it is due, or at `allDay` on its day when it is due
/// on a day without a time. Invalid when it is not due.
[[nodiscard]] QDateTime reminderAt(const ToDo &todo, const QTime &allDay);

/// The view a name gives, "today", "upcoming", "anytime", "all" or "done", or nothing for
/// another.
[[nodiscard]] std::optional<ToDoView> toDoView(const QString &name);

/// The to-dos in `view` as of `today`, with `tag` or one nested in it when it is given, and of
/// `priority`, 0 for none, when that is. Open ones
/// that are dated come soonest first, a day's untimed ones before its timed ones; undated ones come
/// task notes first, then by note. Done ones come most recently done first, then those with no day
/// written.
[[nodiscard]] QList<ToDo> toDos(const VaultIndex &index, ToDoView view, const QDate &today,
                                const QString &tag = {},
                                std::optional<int> priority = std::nullopt);

} // namespace jot
