#pragma once

#include "jot/CodeColors.h"
#include "jot/Diagram.h"
#include "jot/NoteScan.h"

#include <QColor>
#include <QHash>
#include <QSet>
#include <QSize>
#include <QSyntaxHighlighter>
#include <QTextBlock>
#include <QTextBlockUserData>
#include <QTextCharFormat>
#include <QTextCursor>

#include "jot/WebVideo.h"

#include <functional>
#include <memory>
#include <utility>

namespace Sonnet {
class Speller;
}

namespace jot {

class CodeHighlighter;

/// Styles Markdown as it is typed, in the manner of a live preview, without changing a
/// character: headings grow, emphasis shows, and the marks that make them fade, folding
/// away entirely except on the line the cursor is on. Formats are not part of the text or
/// its undo history, so the note stays exactly what the user wrote.
/// Each heading level's size against the body text, as the editor and the reading view draw it.
inline constexpr double kHeadingScale[] = {1.75, 1.45, 1.22, 1.08, 1.0, 1.0};

class MarkdownHighlighter : public QSyntaxHighlighter
{
    Q_OBJECT

public:
    /// What the styles are made of, from the theme.
    struct Look
    {
        QColor text;
        QColor muted;
        QColor faint;
        QColor accent;
        /// What a CriticMarkup deletion is drawn in.
        QColor danger;
        QColor codeBackground;
        /// The colors code is highlighted in.
        CodeColors code;
        QString family;
        QString monoFamily;
        QString displayFamily;
        int pixelSize = 15;
        int strongWeight = 600;
        int boldWeight = 700;
        /// The widest and tallest an image is shown inline.
        int imageWidth = 600;
        int imageHeight = 360;
        /// The theme diagrams are drawn in.
        DiagramStyle diagram;
    };

    /// An image shown in place of a line that holds only its link, or of a diagram's fence.
    struct Image : QTextBlockUserData
    {
        QString path;
        QSize size;
        /// A sound, drawn as a player rather than a picture.
        bool audio = false;
        /// A video, drawn as a player showing its picture.
        bool video = false;
        /// A video on the web, drawn as a card until played; `path` is then empty.
        std::optional<WebVideo> web;
    };

    /// A table of contents shown in place of a `${toc}` line: room for `rows` headings,
    /// each `rowHeight` tall.
    struct Toc : QTextBlockUserData
    {
        int rows = 0;
        int rowHeight = 0;
    };

    /// Math drawn in a line of prose, in the room its first `$` keeps: where that is in the
    /// line, the drawing, and the size it is shown at.
    struct Math
    {
        int at = 0;
        QString path;
        QSize size;
    };

    /// What a line of prose holds that is drawn beside or over it: its CriticMarkup comments,
    /// `{>>...<<}`, where each starts and what it says, and its math.
    struct Inlines : QTextBlockUserData
    {
        QList<std::pair<int, QString>> comments;
        QList<Math> math;
    };

    /// Finds the file an image link reaches, from its target as written. Empty when it reaches
    /// none.
    using Resolver = std::function<QString(const QString &target)>;

    /// What sits under a position: a link's target, or the box of a task.
    struct Hit
    {
        enum Kind { None, Link, Url, TaskBox };
        Kind kind = None;
        /// The link's target as written, without its label.
        QString target;
        /// For a task box, where its mark (space or x) is in the document.
        int position = -1;
    };

    explicit MarkdownHighlighter(QTextDocument *document);
    ~MarkdownHighlighter() override;

    void setLook(const Look &look);
    void setResolver(Resolver resolver);
    /// The block the cursor is in shows its marks; others fold them away.
    void setCursorPosition(int position);
    /// Shows the Markdown exactly as written, in the monospace font and highlighted, but with
    /// nothing folded, resized or shown as an image.
    void setSourceMode(bool source);
    /// Fades every block but those of the paragraph the cursor is in, while it is in one.
    void setFocusMode(bool focus);
    /// Works out the paragraph focus mode lights again, as after an edit, which can join or
    /// part paragraphs with the cursor where it was.
    void refreshFocus();
    /// Paints the text from `start` to `end` in the selection's color, as after a yank, or
    /// nothing when `end` is not past `start`.
    void setFlash(int start, int end);
    /// Marks the characters at `positions`, as vi's matches for f and t; none when empty.
    void setMatches(const QList<int> &positions);
    /// Paints what find found, `length` long from each of `starts`, the one at `current` more
    /// strongly; none when `starts` is empty.
    void setFound(const QList<int> &starts, int length, int current);
    /// Folds CriticMarkup comments away, for the margin to show, but on the cursor's line.
    void setFoldComments(bool fold);
    /// Underlines words of prose `speller` does not know; none when it is null.
    void setSpeller(const Sonnet::Speller *speller);
    /// Where the misspelled word at `position` is, `{start, end}` in the document, or -1s.
    [[nodiscard]] std::pair<int, int> misspelledAt(int position) const;

    [[nodiscard]] Hit hitAt(int position) const;
    /// Redraws each table of contents, as after the headings changed.
    void refreshContents();

    /// Block states, kept so code fences and front matter carry from line to line. A code
    /// line packs a hash of its language and parser state above these two bits, so a change
    /// that reaches the next line makes it highlight again.
    enum State { Normal = 0, InFence = 1, InFrontMatter = 2, InMath = 3 };
    /// A block state without the code's own state packed above it.
    [[nodiscard]] static int kindOf(int state) { return state < 0 ? Normal : state & 3; }

protected:
    void highlightBlock(const QString &text) override;
    /// Styles a block, as highlightBlock() does before focus mode fades it.
    void styleBlock(const QString &text);
    /// The blocks of the paragraph `block` is in: it and the lines next to it with text.
    [[nodiscard]] std::pair<int, int> paragraphOf(int block) const;
    /// Restyles the blocks from `range.first` to `range.second`.
    void rehighlightRange(std::pair<int, int> range);

private:
    void merge(int start, int length, const QTextCharFormat &format);
    void mark(int start, int length);
    void inlineStyles(const QString &text, int from);
    /// The reference link labels the document defines; when they change, every block restyles.
    const QSet<QString> &definedLabels();
    /// Colors an open to-do's markers, such as `@weekly` and `@due 2026-11-02`, each kind its own
    /// way, keeping them as written.
    void taskMarkerStyles(const QString &text);
    /// Styles a block in source mode: the marks colored or faded, but none hidden or resized.
    void sourceBlock(const QString &text, int state, bool first);
    /// Styles CriticMarkup's additions, deletions, substitutions, highlights and comments, and
    /// gives where its comments are, which nothing else styles inside.
    QList<std::pair<int, int>> criticStyles(const QString &text, int from);
    /// Shows the image a line links to, if the line is only that link; says whether it did.
    bool imageLine(const QString &text);
    [[nodiscard]] QSize imageSize(const QString &path);
    /// Makes the character at `bang` exactly as tall as `shown` and folds away the rest of the
    /// line, reserving the room the image at `path` is drawn into.
    void showImage(const QString &text, int bang, const QString &path, QSize shown,
                   bool audio = false, bool video = false);
    /// Makes the character at `at` exactly `height` tall and folds away the rest of the line.
    void reserve(const QString &text, int at, int height);
    /// Shows the note's headings in place of a table of contents marker; says whether it did.
    bool contentsLine(const QString &text);
    /// The first and last lines of the diagram fence `block` is in, or -1 for both when it
    /// is in none; a fence still open to the end of the note is not drawn yet.
    [[nodiscard]] std::pair<int, int> diagramFence(const QTextBlock &block) const;
    /// Draws the diagram a fence opening at this line holds; says whether it did.
    bool diagramLine(const QString &text, int last);
    /// The first and last lines of the display math `block` is in, between `$$` lines, or -1
    /// for both; math still open to the end of the note is not drawn yet.
    [[nodiscard]] std::pair<int, int> mathFence(const QTextBlock &block) const;
    /// Styles a line of display math, drawing it on its first line when the cursor is
    /// elsewhere; says whether the line was one.
    bool mathLine(const QString &text, int state);
    /// Draws `$...$` math in a line of prose off the cursor's line; gives where it is.
    QList<std::pair<int, int>> inlineMathStyles(const QString &text, int from);
    /// The line's Inlines, made if it has nothing else; null when it holds something else.
    Inlines *inlines();

    Look m_look;
    int m_cursorBlock = -1;
    bool m_cursorHere = false;
    bool m_source = false;
    /// The labels the document defines, as labelKey() writes them, read again after a change.
    QSet<QString> m_labels;
    bool m_labelsStale = true;
    bool m_focus = false;
    bool m_foldComments = false;
    /// The text painted after a yank, its ends kept as cursors so they move with edits; null
    /// cursors when there is none.
    QTextCursor m_flashStart;
    QTextCursor m_flashEnd;
    /// Where vi's f and t matches are, in order.
    QList<int> m_matches;
    /// What find found, in order, how long each is, and the index of the one the cursor is on.
    QList<int> m_found;
    int m_foundLength = 0;
    int m_foundCurrent = -1;
    const Sonnet::Speller *m_speller = nullptr;
    /// The first and last blocks of the paragraph the cursor is in, for focus mode.
    std::pair<int, int> m_paragraph{-1, -1};
    std::unique_ptr<CodeHighlighter> m_code;
    Resolver m_resolver;
    QHash<QString, QSize> m_sizes;
    /// Where image links lead, worked out once each: a lookup can ask the whole vault.
    QHash<QString, QString> m_resolved;
};

} // namespace jot
