#pragma once

#include <QDate>
#include <QList>
#include <QString>
#include <QStringView>

namespace jot {

/// One of a to-do's markers, the plain words after its box that say when it is due, how it
/// repeats and when it was done: `@due 2026-10-29`, with a time after it if it has one
/// (`@due 2026-10-29 14:00`), `@daily`, `@weekly`, `@monthly`, `@yearly`, `@weekdays` or
/// `@every 2 weeks on monday`, and `@done 2026-10-28`. Each starts a word, case aside.
struct TaskMarker
{
    enum Kind { Due, Repeat, Done };
    Kind kind = Due;
    /// Where it is in the to-do's text, from its @ to the end of its value.
    qsizetype start = 0;
    qsizetype end = 0;
    /// What it says: the day, "2026-10-29" or "2026-10-29T14:00", for Due and Done, and the
    /// rule for Repeat, such as "every week".
    QString value;

    bool operator==(const TaskMarker &) const = default;
};

/// The markers in a to-do's text, in order. An `@every` rule runs up to the next @ or # word,
/// or the end; `@due` and `@done` need a day after them, and are only words without one.
[[nodiscard]] QList<TaskMarker> taskMarkers(QStringView text);

/// What a to-do says about itself on its own line, read from its markers.
struct TaskLine
{
    /// The to-do's words, without its markers; #tags and links stay.
    QString title;
    /// "2026-10-29", or "2026-10-29T14:00" with a time; empty when it is not due.
    QString due;
    /// How it repeats, as a rule such as "every week"; empty when it does not.
    QString repeat;
    /// The day it was done; empty when it was not, or not said.
    QString done;

    bool operator==(const TaskLine &) const = default;
};

/// Reads a to-do's text, as it follows `- [ ] ` on its line.
[[nodiscard]] TaskLine readTaskLine(const QString &text);

/// The marker that writes `rule`: `@weekly` for "every week" and the like, else
/// `@every 2 weeks`.
[[nodiscard]] QString repeatMarker(const QString &rule);
/// `@due 2026-10-29`, with ` 14:00` when `due` has a time.
[[nodiscard]] QString dueMarker(const QString &due);
/// `@done 2026-10-28`.
[[nodiscard]] QString doneMarker(const QDate &day);
/// `text` without its markers of `kinds`, its spaces tidied where they were.
[[nodiscard]] QString withoutMarkers(const QString &text, const QList<TaskMarker::Kind> &kinds);

/// A marker offered as one is typed: what goes in, how it is shown, and what it does.
struct MarkerSuggestion
{
    QString text;
    QString shown;
    QString detail;

    bool operator==(const MarkerSuggestion &) const = default;
};

/// The markers that start with `typed`, the letters after an @ ("we" for @weekly and @weekdays),
/// as of `today`. A due day is offered as the day itself, `@due 2026-11-02`.
[[nodiscard]] QList<MarkerSuggestion> markerSuggestions(const QString &typed, const QDate &today);

/// The day after `from` that a repeat falls on: "every day", "every 2 weeks", "every month",
/// "every year", "every weekday", "every monday" and the like. Invalid for a rule Jot does not
/// read.
[[nodiscard]] QDate nextOccurrence(const QString &rule, const QDate &from);

/// A whole checkbox line, `- [ ] …`, ticked done on `today`, which it says with `@done`. When it
/// repeats and is due, the next one goes above it, unticked and due on its next day after today.
[[nodiscard]] QString completedTaskLine(const QString &line, const QDate &today);

} // namespace jot
