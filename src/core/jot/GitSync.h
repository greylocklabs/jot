#pragma once

#include "jot/Sync.h"

#include <QDateTime>
#include <QStringList>

#include <optional>

namespace jot {

/// Syncs a vault with a git repository through the `git` command, so SSH and HTTPS reach any host
/// as they do from a terminal. Each sync commits what changed here, fetches, merges and pushes,
/// trying again when someone pushed first. Where git's own merge stops, notes are merged as the
/// other remotes merge them, front matter key by key, and failing that the other version is kept
/// beside the file as a conflict copy: no conflict markers are left in a file, and an edit beats
/// a delete. Large files go through Git LFS when it is installed.
class GitSync
{
public:
    /// Syncs the vault at `root` with the repository at `url`, on `branch`. The repository's own
    /// files live in `gitDir`, outside the vault, unless the vault has a `.git` of its own: that
    /// one is used as it is set up, so its origin must be `url`, however either is written, and
    /// `branch` checked out, and its large files go to LFS only as it says.
    /// `device` names this computer in commits and conflict copies.
    GitSync(QString root, QString url, QString branch, QString gitDir, QString device);

    SyncReport sync(const QDateTime &now = QDateTime::currentDateTime());

    /// Whether the `git` command is there to run.
    [[nodiscard]] static bool available();
    /// Whether Git LFS is there too.
    [[nodiscard]] static bool lfsAvailable();
    /// Whether the repository at `url` answers, as with the keys and settings this computer
    /// has; says why not otherwise.
    static bool reachable(const QString &url, QString *error);

    /// A vault's own git repository, as it is set up.
    struct OwnRepository
    {
        bool exists = false;
        /// Its origin's address; empty when it has none.
        QString origin;
        /// The branch checked out; empty when HEAD is detached.
        QString branch;
    };
    /// The git repository the vault at `root` is, if it is one.
    [[nodiscard]] static OwnRepository ownRepository(const QString &root);
    /// Whether two addresses name the same repository: SSH and HTTPS forms, with or without
    /// `.git` or a trailing slash, and the host in any case.
    [[nodiscard]] static bool sameRemote(const QString &a, const QString &b);

    /// Files kept in Git LFS, by their extensions.
    static const QStringList kLargeFiles;
    /// The `.gitattributes` pattern for files with `extension`, in either case.
    [[nodiscard]] static QByteArray largeFilePattern(const QString &extension);
    /// How long a fetch, a push, or a step that may fetch or filter large files, may take.
    static constexpr int kNetworkMs = 5 * 60 * 1000;

private:
    struct Run
    {
        int code = -1;
        QByteArray out;
        QByteArray err;
        [[nodiscard]] bool ok() const { return code == 0; }
    };
    Run git(const QStringList &arguments, int timeoutMs = kNetworkMs) const;
    /// Runs git; `batchSsh` keeps SSH from asking anything.
    static Run command(const QStringList &arguments, const QString &directory, int timeoutMs,
                       bool batchSsh = true);
    /// Whether the person chose how git runs SSH, in the environment or git's settings as
    /// `gitArguments` find them.
    static bool sshChosen(const QStringList &gitArguments);
    bool prepare(QString *error);
    /// Commits what changed here; gives the paths.
    std::optional<QStringList> commit(QString *error);
    /// Merges what was fetched, resolving what git could not; false, with `error`, when it
    /// could not be made.
    bool merge(SyncReport &report, QString *error);
    bool resolve(const QString &path, SyncReport &report, QString *error);
    [[nodiscard]] QString remoteBranch() const;
    /// Whether the repository keeps files in Git LFS, which is not installed; says so if it does.
    bool lfsMissing(QString *error) const;
    bool writeFile(const QString &path, const QByteArray &data, QString *error) const;

    QString m_root;
    QString m_url;
    QString m_branch;
    QString m_gitDir;
    QString m_device;
    QDateTime m_now;
    /// The vault is a repository of its own, which Jot uses as it is.
    bool m_own = false;
    /// Whether to keep SSH from asking anything, found out at the first git run.
    mutable std::optional<bool> m_batchSsh;
};

} // namespace jot
