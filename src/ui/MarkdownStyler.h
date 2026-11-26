#pragma once

#include "jot/FrontMatter.h"
#include "jot/NoteTypes.h"

#include <QObject>
#include <QPointer>
#include <QQmlEngine>
#include <QQuickTextDocument>
#include <QVariantList>
#include <QVariantMap>

#include <memory>
#include <tuple>

class QTimer;

namespace Sonnet {
class Speller;
}

namespace jot {

class MarkdownHighlighter;

/// Puts Markdown styling on a TextEdit's document, in the theme's colors and type, and
/// says what a click lands on. Set `document` to the TextEdit's textDocument and keep
/// `cursorPosition` bound to its cursor.
class MarkdownStyler : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QQuickTextDocument *document READ document WRITE setDocument NOTIFY documentChanged)
    Q_PROPERTY(
        int cursorPosition READ cursorPosition WRITE setCursorPosition NOTIFY cursorPositionChanged)
    /// The editor's body font, which images on their own lines are measured in.
    Q_PROPERTY(QString fontFamily READ fontFamily WRITE setFontFamily NOTIFY fontFamilyChanged)
    /// The editor's body text size, which headings are scaled from.
    Q_PROPERTY(int pixelSize READ pixelSize WRITE setPixelSize NOTIFY pixelSizeChanged)
    /// The screen's pixels to each one of the note's, which math is drawn at; the window's own
    /// until it is given.
    Q_PROPERTY(qreal pixelRatio READ pixelRatio WRITE setPixelRatio NOTIFY pixelRatioChanged)
    /// The width of the text, which images shown inline fit within.
    Q_PROPERTY(int width READ width WRITE setWidth NOTIFY widthChanged)
    /// The images shown in place of their links, as `{position, source, width, height, audio,
    /// video, inline}`, where `position` is where the link's line starts, `audio` and `video` say
    /// it is a sound or a video to play, and `inline` that it is math drawn within a line, at
    /// `position`.
    Q_PROPERTY(QVariantList images READ images NOTIFY imagesChanged)
    /// The tables of contents shown in place of `${toc}` lines, as `{position, rowHeight,
    /// headings}`, each heading `{text, level, position}` with where its line starts and its
    /// level from 1 for the note's highest. Told with the images, which they make room among.
    Q_PROPERTY(QVariantList contents READ contents NOTIFY imagesChanged)
    /// The note's `#` headings, as `{text, level, position}` like a table of contents has
    /// them, for the outline.
    Q_PROPERTY(QVariantList headings READ headings NOTIFY imagesChanged)
    /// The headings folded away, as headingKeys() names them. Source mode shows everything.
    Q_PROPERTY(QStringList folded READ folded WRITE setFolded NOTIFY foldsChanged)
    /// The headings that have something under them to fold, as `{key, position, end, folded}`:
    /// the name to fold it by, where its line starts and ends, and whether it is folded.
    Q_PROPERTY(QVariantList folds READ folds NOTIFY foldsChanged)
    /// The note's words and characters, front matter left out, as `{words, characters}`.
    Q_PROPERTY(QVariantMap count READ count NOTIFY imagesChanged)
    /// What Tab indents a line with, and Shift+Tab takes off: a tab or spaces.
    Q_PROPERTY(QString indentUnit MEMBER m_indentUnit NOTIFY indentUnitChanged)
    /// The Markdown shown as written, without live preview.
    Q_PROPERTY(bool sourceMode READ sourceMode WRITE setSourceMode NOTIFY sourceModeChanged)
    /// Everything but the paragraph the cursor is in faded.
    Q_PROPERTY(bool focusMode READ focusMode WRITE setFocusMode NOTIFY focusModeChanged)
    /// CriticMarkup comments folded away for the margin to show, but on the cursor's line.
    Q_PROPERTY(bool foldComments READ foldComments WRITE setFoldComments NOTIFY foldCommentsChanged)
    /// The CriticMarkup comments, as `{position, text}` with where each `{>>` is.
    Q_PROPERTY(QVariantList comments READ comments NOTIFY imagesChanged)
    /// Words the dictionary does not know are underlined.
    Q_PROPERTY(bool spellcheck READ spellcheck WRITE setSpellcheck NOTIFY spellingChanged)
    /// The dictionary's language, such as "en_US"; the desktop's when empty.
    Q_PROPERTY(QString language READ language WRITE setLanguage NOTIFY spellingChanged)
    /// The note's front matter keys, as the text has them now.
    Q_PROPERTY(QVariantMap frontMatter READ frontMatter NOTIFY frontMatterChanged)
    /// Where the closing fence of the front matter starts, while the cursor is off its line;
    /// -1 otherwise. Styled mode draws it at the top of the room the line keeps.
    Q_PROPERTY(int closingFence READ closingFence NOTIFY closingFenceChanged)
    /// That fence as written, `---` or `...`.
    Q_PROPERTY(QString closingFenceMark READ closingFenceMark NOTIFY closingFenceChanged)
    /// What is wrong with the front matter, each as `{line, message}`, the line counted from 1.
    Q_PROPERTY(QVariantList frontMatterProblems READ frontMatterProblems NOTIFY frontMatterChanged)
    /// The note's properties for the panel that shows them, as propertyRows() orders them: each
    /// `{key, kind, typed, present, list, choices, linkType, value, problem}`. `kind` is a
    /// property's kind, "type", or for a key the type does not name "text", "list" or "nested";
    /// `value` is text, a list's items joined with commas and a link without its brackets.
    Q_PROPERTY(QVariantList properties READ properties NOTIFY frontMatterChanged)
    /// Where the note's body starts, after its front matter, or 0 without any.
    Q_PROPERTY(int bodyStart READ bodyStart NOTIFY frontMatterChanged)
    /// Hides the front matter's lines, which the properties show instead.
    Q_PROPERTY(bool hideFrontMatter READ hideFrontMatter WRITE setHideFrontMatter NOTIFY
                   hideFrontMatterChanged)

public:
    explicit MarkdownStyler(QObject *parent = nullptr);
    ~MarkdownStyler() override;

    [[nodiscard]] QQuickTextDocument *document() const { return m_document; }
    void setDocument(QQuickTextDocument *document);
    [[nodiscard]] int cursorPosition() const { return m_cursorPosition; }
    void setCursorPosition(int position);
    [[nodiscard]] QString fontFamily() const { return m_fontFamily; }
    void setFontFamily(const QString &family);
    [[nodiscard]] int pixelSize() const { return m_pixelSize; }
    void setPixelSize(int size);
    [[nodiscard]] qreal pixelRatio() const;
    void setPixelRatio(qreal ratio);
    /// `at` on a whole pixel of the screen at pixelRatio(), so a picture drawn at it is shown
    /// pixel for pixel.
    Q_INVOKABLE qreal onPixel(qreal at) const;
    [[nodiscard]] int width() const { return m_width; }
    void setWidth(int width);
    [[nodiscard]] QVariantList images() const;
    [[nodiscard]] QVariantList contents() const;
    [[nodiscard]] QVariantList headings() const;
    /// Which of `headings`, as headings() gives them, `position` is under: the last starting at
    /// or before it, or -1.
    Q_INVOKABLE static int headingAt(const QVariantList &headings, int position);
    [[nodiscard]] QVariantList properties() const { return m_properties; }
    [[nodiscard]] int bodyStart() const { return m_bodyStart; }
    [[nodiscard]] bool hideFrontMatter() const { return m_hideFrontMatter; }
    void setHideFrontMatter(bool hide);
    [[nodiscard]] QStringList folded() const { return m_folded; }
    void setFolded(const QStringList &folded);
    [[nodiscard]] QVariantList folds() const { return m_folds; }
    /// The heading whose folded section `position` is in, by its key, or empty.
    Q_INVOKABLE QString foldAt(int position) const;
    /// The line of the text, from 1, that `position` is on.
    Q_INVOKABLE int lineAtPosition(int position) const;
    /// Where the text's `line`, from 1, starts, or its end past its last line.
    Q_INVOKABLE int positionOfLine(int line) const;
    /// Whether `position` is folded away, for what is drawn over the note there.
    Q_INVOKABLE bool isFolded(int position) const { return !foldAt(position).isEmpty(); }
    [[nodiscard]] QVariantMap count() const;
    /// The words and characters in `text`, as `count` has them.
    Q_INVOKABLE static QVariantMap countOf(const QString &text);
    [[nodiscard]] QVariantMap frontMatter() const { return m_frontMatter; }
    [[nodiscard]] QVariantList frontMatterProblems() const { return m_frontMatterProblems; }
    [[nodiscard]] bool sourceMode() const { return m_sourceMode; }
    void setSourceMode(bool source);
    [[nodiscard]] bool focusMode() const { return m_focusMode; }
    void setFocusMode(bool focus);
    [[nodiscard]] bool foldComments() const { return m_foldComments; }
    void setFoldComments(bool fold);
    [[nodiscard]] QVariantList comments() const;
    [[nodiscard]] bool spellcheck() const { return m_spellcheck; }
    void setSpellcheck(bool on);
    [[nodiscard]] QString language() const { return m_language; }
    void setLanguage(const QString &language);

    /// The misspelled word at `position`: `{start, end, word, suggestions}`, or empty.
    Q_INVOKABLE QVariantMap misspelledAt(int position) const;
    /// Takes `word` as spelled right: for good, in the desktop's own dictionary, or `forNow`,
    /// until Jot closes.
    Q_INVOKABLE void acceptWord(const QString &word, bool forNow);

    /// What is at `position`: `{kind, target, position}`, where kind is "", "link", "url" or
    /// "task"; for a task, `position` is where its mark is.
    Q_INVOKABLE QVariantMap hitAt(int position) const;

    /// Formats the text from `from` to `to` as one step to undo, and gives the selection
    /// after as `{start, end}`. `action` is bold, italic, strike, code, link, codeblock,
    /// bullet, numbered, task, quote, h1, h2 or h3. In tables and lists, newline, indent and
    /// outdent do what Return, Tab and Shift+Tab do there, and give nothing elsewhere so the
    /// key does its usual. table inserts a table, or lines up the one at the cursor. linkTarget
    /// puts the target in `argument`, "target\nwords", in the link being typed, closes it, and
    /// gives it the words when it has none.
    Q_INVOKABLE QVariantMap format(const QString &action, int from, int to,
                                   const QString &argument = {});

    /// The link target being typed at `position`: `{start, text}`, where `start` is just after
    /// its `](`. Empty when there is none.
    Q_INVOKABLE QVariantMap linkTargetAt(int position) const;

    /// The emoji name being typed at `position`: `{start, text}`, where `start` is its colon.
    /// Empty when there is none.
    Q_INVOKABLE QVariantMap emojiAt(int position) const;

    /// The to-do marker being typed at `position`: `{start, text}`, where `start` is its @ and
    /// `text` the letters after it. Empty when there is none.
    Q_INVOKABLE QVariantMap markerAt(int position) const;
    /// The markers offered for `typed`, the letters after an @, as of today: `{text, shown,
    /// detail}` for each.
    Q_INVOKABLE static QVariantList markerSuggestions(const QString &typed);
    /// What is being typed in closed front matter at `position`: `{kind, key, text, start}`, where
    /// `kind` is "key" for a key at the start of a line, or "value" for a key's value, or a list
    /// item under it, and `text` runs from `start` to `position`. Empty elsewhere.
    Q_INVOKABLE QVariantMap frontMatterAt(int position) const;
    /// The dictionaries installed, as `lang` names them.
    Q_INVOKABLE QStringList languages() const;

    /// Puts `text` in place of what is from `start` to `end`, as one step to undo.
    Q_INVOKABLE void replace(int start, int end, const QString &text);
    /// The line, from 0, `position` is on.
    Q_INVOKABLE int lineAt(int position) const;
    [[nodiscard]] int closingFence() const { return m_closingFence; }
    [[nodiscard]] QString closingFenceMark() const { return m_closingFenceMark; }
    /// Where `line`, from 0, starts; the end of the text past the last line.
    Q_INVOKABLE int lineStart(int line) const;
    /// Writes the days said in words on the to-do on `line`, from 0, as dates, and ticks one
    /// said done, as tidiedTaskLine() does; says whether it changed anything.
    Q_INVOKABLE bool tidyTaskLine(int line);
    /// Paints the text from `start` to `end` in the selection's color for half a second, as
    /// vi does to what it yanked.
    Q_INVOKABLE void flash(int start, int end);
    /// Marks the characters at `positions`, as vi's f and t matches.
    Q_INVOKABLE void setMatches(const QVariantList &positions);
    /// Where `needle` is in the note, as each match's start, as findAll() finds it.
    Q_INVOKABLE QVariantList findAll(const QString &needle, bool matchCase, bool wholeWords) const;
    /// Paints the matches find found, `length` long from each of `starts`, the one at index
    /// `current` more strongly; none when `starts` is empty.
    Q_INVOKABLE void setFound(const QVariantList &starts, int length, int current);
    /// Puts `text` in place of each match, `length` long from each of `starts`, as one step to
    /// undo. Gives how many it replaced.
    Q_INVOKABLE int replaceAll(const QVariantList &starts, int length, const QString &text);

Q_SIGNALS:
    void documentChanged();
    void cursorPositionChanged();
    void fontFamilyChanged();
    void pixelSizeChanged();
    void pixelRatioChanged();
    void widthChanged();
    void imagesChanged();
    void frontMatterChanged();
    void hideFrontMatterChanged();
    void closingFenceChanged();
    void sourceModeChanged();
    void focusModeChanged();
    void foldCommentsChanged();
    void foldsChanged();
    void indentUnitChanged();
    void spellingChanged();

private:
    void applyLook();
    /// Images can come and go with any edit or move of the cursor; the list is told once
    /// the event loop is back, after the highlighter has had its turn.
    void scheduleImages();
    void readFrontMatter();
    /// Hides the lines of the folded sections, and shows the rest, as the text now stands.
    void applyFolds();

    QPointer<QQuickTextDocument> m_document;
    MarkdownHighlighter *m_highlighter = nullptr;
    int m_cursorPosition = 0;
    QString m_fontFamily;
    int m_pixelSize = 16;
    qreal m_pixelRatio = 0;
    int m_width = 600;
    bool m_imagesPending = false;
    QVariantMap m_frontMatter;
    int m_closingFence = -1;
    QString m_closingFenceMark;
    /// Finds the closing fence again, as the text or the cursor moved.
    void findClosingFence();
    /// Has the TextEdit draw all of the text again, as its own updates miss lines that moved.
    void redrawAll();
    /// A property row as the panel takes it.
    [[nodiscard]] QVariantMap propertyMap(const PropertyRow &row) const;
    QVariantList m_frontMatterProblems;
    /// The note's text up to its body when its type was last checked, and what was found.
    QString m_typeCheckedHead;
    QList<FrontMatterProblem> m_typeProblems;
    /// The headings the tables of contents were last drawn with.
    QList<std::pair<int, QString>> m_headings;
    bool m_sourceMode = false;
    QVariantList m_properties;
    int m_bodyStart = 0;
    bool m_hideFrontMatter = false;
    bool m_focusMode = false;
    bool m_foldComments = false;
    QStringList m_folded;
    QVariantList m_folds;
    /// Each folded section, as `{key, start, end}` positions in the document.
    QList<std::tuple<QString, int, int>> m_foldRanges;
    QString m_indentUnit = QStringLiteral("\t");
    bool m_spellcheck = false;
    QString m_language;
    std::unique_ptr<Sonnet::Speller> m_speller;
    /// Hands the highlighter the speller, or none, as the settings say.
    void applySpelling();
    /// Widths settle before images are fitted again, so a window being resized does
    /// not restyle the note on every frame.
    QTimer *m_widthSettle = nullptr;
    /// Ends a yank's flash.
    QTimer *m_flashEnd = nullptr;
};

} // namespace jot
