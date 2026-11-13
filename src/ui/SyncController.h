#pragma once

#include "jot/DriveTransport.h"
#include "jot/DropboxTransport.h"
#include "jot/Settings.h"
#include "jot/Sync.h"

#include <QDateTime>
#include <QObject>
#include <QPointer>
#include <QQmlEngine>
#include <QThread>
#include <QTimer>
#include <QUrl>
#include <QVariantMap>

#include <atomic>

namespace jot {

/// Keeps the open vault in step with where it syncs to: when it opens, shortly after each
/// change, every few minutes, and when asked. Each sync runs on a thread of its own, one at a
/// time; what it changes in the vault reaches the app as any outside edit does.
class SyncController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Given by Vault")
    /// The vault syncs with somewhere.
    Q_PROPERTY(bool configured READ configured NOTIFY changed)
    /// Where it syncs to, in words: a folder's path.
    Q_PROPERTY(QString remote READ remote NOTIFY changed)
    /// Whether the vault syncs with a git repository, which has options of its own.
    Q_PROPERTY(bool git READ git NOTIFY changed)
    /// For a git remote: whether commits, and pushes, are made by hand, as the vault says.
    Q_PROPERTY(bool manualCommits READ manualCommits NOTIFY changed)
    Q_PROPERTY(bool manualPushes READ manualPushes NOTIFY changed)
    /// Files changed and not committed, and commits not pushed, as the last sync left them.
    Q_PROPERTY(int uncommitted READ uncommitted NOTIFY changed)
    Q_PROPERTY(int unpushed READ unpushed NOTIFY changed)
    /// Minutes between automatic syncs, `syncEvery` in the vault's settings; 0 syncs only when
    /// asked.
    Q_PROPERTY(int syncEvery READ syncEvery NOTIFY changed)
    Q_PROPERTY(bool syncing READ syncing NOTIFY changed)
    Q_PROPERTY(QDateTime lastSynced READ lastSynced NOTIFY changed)
    /// What went wrong with the last sync; empty when it went well.
    Q_PROPERTY(QString problem READ problem NOTIFY changed)
    /// This build can sync with Dropbox, having an app key for it.
    Q_PROPERTY(bool dropboxAvailable READ dropboxAvailable CONSTANT)
    /// The git command is installed, so the vault can sync with a git repository.
    Q_PROPERTY(bool gitAvailable READ gitAvailable CONSTANT)
    /// This build can sync with Google Drive, having a client for it.
    Q_PROPERTY(bool driveAvailable READ driveAvailable CONSTANT)
    /// A bucket is being checked before it is synced with.
    Q_PROPERTY(bool checking READ checking NOTIFY changed)
    /// How sync stands, in a few words: "Synced 3 minutes ago", "Syncing...".
    Q_PROPERTY(QString summary READ summary NOTIFY changed)
    /// When the last sync that worked was, as "Updated 3:45 PM"; empty before one.
    Q_PROPERTY(QString stamp READ stamp NOTIFY changed)
    /// What the last sync did, and what went wrong since, in lines to hover over.
    Q_PROPERTY(QString details READ details NOTIFY changed)
    /// Notes changed here since the last sync, which the next one sends, as setPending() says.
    Q_PROPERTY(bool pending READ pending NOTIFY changed)

public:
    explicit SyncController(Settings &settings, QObject *parent = nullptr);
    ~SyncController() override;

    /// The vault now open, at `root`, or none when empty. `save` writes what the editor holds,
    /// before each sync reads the files.
    void setVault(const QString &root, std::function<void()> save);
    /// Something in the vault changed here; a sync follows shortly.
    void localChanged();

    /// Syncs the vault with the folder at `folder` from now on, starting at once. Refused, with
    /// a problem saying why, for a folder inside the vault or holding it.
    Q_INVOKABLE bool syncWithFolder(const QUrl &folder);
    /// Syncs the vault with an S3-compatible bucket from now on, once it is checked: it must be
    /// reachable with this key and refuse writes over objects it was not told of. The key goes
    /// in the keyring. Says why not in `problem` otherwise.
    Q_INVOKABLE void syncWithBucket(const QString &endpoint, const QString &region,
                                    const QString &bucket, const QString &folder,
                                    const QString &keyId, const QString &secret);
    /// Where to send the user to let Jot into its folder in their Dropbox; the code Dropbox
    /// then shows goes to connectDropbox().
    Q_INVOKABLE QUrl startDropbox();
    /// Syncs the vault with Jot's folder in the user's Dropbox, under `folder` there, once the
    /// code from the sign-in is traded for a lasting one, kept in the keyring.
    Q_INVOKABLE void connectDropbox(const QString &code, const QString &folder);
    /// Signs in to Google in the browser and syncs the vault with the folder `folder` in Jot's
    /// folder in the user's Drive, by default named as the vault is, made if need be, keeping
    /// the sign-in in the keyring.
    Q_INVOKABLE void connectDrive(const QString &folder);
    /// Syncs the vault with the git repository at `url`, on `branch` (`main` when empty), once
    /// it answers with the keys and settings this computer has for git; Jot keeps none.
    Q_INVOKABLE void connectGit(const QString &url, const QString &branch);
    /// Commits what changed with `message`, then syncs.
    Q_INVOKABLE void commit(const QString &message);
    /// Syncs and pushes, where pushes are made by hand.
    Q_INVOKABLE void push();
    /// The files changed and not committed, for a git remote.
    Q_INVOKABLE QStringList changes() const;
    /// The commit message the vault's template makes for `paths`.
    Q_INVOKABLE QString suggestedMessage(const QStringList &paths) const;
    /// Reads the vault's sync options again, after they changed.
    void reloadOptions();
    /// The commit message git sync uses when the vault sets none.
    Q_INVOKABLE static QString defaultCommitMessage();
    /// The git repository the vault is, as `{exists, origin, branch}`: what connectGit() must
    /// be given for it, since Jot syncs such a vault on its own origin and branch.
    Q_INVOKABLE QVariantMap ownRepository() const;
    /// Stops syncing this vault, forgetting what the last sync knew; its files stay as they are,
    /// here and there.
    Q_INVOKABLE void stop();
    Q_INVOKABLE void syncNow();

    [[nodiscard]] bool configured() const { return !m_remote.isEmpty(); }
    [[nodiscard]] QString remote() const { return remoteLabel(m_remote); }
    [[nodiscard]] bool git() const { return m_remote.startsWith(u"git:"); }
    [[nodiscard]] bool manualCommits() const;
    [[nodiscard]] bool manualPushes() const;
    [[nodiscard]] int uncommitted() const { return m_uncommitted; }
    [[nodiscard]] int unpushed() const { return m_unpushed; }
    [[nodiscard]] int syncEvery() const;
    /// What to say when `failed` files did not sync, syncing every `every` minutes, or only
    /// when asked for 0; empty for none.
    [[nodiscard]] static QString failedFiles(int failed, int every);
    [[nodiscard]] bool syncing() const { return m_thread != nullptr && !m_checking; }
    [[nodiscard]] bool checking() const { return m_checking; }
    [[nodiscard]] static bool dropboxAvailable() { return !dropboxAppKey().isEmpty(); }
    [[nodiscard]] static bool driveAvailable() { return !googleClientId().isEmpty(); }
    [[nodiscard]] static bool gitAvailable();
    /// How long a sign-in in the browser may take.
    static constexpr int kSignInMs = 5 * 60 * 1000;
    [[nodiscard]] QDateTime lastSynced() const { return m_lastSynced; }
    [[nodiscard]] QString problem() const { return m_problem; }
    [[nodiscard]] QString stamp() const;
    [[nodiscard]] QString details() const;
    [[nodiscard]] bool pending() const { return m_pending; }
    void setPending(bool pending);
    /// When the last sync that worked began; notes changed since, apart from those it wrote,
    /// wait for the next.
    [[nodiscard]] QDateTime syncedFrom() const { return m_syncedFrom; }
    /// The notes the last sync that worked wrote here.
    [[nodiscard]] QStringList written() const;
    [[nodiscard]] QString summary() const;

    /// How long after a change here a sync follows, and how often one runs regardless unless
    /// the vault says otherwise.
    static constexpr int kAfterChangeMs = 10 * 1000;
    static constexpr int kEveryMs = 5 * 60 * 1000;

Q_SIGNALS:
    void changed();
    /// A sync finished; `report` says what it did.
    void finished(const jot::SyncReport &report);

private:
    void start();
    void done(const SyncReport &report);
    /// Starts or stops the timer for automatic syncs, as syncEvery() says.
    void applyInterval();
    /// Checks a new remote on a thread of its own with `check`, which may change the secret
    /// and name the remote, then keeps the secret for it in the keyring and syncs with it; or
    /// says what went wrong.
    void connectRemote(const QString &remote, const QString &secret,
                       std::function<QString(QString *secret, QString *remote)> check);
    /// Whether another vault on this computer syncs with `remote`, which would mix the two.
    [[nodiscard]] bool takenByAnotherVault(const QString &remote) const;

    Settings &m_settings;
    QString m_root;
    QString m_remote;
    /// The remote's key, read from the keyring when first needed.
    QString m_secret;
    /// The Dropbox sign-in under way, until its code comes back.
    QString m_dropboxVerifier;
    std::function<void()> m_save;
    QPointer<QThread> m_thread;
    bool m_again = false;
    /// The app is going: a sign-in waiting for the browser gives up.
    std::atomic<bool> m_quitting = false;
    bool m_checking = false;
    QDateTime m_lastSynced;
    QString m_problem;
    /// When the problem came, and what the last sync that worked did.
    QDateTime m_failedAt;
    QDateTime m_startedAt;
    QDateTime m_syncedFrom;
    SyncReport m_lastReport;
    bool m_pending = false;
    /// A commit or push asked for, to go with the next sync.
    SyncRequest m_request;
    int m_uncommitted = 0;
    int m_unpushed = 0;
    QTimer m_soon;
    QTimer m_every;
    /// Says "3 minutes ago" anew each minute.
    QTimer m_clock;
};

} // namespace jot
