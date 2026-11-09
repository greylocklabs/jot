#include "MathRenderer.h"

#include "jot/Logging.h"

#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QRegularExpression>
#include <QStandardPaths>

#include <latex.h>
#include <platform/qt/graphic_qt.h>

using namespace Qt::StringLiterals;

namespace jot {

namespace {

/// MicroTeX's Qt drawing, with each glyph drawn as its outline: MicroTeX sets its fonts a unit
/// tall and scales the painter up, which glyphs drawn as text do not survive.
class OutlineGraphics : public tex::Graphics2D_qt
{
public:
    using Graphics2D_qt::Graphics2D_qt;

    void drawChar(wchar_t c, float x, float y) override { drawText(std::wstring(1, c), x, y); }

    void drawText(const std::wstring &text, float x, float y) override
    {
        const auto *font = dynamic_cast<const tex::Font_qt *>(getFont());
        if (!font)
            return;
        QFont outline = font->getQFont();
        outline.setStyleStrategy(QFont::ForceOutline);
        outline.setHintingPreference(QFont::PreferNoHinting);
        // MicroTeX's sizes are in its own units, not points, so the outline is made in pixels,
        // large for its detail, then drawn at the size asked.
        constexpr int large = 100;
        const qreal size = outline.pointSizeF();
        outline.setPixelSize(large);
        QPainterPath path;
        path.addText(0, 0, outline, QString::fromWCharArray(text.data(), qsizetype(text.size())));
        QPainter *painter = getQPainter();
        painter->save();
        painter->translate(x, y);
        painter->scale(size / large, size / large);
        painter->fillPath(path, painter->pen().color());
        painter->restore();
    }
};

} // namespace

MathRenderer &MathRenderer::instance()
{
    static MathRenderer renderer;
    return renderer;
}

bool MathRenderer::start()
{
    if (m_tried)
        return m_started;
    m_tried = true;
    // MicroTeX reads its fonts from files, so they are unpacked from Jot once per version of
    // them, which the folder's name stands for: the commit in third_party/microtex/VENDORED.txt.
    const QString root =
        QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + u"/microtex/0e3707f"_s;
    if (!QFileInfo::exists(root)) {
        // Unpacked beside it, then moved into place whole, so a failure halfway is tried again.
        QDir unpacking(root + u".unpacking"_s);
        unpacking.removeRecursively();
        QDirIterator files(u":/jot/microtex"_s, QDir::Files | QDir::Hidden,
                           QDirIterator::Subdirectories);
        while (files.hasNext()) {
            const QString from = files.next();
            const QString to = unpacking.path() + from.mid(u":/jot/microtex"_s.size());
            QDir().mkpath(QFileInfo(to).path());
            if (!QFile::copy(from, to)) {
                qCWarning(lcVault) << "could not unpack the math fonts to" << to;
                unpacking.removeRecursively();
                m_tried = false;
                return false;
            }
        }
        if (!QDir().rename(unpacking.path(), root) && !QFileInfo::exists(root)) {
            qCWarning(lcVault) << "could not put the math fonts in" << root;
            unpacking.removeRecursively();
            m_tried = false;
            return false;
        }
    }
    try {
        tex::LaTeX::init(root.toStdString());
        m_started = true;
    } catch (const std::exception &e) {
        qCWarning(lcVault) << "could not start drawing math:" << e.what();
    }
    return m_started;
}

QString MathRenderer::folder()
{
    return QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + u"/math"_s;
}

MathRenderer::Drawn MathRenderer::draw(const QString &latex, bool display, int pixelSize,
                                       const QColor &color)
{
    const QString key =
        u"%1|%2|%3|%4"_s.arg(display).arg(pixelSize).arg(color.name(QColor::HexArgb), latex);
    if (const auto found = m_drawn.constFind(key); found != m_drawn.cend())
        return *found;
    Drawn drawn;
    // MicroTeX expands commands a formula defines without end when they call themselves, and
    // keeps them for every formula after, so such formulas stay as written.
    static const QRegularExpression defines(
        uR"(\\(?:re)?new(?:command|environment)\b|\\DeclareMathOperator\b|\\def\b|\\let\b)"_s);
    if (latex.contains(defines)) {
        drawn.error = QObject::tr("Formulas that define commands are not drawn.");
        return drawn;
    }
    if (!start()) {
        drawn.error = QObject::tr("Math cannot be drawn here.");
        return drawn;
    }
    const QString path =
        folder() + u'/' +
        QString::fromLatin1(
            QCryptographicHash::hash(key.toUtf8(), QCryptographicHash::Sha1).toHex()) +
        u"@2x.png"_s;
    try {
        const float size = float(pixelSize) * (display ? 1.25f : 1.f) * 2.f;
        std::unique_ptr<tex::TeXRender> render(tex::LaTeX::parse(
            (display ? u"\\displaystyle "_s + latex : latex).toStdWString(), 4000, size, size / 3.f,
            tex::argb(color.alpha(), color.red(), color.green(), color.blue())));
        const int width = render->getWidth();
        const int height = render->getHeight();
        if (width <= 0 || height <= 0)
            throw std::runtime_error("nothing to draw");
        // Drawn with room around it, as glyphs can reach past the box MicroTeX gives, then cut
        // to what was drawn and that box.
        const int pad = int(size);
        QImage canvas(width + 2 * pad, height + 2 * pad, QImage::Format_ARGB32_Premultiplied);
        canvas.fill(Qt::transparent);
        {
            QPainter painter(&canvas);
            painter.setRenderHint(QPainter::Antialiasing);
            painter.setRenderHint(QPainter::TextAntialiasing);
            OutlineGraphics graphics(&painter);
            render->draw(graphics, pad, pad);
        }
        QRect inked(pad, pad, width, height);
        for (int y = 0; y < canvas.height(); ++y) {
            const auto *line = reinterpret_cast<const QRgb *>(canvas.constScanLine(y));
            for (int x = 0; x < canvas.width(); ++x) {
                if (qAlpha(line[x]) > 0)
                    inked |= QRect(x, y, 1, 1);
            }
        }
        const QImage image = canvas.copy(inked);
        QDir().mkpath(folder());
        if (!QFileInfo::exists(path) && !image.save(path))
            throw std::runtime_error("could not save the drawing");
        drawn.path = path;
        drawn.size = QSize((image.width() + 1) / 2, (image.height() + 1) / 2);
        drawn.baseline = (pad + height - render->getDepth() - inked.top()) / 2;
    } catch (const std::exception &e) {
        drawn.error = QString::fromUtf8(e.what());
    }
    m_drawn.insert(key, drawn);
    return drawn;
}

} // namespace jot
