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
/// `<name> (conflict [<computer>] <yyyy-MM-dd HHmm>)[ <n>].<extension>`.
[[nodiscard]] std::optional<ConflictCopy> readConflictCopy(const QString &path);

/// The conflict copies of the file at `path` among `paths`, oldest first.
[[nodiscard]] QStringList conflictCopiesOf(const QString &path, const QStringList &paths);

} // namespace jot
