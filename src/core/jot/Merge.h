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

/// `after`, which ends its lines with `\n` alone, ending them as `before` did: a line kept from
/// `before` keeps the break it had there, and a new or changed one takes the break most of
/// `before`'s lines have. The same `after` when `before` has no `\r\n`.
[[nodiscard]] QString keepLineEndings(const QString &before, const QString &after);

/// Where `position` in `before` belongs once it has become `after`: moved along by what was
/// put in or taken out ahead of it, and to the end of a change made around it.
[[nodiscard]] int movedPosition(const QString &before, const QString &after, int position);

} // namespace jot
