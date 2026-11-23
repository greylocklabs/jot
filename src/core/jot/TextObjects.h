#pragma once

#include <QChar>
#include <QString>

#include <optional>
#include <utility>

namespace jot {

/// The stretch of `text` a vi text object covers at `position`, as `[start, end)`, or nothing
/// when there is none there. `kind` is `w` for a word, `W` for a run of anything but space, a
/// quote (`"`, `'` or `` ` ``) for what is quoted on the line, a bracket (`(` `)` `b`, `[` `]`,
/// `{` `}` `B`, `<` `>`) for what the innermost pair around it holds, or `p` for a paragraph.
/// `around` takes in the quotes or brackets, and the space after a word or quoted text, or the
/// blank lines after a paragraph, as vim's `a` does; otherwise only what is inside, as `i` does.
[[nodiscard]] std::optional<std::pair<int, int>> textObject(const QString &text, int position,
                                                            QChar kind, bool around);

} // namespace jot
