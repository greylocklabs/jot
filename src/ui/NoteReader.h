#pragma once

#include <QList>
#include <QObject>
#include <QPointer>
#include <QQmlEngine>
#include <QQuickTextDocument>
#include <QVariantMap>

namespace jot {

/// Draws a note for reading in a read-only TextEdit's document: its Markdown rendered by Qt's
/// md4c importer, in the theme's colors and type, with tags as links and
/// images from the vault. Set `document` to the TextEdit's textDocument.
class NoteReader : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QQuickTextDocument *document READ document WRITE setDocument NOTIFY changed)
    /// The note as stored, front matter and all.
    Q_PROPERTY(QString text READ text WRITE setText NOTIFY changed)
    /// The body text's font and size, which headings are scaled from.
    Q_PROPERTY(QString fontFamily READ fontFamily WRITE setFontFamily NOTIFY changed)
    Q_PROPERTY(int pixelSize READ pixelSize WRITE setPixelSize NOTIFY changed)
    /// The screen's pixels to each one of the note's, which math is drawn at; the window's own
    /// until it is given.
    Q_PROPERTY(qreal pixelRatio READ pixelRatio WRITE setPixelRatio NOTIFY changed)
    /// The width of the text, which images fit within.
    Q_PROPERTY(int width READ width WRITE setWidth NOTIFY changed)
    /// The sounds and videos embedded on lines of their own, each `{position, source, width,
    /// height, video}`: where in the document the room kept for its player is, and the file.
    Q_PROPERTY(QVariantList sounds READ sounds NOTIFY soundsChanged)
    /// Where each run of code lines is, `{start, end}` as positions in the document, for the
    /// view to draw the raised surface behind them, `pad` around the text.
    Q_PROPERTY(QVariantList codeRuns READ codeRuns NOTIFY codeRunsChanged)
    Q_PROPERTY(qreal codePadding READ codePadding NOTIFY changed)
    /// Where each quote is, `{start, end, level}` for every level it nests to, for the view to
    /// draw a bar down its left; each level's text starts `quoteIndent` further in.
    Q_PROPERTY(QVariantList quoteRuns READ quoteRuns NOTIFY quoteRunsChanged)
    Q_PROPERTY(qreal quoteIndent READ quoteIndent CONSTANT)
    /// The headings folded away, as headingKeys() names them, which are left out of the drawing.
    Q_PROPERTY(QStringList folded READ folded WRITE setFolded NOTIFY foldsChanged)
    /// The drawn headings that have something under them to fold, as `{key, position, end,
    /// folded}`: the name to fold it by, where its line starts and ends, and whether it is.
    Q_PROPERTY(QVariantList folds READ folds NOTIFY foldsChanged)
    /// What a search found, `{start, end, current}` as positions in the document, for the view
    /// to mark behind the text, the one it is on more strongly.
    Q_PROPERTY(QVariantList found READ found NOTIFY foundChanged)

public:
    explicit NoteReader(QObject *parent = nullptr);

    [[nodiscard]] QQuickTextDocument *document() const { return m_document; }
    void setDocument(QQuickTextDocument *document);
    [[nodiscard]] QString text() const { return m_text; }
    void setText(const QString &text);
    [[nodiscard]] QString fontFamily() const { return m_fontFamily; }
    void setFontFamily(const QString &family);
    [[nodiscard]] int pixelSize() const { return m_pixelSize; }
    [[nodiscard]] qreal pixelRatio() const;
    void setPixelRatio(qreal ratio);
    void setPixelSize(int size);
    [[nodiscard]] int width() const { return m_width; }
    void setWidth(int width);
    [[nodiscard]] QVariantList sounds() const { return m_sounds; }
    [[nodiscard]] QVariantList codeRuns() const { return m_codeRuns; }
    [[nodiscard]] qreal codePadding() const { return m_pixelSize * 0.6; }
    [[nodiscard]] QVariantList quoteRuns() const { return m_quoteRuns; }
    [[nodiscard]] static qreal quoteIndent();
    [[nodiscard]] QVariantList found() const { return m_found; }
    [[nodiscard]] QStringList folded() const { return m_folded; }
    void setFolded(const QStringList &folded);
    [[nodiscard]] QVariantList folds() const { return m_folds; }

    /// Where `needle` is in the note as drawn, as each match's start, as the find bar asks.
    Q_INVOKABLE QVariantList findAll(const QString &needle, bool matchCase, bool wholeWords) const;
    /// Marks the matches at `starts`, each `length` long, the one at `current` more strongly.
    Q_INVOKABLE void setFound(const QVariantList &starts, int length, int current);

    /// The checkbox task drawn at `position`: `{line, done, text, textStart}`, with its line in
    /// the note's file and where its text starts after the box, or empty when there is none there.
    Q_INVOKABLE QVariantMap taskAt(int position) const;
    /// Puts the text from `start` to `end` on the clipboard, without the characters that hold
    /// the place of pictures and task boxes.
    Q_INVOKABLE void copy(int start, int end) const;
    /// Where the heading numbered `index`, from 0, as headingsOf() numbers them, starts in the
    /// drawn document, or -1.
    Q_INVOKABLE int headingPosition(int index) const;
    /// Where the note's line `line`, from 1, is drawn, or the nearest line before it that is;
    /// for going from writing to reading on the same line.
    Q_INVOKABLE int positionOfLine(int line) const;
    /// The note's line, from 1, drawn at `position`, or the nearest before it; for going from
    /// reading back to writing on the same line.
    Q_INVOKABLE int lineAtPosition(int position) const;

Q_SIGNALS:
    /// The note was drawn again, so places in it are where they will stay.
    void rendered();
    void changed();
    void soundsChanged();
    void codeRunsChanged();
    void quoteRunsChanged();
    void foundChanged();
    void foldsChanged();

private:
    void schedule();
    void render();
    void style();
    void drawTasks();
    void placeImages();
    void placeSounds();

    QPointer<QQuickTextDocument> m_document;
    QString m_text;
    QString m_fontFamily;
    int m_pixelSize = 15;
    qreal m_pixelRatio = 0;
    int m_width = 600;
    bool m_pending = false;
    /// The note's checkbox tasks as the scanner reads them, and the blocks they were drawn in,
    /// in the same order; the blocks are empty when the two do not line up.
    struct Task
    {
        int line = 0;
        QString text;
    };
    QList<Task> m_tasks;
    QList<int> m_taskBlocks;
    QVariantList m_sounds;
    QVariantList m_codeRuns;
    QVariantList m_quoteRuns;
    QVariantList m_found;
    QStringList m_folded;
    /// The note's lines found in what is drawn, as {line, position}, in order of both.
    QList<std::pair<int, int>> m_lines;
    /// Finds the note's lines in what is drawn, by their words.
    void findLines(QStringView body, int firstLine, const QList<std::pair<int, int>> &cutLines);
    /// Folded list items Read found no drawn item for, so shows unfolded.
    QStringList m_unplaced;
    QVariantList m_folds;
    /// The body as drawn, folded sections left out, and for each of its headings as
    /// headingsOf() finds them in the whole body, whether it was left out with one.
    QString m_shown;
    QList<bool> m_hiddenHeadings;
};

} // namespace jot
