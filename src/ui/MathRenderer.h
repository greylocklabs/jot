#pragma once

#include <QColor>
#include <QHash>
#include <QSize>
#include <QString>

namespace jot {

/// Draws LaTeX math with MicroTeX into PNG files in folder(), at the screen's pixel ratio, so
/// each pixel drawn is one on the screen and the math stays sharp.
class MathRenderer
{
public:
    /// What drawing a formula made: the file, the size it is shown at, and how far its
    /// baseline is from its top; or why it could not be drawn.
    struct Drawn
    {
        QString path;
        QSize size;
        int baseline = 0;
        QString error;
        [[nodiscard]] bool ok() const { return error.isEmpty() && !path.isEmpty(); }
    };

    static MathRenderer &instance();
    /// The pixel ratio of the window `document` is shown in, or of the screen with the most
    /// pixels when it is in none yet.
    [[nodiscard]] static qreal pixelRatioOf(const QObject *document);
    /// Where drawings are kept; only files here are drawn math.
    static QString folder();

    /// `latex` drawn `pixelSize` tall per em in `color`, for a screen of `ratio` pixels to each
    /// one of the size it is shown at; `display` for math set apart on its own lines, which is
    /// drawn larger and centered on nothing.
    Drawn draw(const QString &latex, bool display, int pixelSize, const QColor &color,
               qreal ratio = 2);
    /// The pixel ratio the drawing at `path` was made for, 2 when it is not one of these.
    [[nodiscard]] qreal ratioOf(const QString &path) const { return m_ratios.value(path, 2); }

private:
    MathRenderer() = default;
    /// Unpacks MicroTeX's fonts into the cache and starts it, once; false if that failed.
    bool start();

    bool m_tried = false;
    bool m_started = false;
    QHash<QString, Drawn> m_drawn;
    QHash<QString, qreal> m_ratios;
};

} // namespace jot
