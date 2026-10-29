#pragma once

#include <QString>

#include <optional>

namespace jot {

/// `text` with a patch from a Joplin revision applied: a JSON list of diff-match-patch patches,
/// each `{"diffs": [[op, text], ...], "start1": n, "start2": n, "length1": n, "length2": n}`,
/// where op is -1 to delete, 0 to keep and 1 to insert. Each patch goes where it says, or at the
/// nearest place its text is found. Nothing when a patch's text is nowhere, or the patch is not
/// one; an empty patch changes nothing.
[[nodiscard]] std::optional<QString> applyJoplinPatch(const QString &text, const QString &patch);

} // namespace jot
