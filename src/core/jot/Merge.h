#pragma once

#include <QString>

#include <optional>

namespace jot {

/// Merges two edits of the same text line by line, as version control does: `base` is what
/// both started from, `mine` and `theirs` what each made of it. Changes to different lines both
/// come through, and the same change made on both sides comes through once. Nothing when the
/// two changed the same lines, or lines next to each other, differently, or when the texts are
/// too long and different to compare quickly.
[[nodiscard]] std::optional<QString> mergeText(const QString &base, const QString &mine,
                                               const QString &theirs);

} // namespace jot
