#pragma once

#include <QString>
#include <QStringList>

namespace jot {

/// What a to-do says about itself on its own line, in the Obsidian Tasks plugin's format, so
/// vaults stay readable there: `📅 2026-10-29` when it is due, with a time after it if it has
/// one (`📅 2026-10-29 14:00`, Jot's own addition), `🔁 every week` how it repeats, and
/// `✅ 2026-10-28` when it was done. `@labels` sort it, as Todoist's labels do.
struct TaskLine
{
    /// The to-do's words, without what is above; #tags and links stay.
    QString title;
    /// "2026-10-29", or "2026-10-29T14:00" with a time; empty when it is not due.
    QString due;
    /// How it repeats, as written after 🔁, such as "every week"; empty when it does not.
    QString repeat;
    /// The day it was done, from ✅; empty when it was not, or not said.
    QString done;
    /// Its labels, without the @, in the order written.
    QStringList labels;

    bool operator==(const TaskLine &) const = default;
};

/// Reads a to-do's text, as it follows `- [ ] ` on its line.
[[nodiscard]] TaskLine readTaskLine(const QString &text);

} // namespace jot
