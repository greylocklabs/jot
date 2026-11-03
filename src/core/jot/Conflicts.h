#pragma once

#include <QDateTime>
#include <QString>
#include <QStringList>

#include <optional>

namespace jot {

/// A copy of a file kept beside it when two versions could not be merged.
struct ConflictCopy
{
    /// The file it is another version of, from the vault's root.
    QString original;
    /// The computer it came from, when its name says; else empty.
    QString from;
    /// When it was kept, when its name says; else invalid.
    QDateTime when;
};

/// What the name of the file at `path` says, when it is a conflict copy: Jot's own,
/// `<name> (conflict [<computer>] <yyyy-MM-dd HHmm>)[ <n>].<extension>`, or one left by the app
/// that syncs the folder: Syncthing's `<name>.sync-conflict-<yyyyMMdd>-<HHmmss>-<device>`,
/// Nextcloud's `<name> (conflicted copy [<user>] <yyyy-MM-dd HHmmss>)`, ownCloud's
/// `<name>_conflict-<yyyyMMdd>-<HHmmss>` and Dropbox's `<name> (<computer>'s conflicted copy
/// <yyyy-MM-dd>)`, each with the file's extension after.
[[nodiscard]] std::optional<ConflictCopy> readConflictCopy(const QString &path);

/// Where to keep the other version of the file at `path`, from `device` at `when`, in the vault
/// at `root`: beside it, named as readConflictCopy() reads it, and free.
[[nodiscard]] QString conflictCopyPath(const QString &root, const QString &path,
                                       const QString &device, const QDateTime &when);

/// The conflict copies of the file at `path` among `paths`, oldest first.
[[nodiscard]] QStringList conflictCopiesOf(const QString &path, const QStringList &paths);

} // namespace jot
