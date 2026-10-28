#pragma once

#include <QDate>
#include <QList>
#include <QString>
#include <QStringList>
#include <QStringView>

#include <functional>
#include <optional>

namespace jot {

/// What a note's Markdown body holds that the index cares about. Code blocks and code
/// spans are skipped, since a `#include` or `[[x]]` in code is not a tag or a link.
struct NoteScan
{
    struct Link
    {
        /// The target as written, without the label: "Mara", "Mara#Voice", "a%20b.md".
        QString raw;
        /// "link", "wiki" or "embed".
        QString kind;
        /// Counted from 1, in the whole file.
        int line = 0;

        bool operator==(const Link &) const = default;
    };
    struct Task
    {
        int line = 0;
        bool done = false;
        QString text;

        bool operator==(const Task &) const = default;
    };

    /// Inline `#tags`, without the `#`, in the order first seen.
    QStringList tags;
    QList<Link> links;
    QList<Task> tasks;
};

/// Scans `body`, whose first line is line `firstLine` of the file.
[[nodiscard]] NoteScan scanNote(QStringView body, int firstLine = 1);

/// `body` as Markdown for reading: [[wikilinks]] become links to `jot-note:` targets,
/// ![[embeds]] images at the URL `embedUrl` gives for their target (links when it gives
/// none), and #tags links to `jot-tag:` ones. Images from the web become links to them, so
/// drawing the note fetches nothing. Targets are percent-encoded. Code is left alone.
[[nodiscard]] QString readingMarkdown(QStringView body,
                                      const std::function<QString(const QString &)> &embedUrl);

/// `body` with link targets changed: `replace` gets each link's target as written, without
/// its label, and whether it is a [[wikilink]], and gives a new target or nothing to leave
/// it. Code is left alone.
[[nodiscard]] QString
rewriteLinks(QStringView body,
             const std::function<std::optional<QString>(const QString &raw, bool wiki)> &replace);

/// `note` with the task on `line` (counted from 1, in the whole file) ticked or unticked,
/// and nothing else changed. Given `text`, the task must say that: when lines have moved
/// since `line` was read, the one task that does is changed instead. Ticking one that repeats
/// on `today` puts its next one above it, as completedTaskLine() says. Nothing when there is
/// no such task.
[[nodiscard]] std::optional<QString> setTaskDone(const QString &note, int line, bool done,
                                                 const QString &text = {},
                                                 const QDate &today = QDate::currentDate());

/// What ticking a checkbox done changes: the stretch of text from `start` to `end`, its whole
/// line, becomes `text`.
struct TickEdit
{
    qsizetype start = 0;
    qsizetype end = 0;
    QString text;
};

/// The edit that ticks done the checkbox on the line holding `position` in `note`, on `today`,
/// as completedTaskLine() says: the next one above it when it repeats.
[[nodiscard]] TickEdit tickedLineAt(QStringView note, qsizetype position, const QDate &today);

/// Where `line` (counted from 1) starts in `text`, or its end for a line past the last.
[[nodiscard]] qsizetype lineStart(QStringView text, int line);

/// The number of the file's line that `body` starts on, given the whole note.
[[nodiscard]] int bodyFirstLine(QStringView note, QStringView body);

} // namespace jot
