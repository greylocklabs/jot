#pragma once

#include <QColor>
#include <QList>
#include <QSize>
#include <QString>

namespace jot {

/// The theme a diagram is drawn in.
struct DiagramStyle
{
    QColor background;
    QColor text;
    QColor muted;
    QColor surface;
    QColor surfaceAlt;
    QColor border;
    QColor accent;
    QColor danger;
    /// Fills for timeline sections and chart series, in order; Pele repeats them after the last.
    QList<QColor> series;
    /// Text on a light fill, and on a dark one.
    QColor onLight;
    QColor onDark;
    QString fontFamily;
    QString monoFamily;
    int pixelSize = 15;

    bool operator==(const DiagramStyle &) const = default;
};

/// A diagram drawn as SVG that Qt can show as it is: its colors and fonts written out rather
/// than left to CSS.
struct Diagram
{
    /// The SVG file it was saved to, which lasts as long as Jot runs.
    QString path;
    QSize size;
    /// Why it could not be drawn, such as a mistake in the diagram.
    QString error;

    [[nodiscard]] bool ok() const { return error.isEmpty() && !path.isEmpty(); }
};

/// Whether a code fence's language is one Jot draws as a diagram.
[[nodiscard]] bool isDiagramLanguage(QStringView language);

/// Draws Mermaid text with Pele, in a JavaScript engine made on first use. Call it from the
/// GUI thread only. The same text in the same style is drawn once.
[[nodiscard]] Diagram drawDiagram(const QString &source, const DiagramStyle &style);

} // namespace jot
