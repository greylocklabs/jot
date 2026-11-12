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

/// A heading in a note: its level, 1 to 6, its text without the marks, and where its line
/// starts.
struct Heading
{
    int level = 1;
    QString text;
    qsizetype position = 0;
    /// In a quote or a list, which a table of contents leaves out.
    bool nested = false;
};

/// Every heading a reader draws for `body`, nested ones too, in order.
[[nodiscard]] QList<Heading> allHeadingsOf(QStringView body);

/// The headings of `body`, in order: `#` ones, and a line of text underlined with `=` or `-`.
/// Code is left alone, and so are headings in quotes and lists.
[[nodiscard]] QList<Heading> headingsOf(QStringView body);

/// The `$...$` math in a line of prose, as Pandoc reads it: each span's start and end, its
/// dollar signs included. The first has a character other than a space after it, the second
/// one before it and no digit after it, so prices stay text; `\$` is a dollar sign, and
/// nothing in code counts.
[[nodiscard]] QList<std::pair<qsizetype, qsizetype>> inlineMath(QStringView line);

/// Whether `line` opens or closes display math, being `$$` and nothing else but spaces, or
/// holds all of it, as `$$x^2$$`. The math in the second is put in `math`.
[[nodiscard]] bool isMathFence(QStringView line, QString *math = nullptr);

/// Whether `line` is a table of contents marker, `${toc}` as Joplin writes it.
[[nodiscard]] bool isTocMarker(QStringView line);

/// `body` as Markdown for reading: [[wikilinks]] become links to `jot-note:` targets,
/// ![[embeds]] images at the URL `embedUrl` gives for their target (links when it gives
/// none), and #tags links to `jot-tag:` ones. Images from the web become links to them, so
/// drawing the note fetches nothing. Embedded sounds become links to `jot-sound:` targets, or
/// `jot-wiki-sound:` for wikilinks, and videos to `jot-video:` or `jot-wiki-video:` ones, named
/// for their file when they have no words. Targets are
/// percent-encoded. A table of contents marker becomes a list of links to `jot-heading:` targets,
/// each heading's number from 0. CriticMarkup reads as its edits made, with deletions struck
/// through and comments in italics. Math becomes the picture at the URL `mathUrl` gives for its
/// TeX, and whether it is set apart, and stays as written when it gives none. Code is left alone.
[[nodiscard]] QString
readingMarkdown(QStringView body, const std::function<QString(const QString &)> &embedUrl,
                const std::function<QString(const QString &, bool)> &mathUrl = {});

/// `body` with link targets changed: `replace` gets each link's target as written, without
/// its label, and whether it is a [[wikilink]], and gives a new target or nothing to leave
/// it. Code is left alone.
[[nodiscard]] QString
rewriteLinks(QStringView body,
             const std::function<std::optional<QString>(const QString &raw, bool wiki)> &replace);

/// `note` with the words of the task on `line` (counted from 1, in the whole file), which say
/// `text`, replaced with `now`, its box and the rest of the note as they were. Found as
/// setTaskDone() finds it; nothing when there is no such task.
[[nodiscard]] std::optional<QString> setTaskText(const QString &note, int line, const QString &text,
                                                 const QString &now);

/// `note` with the task on `line` (counted from 1, in the whole file) ticked or unticked,
/// and nothing else changed. Given `text`, the task must say that: when lines have moved
/// since `line` was read, the one task that does is changed instead. Ticking one writes
/// `today` after it, and one that repeats gets its next one above it, as completedTaskLine()
/// says; unticking one takes away its `@done` day. Nothing when there is no such task.
[[nodiscard]] std::optional<QString> setTaskDone(const QString &note, int line, bool done,
                                                 const QString &text = {},
                                                 const QDate &today = QDate::currentDate());

/// Where a checkbox's words are in a note: from `start` to `end`, after its box.
struct TaskWords
{
    qsizetype start = 0;
    qsizetype end = 0;
};

/// The words of the checkbox on the line at `position`, or nothing when that line is no
/// checkbox.
[[nodiscard]] std::optional<TaskWords> taskWordsAt(QStringView note, qsizetype position);

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
