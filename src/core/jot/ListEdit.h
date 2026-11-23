#pragma once

#include <QString>

namespace jot {

/// `body` with the ticked to-dos of each list moved to its end, in their order, each with the
/// lines indented under it; nested lists are done the same way. Numbered lists are numbered
/// again from their first number.
[[nodiscard]] QString moveTickedToEnd(const QString &body);

/// `body` without its ticked to-dos, each going with the lines indented under it.
[[nodiscard]] QString removeTicked(const QString &body);

/// `body` with every to-do unticked.
[[nodiscard]] QString untickAll(const QString &body);

/// `body` with the innermost list holding `position` sorted by its items' words, A to Z in any
/// case, each with the lines under it; `body` as it is when `position` is in no list.
[[nodiscard]] QString sortList(const QString &body, int position);

} // namespace jot
