#pragma once

#include <QList>
#include <QObject>
#include <QPointer>
#include <QQmlEngine>
#include <QQuickTextDocument>
#include <QVariantMap>

namespace jot {

/// Draws a note for reading in a read-only TextEdit's document: its Markdown rendered by Qt's
/// md4c importer, in the theme's colors and type, with wikilinks, embeds and tags as links and
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
    /// The width of the text, which images fit within.
    Q_PROPERTY(int width READ width WRITE setWidth NOTIFY changed)
    /// The sounds embedded on lines of their own, each `{position, source, width, height}`: where
    /// in the document the room kept for its player is, and the file it plays.
    Q_PROPERTY(QVariantList sounds READ sounds NOTIFY soundsChanged)
    /// Where each run of code lines is, `{start, end}` as positions in the document, for the
    /// view to draw the raised surface behind them, `pad` around the text.
    Q_PROPERTY(QVariantList codeRuns READ codeRuns NOTIFY codeRunsChanged)
    Q_PROPERTY(qreal codePadding READ codePadding NOTIFY changed)

public:
    explicit NoteReader(QObject *parent = nullptr);

    [[nodiscard]] QQuickTextDocument *document() const { return m_document; }
    void setDocument(QQuickTextDocument *document);
    [[nodiscard]] QString text() const { return m_text; }
    void setText(const QString &text);
    [[nodiscard]] QString fontFamily() const { return m_fontFamily; }
    void setFontFamily(const QString &family);
    [[nodiscard]] int pixelSize() const { return m_pixelSize; }
    void setPixelSize(int size);
    [[nodiscard]] int width() const { return m_width; }
    void setWidth(int width);
    [[nodiscard]] QVariantList sounds() const { return m_sounds; }
    [[nodiscard]] QVariantList codeRuns() const { return m_codeRuns; }
    [[nodiscard]] qreal codePadding() const { return m_pixelSize * 0.6; }

    /// The checkbox task drawn at `position`: `{line, done, text, textStart}`, with its line in
    /// the note's file and where its text starts after the box, or empty when there is none there.
    Q_INVOKABLE QVariantMap taskAt(int position) const;
    /// Puts the text from `start` to `end` on the clipboard, without the characters that hold
    /// the place of pictures and task boxes.
    Q_INVOKABLE void copy(int start, int end) const;
    /// Where the heading numbered `index`, from 0, as headingsOf() numbers them, starts in the
    /// drawn document, or -1.
    Q_INVOKABLE int headingPosition(int index) const;

Q_SIGNALS:
    void changed();
    void soundsChanged();
    void codeRunsChanged();

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
};

} // namespace jot
