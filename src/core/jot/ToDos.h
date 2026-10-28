#pragma once

#include "jot/VaultIndex.h"

#include <QDate>
#include <QList>
#include <QString>
#include <QStringList>

namespace jot {

/// A to-do still open: a checkbox in a note, or a note that is a task.
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
    QStringList labels;
    /// A checkbox's own #tags, or a task note's.
    QStringList tags;

    /// The day it is due, invalid when it is not.
    [[nodiscard]] QDate dueDate() const;

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
};

/// The view a name gives, "today", "upcoming", "anytime" or "all", or nothing for another.
[[nodiscard]] std::optional<ToDoView> toDoView(const QString &name);

/// The open to-dos in `view` as of `today`, with `label` and with `tag` or one nested in it
/// when those are given. Dated ones come soonest first, a day's untimed ones before its timed
/// ones; undated ones come task notes first, then by note.
[[nodiscard]] QList<ToDo> toDos(const VaultIndex &index, ToDoView view, const QDate &today,
                                const QString &label = {}, const QString &tag = {});

} // namespace jot
