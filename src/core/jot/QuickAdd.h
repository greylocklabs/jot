#pragma once

#include <QDateTime>
#include <QString>
#include <QStringList>

#include <optional>

namespace jot {

/// A to-do as typed into quick add, read the way a to-do app reads it.
struct QuickTask
{
    /// The words left once the day, time and repeat are taken out; #tags stay.
    QString title;
    /// "2026-10-29", or "2026-10-29T14:00" with a time; empty when no day was given.
    QString due;
    /// How it repeats, as a rule such as "every week".
    QString repeat;
    /// 1 urgent to 4 low, from `p1` to `p4`, with or without an @; 0 for none.
    int priority = 0;
    /// The to-do as its checkbox line says it, without `- [ ] `.
    QString line;
    /// The words named a day, rather than it being today for a time alone or a repeat's first.
    bool dayGiven = false;
};

/// Reads `typed`, such as "call Soren tomorrow 3pm #calls", as of `now`. It understands
/// today, tomorrow, weekday names (the next one after today), next week, next month, the
/// weekend, in N days, weeks or months, a month and day such as "oct 29", ISO dates, times
/// such as "3pm", "3:30 pm", "15:00" and "noon", and repeats such as "every week", "every 2
/// weeks", "every monday", "every weekday" and "daily". A time alone means today; a repeat
/// alone starts on its first day. `p1` to `p4` is its priority. Markers typed as written, such as
/// `@weekly` or `@due 2026-11-02`, are taken as they say.
[[nodiscard]] QuickTask readQuickTask(const QString &typed, const QDateTime &now);

/// `text`, a checkbox's words after `- [ ] `, due on `due` and repeating as `repeat`, either
/// empty to take it away, in Jot's markers, `@weekly @due 2026-11-02`. Its priority and done day
/// stay.
[[nodiscard]] QString withTaskSchedule(const QString &text, const QString &due,
                                       const QString &repeat);

/// `text`, a checkbox's words, with a day, time or repeat said in them, such as "water the
/// plants every week" or "call Soren tomorrow 3pm", written as markers as quick add does. A day
/// or repeat it says replaces the one marked already. Nothing when it says neither.
[[nodiscard]] std::optional<QString> scheduledFromWords(const QString &text, const QDateTime &now);

/// `text`, a checkbox's words, due and repeating as `when` says in words, such as "friday 3pm
/// every week", or with neither when `when` is empty. When `when` says only `text`'s own repeat
/// rule besides the day, that rule stays as written, even where quick add reads less of it,
/// such as "every week when done". Nothing when `when` says no day or repeat.
[[nodiscard]] std::optional<QString> scheduledAs(const QString &text, const QString &when,
                                                 const QDateTime &now);

class Vault;

/// The note quick add puts to-dos in: `inbox` in `.jot/vault.yaml`, or Inbox.md.
[[nodiscard]] QString inboxPath(const Vault &vault);

/// `note` with a checkbox saying `line` after everything in it, in the note's own line breaks.
[[nodiscard]] QString withTaskAdded(const QString &note, const QString &line);

} // namespace jot
