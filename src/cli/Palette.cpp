#include "Palette.h"

#include "greylock/Color.h"
#include "greylock/ThemeLoader.h"

#include <algorithm>

namespace jot::cli {

namespace {

/// Lightnesses, in OKLCH, that read at 4.5:1 on a near-white or a near-black
/// page at every hue.
constexpr double kOnLightPage = 0.5;
constexpr double kOnDarkPage = 0.8;

} // namespace

Palette Palette::forTheme(const QString &idOrPath)
{
    if (!idOrPath.isEmpty()) {
        const greylock::ThemeLoadResult loaded = greylock::ThemeLoader::load(idOrPath);
        if (loaded.ok())
            return Palette(loaded.theme);
    }
    return Palette(greylock::ThemeLoader::defaultTheme());
}

QColor Palette::fill(const QColor &calendar) const
{
    const greylock::ThemeSpec::Calendar &c = m_theme.calendar;
    return c.harmonize ? greylock::color::harmonize(calendar, c.lightness, c.chroma) : calendar;
}

QColor Palette::ink(const QColor &calendar) const
{
    const greylock::ThemeSpec::Calendar &c = m_theme.calendar;
    if (c.harmonize)
        return greylock::color::harmonize(calendar, c.inkLightness, c.inkChroma);
    // As the app does for an unfitted color: whichever of black and white reads.
    const QColor black(0, 0, 0), white(255, 255, 255);
    return greylock::color::contrastRatio(black, calendar) >=
                   greylock::color::contrastRatio(white, calendar)
               ? black
               : white;
}

QColor Palette::onPage(const QColor &calendar) const
{
    const greylock::ThemeSpec::Calendar &c = m_theme.calendar;
    if (!c.harmonize) {
        // The calendar's own color when it reads, else a shade of it that does.
        if (greylock::color::contrastRatio(calendar, m_theme.colors.background) >= 4.5)
            return calendar;
        return greylock::color::harmonize(
            calendar, m_theme.dark ? kOnDarkPage : kOnLightPage,
            std::min(greylock::color::toOklch(calendar).chroma, 0.15));
    }
    if (m_theme.dark)
        return fill(calendar);
    return greylock::color::harmonize(calendar, kOnLightPage, std::max(c.chroma, c.edgeChroma));
}

} // namespace jot::cli
