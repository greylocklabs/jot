#pragma once

#include <QObject>
#include <QPointer>
#include <QQmlEngine>
#include <QQuickTextDocument>
#include <QVariantList>
#include <QVariantMap>

class QTimer;

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
    /// The width of the text, which images shown inline fit within.
    Q_PROPERTY(int width READ width WRITE setWidth NOTIFY widthChanged)
    /// The images shown in place of their links, as `{position, source, width, height}`,
    /// where `position` is where the link's line starts.
    Q_PROPERTY(QVariantList images READ images NOTIFY imagesChanged)
    /// The tables of contents shown in place of `${toc}` lines, as `{position, rowHeight,
    /// headings}`, each heading `{text, level, position}` with where its line starts and its
    /// level from 1 for the note's highest. Told with the images, which they make room among.
    Q_PROPERTY(QVariantList contents READ contents NOTIFY imagesChanged)
    /// The note's `#` headings, as `{text, level, position}` like a table of contents has
    /// them, for the outline.
    Q_PROPERTY(QVariantList headings READ headings NOTIFY imagesChanged)
    /// The Markdown shown as written, without live preview.
    Q_PROPERTY(bool sourceMode READ sourceMode WRITE setSourceMode NOTIFY sourceModeChanged)
    /// The note's front matter keys, as the text has them now.
    Q_PROPERTY(QVariantMap frontMatter READ frontMatter NOTIFY frontMatterChanged)

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
    [[nodiscard]] int width() const { return m_width; }
    void setWidth(int width);
    [[nodiscard]] QVariantList images() const;
    [[nodiscard]] QVariantList contents() const;
    [[nodiscard]] QVariantList headings() const;
    [[nodiscard]] QVariantMap frontMatter() const { return m_frontMatter; }
    [[nodiscard]] bool sourceMode() const { return m_sourceMode; }
    void setSourceMode(bool source);

    /// What is at `position`: `{kind, target, position}`, where kind is "", "link",
    /// "wikilink", "url" or "task"; for a task, `position` is where its mark is.
    Q_INVOKABLE QVariantMap hitAt(int position) const;

    /// Formats the text from `from` to `to` as one step to undo, and gives the selection
    /// after as `{start, end}`. `action` is bold, italic, strike, code, link, codeblock,
    /// bullet, numbered, task, quote, h1, h2 or h3, or for the note as a whole taskNote, done,
    /// or due with `argument` a date or empty. In tables and lists, newline, indent and
    /// outdent do what Return, Tab and Shift+Tab do there, and give nothing elsewhere so the
    /// key does its usual. table inserts a table, or lines up the one at the cursor. wikilink
    /// puts `argument` in the wikilink being typed and closes it.
    Q_INVOKABLE QVariantMap format(const QString &action, int from, int to,
                                   const QString &argument = {});

    /// The wikilink being typed at `position`: `{start, text}`, where `start` is just after
    /// its `[[`. Empty when there is none.
    Q_INVOKABLE QVariantMap wikilinkAt(int position) const;

    /// The emoji name being typed at `position`: `{start, text}`, where `start` is its colon.
    /// Empty when there is none.
    Q_INVOKABLE QVariantMap emojiAt(int position) const;

    /// Puts `text` in place of what is from `start` to `end`, as one step to undo.
    Q_INVOKABLE void replace(int start, int end, const QString &text);

Q_SIGNALS:
    void documentChanged();
    void cursorPositionChanged();
    void fontFamilyChanged();
    void pixelSizeChanged();
    void widthChanged();
    void imagesChanged();
    void frontMatterChanged();
    void sourceModeChanged();

private:
    void applyLook();
    /// Images can come and go with any edit or move of the cursor; the list is told once
    /// the event loop is back, after the highlighter has had its turn.
    void scheduleImages();
    void readFrontMatter();

    QPointer<QQuickTextDocument> m_document;
    MarkdownHighlighter *m_highlighter = nullptr;
    int m_cursorPosition = 0;
    QString m_fontFamily;
    int m_pixelSize = 16;
    int m_width = 600;
    bool m_imagesPending = false;
    QVariantMap m_frontMatter;
    /// The headings the tables of contents were last drawn with.
    QList<std::pair<int, QString>> m_headings;
    bool m_sourceMode = false;
    /// Widths settle before images are fitted again, so a window being resized does
    /// not restyle the note on every frame.
    QTimer *m_widthSettle = nullptr;
};

} // namespace jot
