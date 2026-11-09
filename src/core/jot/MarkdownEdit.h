#pragma once

#include <QString>
#include <QVariant>

#include <optional>

namespace jot {

/// The formatting a toolbar or shortcut applies to Markdown text, done to the text itself.
/// Each takes the note and a selection, and toggles: applying a style the selection
/// already has takes it away.
namespace markdownedit {

/// One change to make: replace `start` to `end` with `text`, then select
/// `selectionStart` to `selectionEnd` in the changed note.
struct Edit
{
    qsizetype start = 0;
    qsizetype end = 0;
    QString text;
    qsizetype selectionStart = 0;
    qsizetype selectionEnd = 0;

    bool operator==(const Edit &) const = default;
};

/// Styles whole lines take.
enum class LineStyle { Bullet, Numbered, Task, Quote, Heading1, Heading2, Heading3 };

/// Puts `marker` (`**`, `*`, `~~` or a backtick) around the selection, or the word at the
/// cursor, or takes it away. A selection over several lines marks each line.
[[nodiscard]] Edit wrap(const QString &text, qsizetype from, qsizetype to, const QString &marker);

/// Gives every line the selection touches `style`, or takes it away when they all have it.
/// Lists replace one another, so a bullet becomes a task.
[[nodiscard]] Edit styleLines(const QString &text, qsizetype from, qsizetype to, LineStyle style);

/// Makes the selection a link's label, or its target when it is a URL, and leaves the
/// cursor where the other half goes.
[[nodiscard]] Edit link(const QString &text, qsizetype from, qsizetype to);

/// Fences the lines the selection touches as a code block, or unfences them.
[[nodiscard]] Edit codeBlock(const QString &text, qsizetype from, qsizetype to);

/// What Return does on a list item or quote: starts the next item, with the next number or
/// an unticked box, or ends the list when the item is empty. Nothing when the cursor is not
/// in an item's text, so Return breaks the line as usual.
[[nodiscard]] std::optional<Edit> continueList(const QString &text, qsizetype from, qsizetype to);

/// Nests the list items the selection touches one level deeper, or one level out. Nothing
/// when any line touched is not a list item, so Tab types a tab as usual.
[[nodiscard]] std::optional<Edit> indentList(const QString &text, qsizetype from, qsizetype to,
                                             bool outdent);

/// Lines up the table the selection starts in, padding cells so the pipes align, keeping
/// the selection in its cells. Nothing outside a table.
[[nodiscard]] std::optional<Edit> formatTable(const QString &text, qsizetype from, qsizetype to);

/// Tab and Shift+Tab in a table: lines it up and selects the next or previous cell, adding
/// a row after the last cell. Nothing outside a table.
[[nodiscard]] std::optional<Edit> moveInTable(const QString &text, qsizetype from, bool backwards);

/// Return in a table: lines it up and starts a row under the cursor's, or ends the table
/// when the cursor's row is empty. Nothing outside a table.
[[nodiscard]] std::optional<Edit> newTableRow(const QString &text, qsizetype from);

/// A two-column table on lines of its own at the cursor, with its first heading selected.
[[nodiscard]] Edit insertTable(const QString &text, qsizetype from, qsizetype to);

/// A wikilink being typed: where its target starts, just after `[[`, and what is typed so far.
struct WikilinkQuery
{
    qsizetype start = 0;
    QString text;
};

/// The wikilink the cursor is typing, if it is after an unclosed `[[` on its line and
/// before any `|` or `#` that ends the target. Nothing in code.
[[nodiscard]] std::optional<WikilinkQuery> wikilinkAt(const QString &text, qsizetype cursor);

/// An emoji name being typed after a colon, as `:thumbs`: where the colon is, and the name
/// so far with its underscores as spaces.
struct EmojiQuery
{
    qsizetype start = 0;
    QString text;
};

/// The emoji name the cursor is typing: two or more letters, digits, `_`, `+` or `-` after a
/// colon that starts the line or follows a space or an opening bracket or quote. Nothing in
/// code, and nothing for a time such as 10:30 or a link such as https://.
[[nodiscard]] std::optional<EmojiQuery> emojiAt(const QString &text, qsizetype cursor);

/// Puts `target` in the wikilink being typed at the cursor, closing it with `]]` unless it
/// is closed already, and leaves the cursor after the link. A `|label` or `#heading` the link
/// has already stays.
[[nodiscard]] std::optional<Edit> completeWikilink(const QString &text, qsizetype cursor,
                                                   const QString &target);

/// Sets front matter `key` to `value`, or removes it when `value` is null, keeping the
/// selection on the same text.
[[nodiscard]] Edit setKey(const QString &text, qsizetype from, qsizetype to, const QString &key,
                          const QVariant &value);

/// The smallest change from `before` to `after`, with the selection given in `after`.
[[nodiscard]] Edit difference(const QString &before, const QString &after, qsizetype selectionStart,
                              qsizetype selectionEnd);

} // namespace markdownedit

} // namespace jot
