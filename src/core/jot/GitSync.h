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
    /// `device` names this computer in commit messages and conflict copies. Commits are made as
    /// git's own user, or as `Jot <jot@localhost>` when git has none, as Options say.
    GitSync(QString root, QString url, QString branch, QString gitDir, QString device);

    /// Commits what changed here, brings in what others pushed and pushes, as Options say and
    /// `request` asks beyond them.
    SyncReport sync(const QDateTime &now = QDateTime::currentDateTime(),
                    const SyncRequest &request = {});
    /// The vault's files changed and not committed, by path.
    [[nodiscard]] QStringList changes() const;

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

    /// How Jot commits to a git remote, as `.jot/vault.yaml` sets it for every computer.
    struct Options
    {
        /// The commit message, with `{device}`, `{date}`, `{time}`, `{count}` and `{files}`
        /// filled in: `gitMessage`.
        QString message;
        /// Whether commits run the repository's hooks: `gitHooks`, off unless it says true.
        bool hooks = false;
        /// Whether commits are signed: `gitSign`; unset leaves it to git's own settings.
        std::optional<bool> sign;
        /// Whether what is fetched is brought in by rebasing onto it, `gitPull: rebase`, the
        /// default, or by merging it, `gitPull: merge`. A rebase that stops on a conflict is
        /// given up for a merge, which Jot can always finish.
        bool rebase = true;
        /// Whether each sync commits what changed, `gitCommit: auto`, the default, or only a
        /// Commit asked for does, `gitCommit: manual`.
        bool autoCommit = true;
        /// Whether each sync pushes, `gitPush: auto`, the default, or only a Push asked for
        /// does, `gitPush: manual`.
        bool autoPush = true;
    };
    /// The commit message used when the vault sets none.
    static const QString kDefaultMessage;
    /// The options the vault at `root` sets.
    [[nodiscard]] static Options optionsOf(const QString &root);
    /// `pattern` with its placeholders filled in for a commit of `paths` from `device` at `now`:
    /// `{files}` names up to three and says how many more.
    [[nodiscard]] static QString commitMessage(const QString &pattern, const QString &device,
                                               const QDateTime &now, const QStringList &paths);

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
    /// Commits what changed here with `message`; gives the paths.
    std::optional<QStringList> commit(const QString &message, QString *error);
    /// Files changed here that the fetched commits change too, so bringing them in would have to
    /// wait for those to be committed.
    [[nodiscard]] QStringList clashingChanges() const;
    /// The author Jot commits as when git has none: `Jot <jot@localhost>`.
    [[nodiscard]] QStringList identity() const;
    /// What a commit is given: its author when git has none, its hooks and its signing.
    [[nodiscard]] QStringList commitArguments() const;
    /// Brings in what was fetched, by rebasing or merging as Options say; false, with `error`,
    /// when it could not be.
    bool bringIn(SyncReport &report, QString *error);
    /// Rebases onto what was fetched; false, with the rebase given up, when it stops.
    bool rebase();
    /// Merges what was fetched, resolving what git could not; false, with `error`, when it
    /// could not be made.
    bool merge(SyncReport &report, QString *error);
    /// Adds what changed between `before` and HEAD to `report.pulled`.
    void notePulled(const QString &before, SyncReport &report) const;
    /// Whether a rebase is under way in the repository.
    [[nodiscard]] bool rebasing() const;
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
    Options m_options;
    /// The vault is a repository of its own, which Jot uses as it is.
    bool m_own = false;
    /// Whether to keep SSH from asking anything, found out at the first git run.
    mutable std::optional<bool> m_batchSsh;
};

} // namespace jot
