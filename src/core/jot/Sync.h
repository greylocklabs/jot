#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QList>
#include <QString>
#include <QStringList>

#include <memory>
#include <optional>

namespace jot {

/// A file as a remote holds it.
struct RemoteFile
{
    /// From the vault's root, with `/` between folders.
    QString path;
    /// What the remote calls this version of it; any change to the file changes it.
    QString rev;
    /// The SHA-256 of its bytes, when the remote can say without sending them; else empty.
    QByteArray hash;
};

/// Where a vault syncs to: a folder, a bucket or a cloud drive. Each call blocks until it is
/// done. A write or delete names the version it expects to replace, so two devices writing at
/// once cannot overwrite each other unseen.
class Transport
{
public:
    virtual ~Transport() = default;

    /// Every file the remote holds, hidden ones included.
    virtual std::optional<QList<RemoteFile>> list(QString *error) = 0;
    /// The bytes of the file at `path`, and its `rev`.
    virtual std::optional<QByteArray> get(const QString &path, QString *rev, QString *error) = 0;
    /// Writes `data` at `path` if the remote still holds version `expected` there, or nothing
    /// when `expected` is empty, and gives the new version's rev. Says `stale` instead when the
    /// remote holds another.
    virtual std::optional<QString> put(const QString &path, const QByteArray &data,
                                       const QString &expected, bool *stale, QString *error) = 0;
    /// Removes the file at `path` if the remote still holds version `expected`, keeping it in
    /// the remote's trash where it has one. Says `stale` instead when the remote holds another.
    virtual bool remove(const QString &path, const QString &expected, bool *stale,
                        QString *error) = 0;
};

/// What a sync did, path by path.
struct SyncReport
{
    /// Changed here from the remote, deleted here included.
    QStringList pulled;
    /// Sent to the remote, deleted there included.
    QStringList pushed;
    /// Changed on both sides and merged line by line.
    QStringList merged;
    /// Changed on both sides where a merge could not be made: the other version is kept beside
    /// the file as a conflict copy, whose path is given here.
    QStringList conflicts;
    /// What went wrong, file by file, to be tried again next time.
    QStringList errors;
    /// The remote could not be reached or read, so nothing was done.
    QString failure;

    [[nodiscard]] bool changedHere() const
    {
        return !pulled.isEmpty() || !merged.isEmpty() || !conflicts.isEmpty();
    }
};

/// Syncs a vault folder with one remote, as Unison does: it keeps what both sides held after the
/// last sync, the base, and decides each file three ways. A change on one side goes to the
/// other. A change on both is merged line by line for text, front matter key by key, and
/// otherwise kept as a conflict copy beside the file. A delete never wins over an edit, and
/// what a sync deletes here goes to the vault's `.jot/trash/`, so nothing is lost.
class SyncEngine
{
public:
    /// Syncs the vault at `root` with `remote`, keeping its base in `stateDir`, a folder of its
    /// own outside the vault for this vault and remote. `device` names this computer in
    /// conflict copies.
    SyncEngine(QString root, std::unique_ptr<Transport> remote, QString stateDir, QString device);
    ~SyncEngine();

    SyncEngine(const SyncEngine &) = delete;
    SyncEngine &operator=(const SyncEngine &) = delete;

    /// Brings the vault and the remote level. Safe to stop at any point: each file's base is
    /// written as soon as that file is done, and the next sync carries on.
    SyncReport sync(const QDateTime &now = QDateTime::currentDateTime());

    /// Whether `path`, from the vault's root, is synced: notes, attachments and `.jot/` are;
    /// other hidden files and `.jot/trash/` are not.
    [[nodiscard]] static bool synced(const QString &path);
    /// The file at a remote's top that says which remote it is, so an unmounted drive or another
    /// remote is never taken for an emptied one. Hidden, so it never syncs into a vault.
    static constexpr QStringView kMarker = u".jot-remote";
    /// Whether a file at `path` is merged line by line when both sides changed it.
    [[nodiscard]] static bool mergeable(const QString &path);

    /// `mine` and `theirs`, two edits of a note from `base`, merged: front matter key by key
    /// and the body line by line. Nothing when they changed the same thing differently.
    [[nodiscard]] static std::optional<QString> mergeNote(const QString &base, const QString &mine,
                                                          const QString &theirs);

private:
    struct Private;
    std::unique_ptr<Private> d;
};

/// The remote a sync setting names, written `kind:where`, such as `folder:/mnt/nas/Notes`; or
/// nothing, with `error` saying why, for one Jot does not know.
[[nodiscard]] std::unique_ptr<Transport> transportFor(const QString &remote, QString *error);
/// Where a remote is, in words for the app: a folder's path.
[[nodiscard]] QString remoteLabel(const QString &remote);
/// The folder that keeps the base of the vault at `vaultRoot` with `remote`: one for each pair,
/// in this computer's state folder, never in the vault.
[[nodiscard]] QString syncStateDir(const QString &vaultRoot, const QString &remote);

} // namespace jot
