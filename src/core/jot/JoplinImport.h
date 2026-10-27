#pragma once

#include "jot/JoplinExport.h"

#include <QList>
#include <QString>
#include <QStringList>

#include <optional>

namespace jot {

class Vault;

/// What importing a Joplin export into a vault did, or would do in a dry run.
///
/// Importing again is safe. `.jot/joplin.yaml` records which file each Joplin note became and
/// what it held, so a later import of the same or an overlapping export updates notes Joplin
/// changed, leaves alone ones edited in Jot since, and never brings a second copy.
struct JoplinImport
{
    enum class Outcome {
        /// A new note.
        Created,
        /// Changed in Joplin since the last import, and not in Jot, so brought up to date.
        Updated,
        /// The same as the last import brought.
        Unchanged,
        /// Edited in Jot since the last import, so left as it is.
        ChangedInJot,
        /// Imported before, then removed, or moved and edited, in Jot, so not brought back.
        Removed,
    };
    struct Note
    {
        QString joplinId;
        QString title;
        /// Its vault path, or where it was for one Removed.
        QString path;
        Outcome outcome = Outcome::Created;
    };

    QList<Note> notes;
    /// Notebook folders made, by vault path.
    QStringList notebooks;
    /// Attachments copied into `.assets` folders.
    int attachments = 0;
    /// What was left out, in words: encrypted items, notes in Joplin's trash and the like.
    QStringList skipped;

    [[nodiscard]] int count(Outcome outcome) const;
};

/// Imports `from` into `vault`, under its folder `folder` (empty for the top). Notebooks become
/// folders, merging with folders of the same name; Joplin's `:/id` links become relative links;
/// and each attachment goes into the `.assets` folder of the first note that uses it. With
/// `dryRun` nothing is written. Nothing when the vault cannot be written, with `error` set.
[[nodiscard]] std::optional<JoplinImport> importJoplin(const Vault &vault, const JoplinExport &from,
                                                       const QString &folder, bool dryRun,
                                                       QString *error);

} // namespace jot
