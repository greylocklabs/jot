#pragma once

#include "greylock/ThemeSpec.h"

#include <QColor>

namespace jot {

/// The colors code is highlighted in, one for each kind of word.
struct CodeColors
{
    QColor keyword, string, number, type, comment;
};

/// A theme's code colors: its `[editor]` colors, and for any it leaves out, its accent
/// turned around the hue circle at the accent's own lightness, so code takes on the
/// theme's look. Comments are the faint text color.
[[nodiscard]] CodeColors codeColors(const greylock::ThemeSpec &theme);

} // namespace jot
