#pragma once

#include "jot/Vault.h"
#include "jot/VaultIndex.h"

#include <QStringList>

#include <functional>
#include <optional>

namespace jot {

/// Called with each note about to be written, so a watcher can expect the write.
using BeforeWrite = std::function<void(const QString &path, const QString &text)>;

/// Moves the note at `from` to `to`, with its attachments, and rewrites the links in other
/// notes that reached it so they still do, in the style each was written in. Gives the
/// notes it rewrote. The index is brought up to date with all of them.
std::optional<QStringList> renameNote(const Vault &vault, VaultIndex &index, const QString &from,
                                      const QString &to, QString *error,
                                      const BeforeWrite &beforeWrite = {});

/// Moves the notebook at `from` to `to`, with every note and attachment in it, and rewrites
/// links so they still reach the same notes: links from other notes to notes inside it, and
/// the Markdown paths in its notes that lead out of it. Gives the notes it rewrote, by their
/// paths after the move. The index is brought up to date with all of them.
std::optional<QStringList> moveNotebook(const Vault &vault, VaultIndex &index, const QString &from,
                                        const QString &to, QString *error,
                                        const BeforeWrite &beforeWrite = {});

/// The target to write for a link that reached `from` from `source` and should now reach `to`:
/// a path from the source's folder, or from the vault's root when `raw` started with `/`, with
/// `.md` only when `raw` had it. Any `#heading` stays.
[[nodiscard]] QString retarget(const QString &raw, const QString &source, const QString &to);

} // namespace jot
