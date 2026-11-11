#pragma once

#include <QList>
#include <QStringView>

namespace jot {

/// Where `needle` is in `text`, as the start of each match, left to right and never
/// overlapping. Unless `matchCase`, case aside; with `wholeWords`, only where no letter, digit or
/// underscore touches it on either side. Nothing for an empty needle.
[[nodiscard]] QList<qsizetype> findAll(QStringView text, QStringView needle, bool matchCase,
                                       bool wholeWords);

} // namespace jot
