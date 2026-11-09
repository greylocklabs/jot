#pragma once

#include <QColor>
#include <QHash>
#include <QSize>
#include <QString>

namespace jot {

/// Draws LaTeX math with MicroTeX into PNG files in Jot's cache, named `<hash>@2x.png` as they
/// are drawn at twice the size they are shown, to stay sharp on a dense screen.
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

    /// `latex` drawn `pixelSize` tall per em in `color`; `display` for math set apart on its
    /// own lines, which is drawn larger and centered on nothing.
    Drawn draw(const QString &latex, bool display, int pixelSize, const QColor &color);

private:
    MathRenderer() = default;
    /// Unpacks MicroTeX's fonts into the cache and starts it, once; false if that failed.
    bool start();

    bool m_tried = false;
    bool m_started = false;
    QHash<QString, Drawn> m_drawn;
};

} // namespace jot
