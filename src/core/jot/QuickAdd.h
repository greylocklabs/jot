#pragma once

#include <QDateTime>
#include <QString>
#include <QStringList>

namespace jot {

/// A to-do as typed into quick add, read the way a to-do app reads it.
struct QuickTask
{
    /// The words left once the day, time and repeat are taken out; @labels and #tags stay.
    QString title;
    /// "2026-10-29", or "2026-10-29T14:00" with a time; empty when no day was given.
    QString due;
    /// How it repeats, as Obsidian Tasks writes it after 🔁, such as "every week".
    QString repeat;
    /// The to-do as its checkbox line says it, without `- [ ] `.
    QString line;
};

/// Reads `typed`, such as "call Soren tomorrow 3pm @minutes", as of `now`. It understands
/// today, tomorrow, weekday names (the next one after today), next week, next month, the
/// weekend, in N days, weeks or months, a month and day such as "oct 29", ISO dates, times
/// such as "3pm", "3:30 pm", "15:00" and "noon", and repeats such as "every week", "every 2
/// weeks", "every monday", "every weekday" and "daily". A time alone means today; a repeat
/// alone starts on its first day.
[[nodiscard]] QuickTask readQuickTask(const QString &typed, const QDateTime &now);

class Vault;

/// The note quick add puts to-dos in: `inbox` in `.jot/vault.yaml`, or Inbox.md.
[[nodiscard]] QString inboxPath(const Vault &vault);

/// `note` with a checkbox saying `line` after everything in it, in the note's own line breaks.
[[nodiscard]] QString withTaskAdded(const QString &note, const QString &line);

} // namespace jot
