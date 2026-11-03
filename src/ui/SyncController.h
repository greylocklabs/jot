#pragma once

#include "jot/Settings.h"
#include "jot/Sync.h"

#include <QDateTime>
#include <QObject>
#include <QPointer>
#include <QQmlEngine>
#include <QThread>
#include <QTimer>
#include <QUrl>

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
    Q_PROPERTY(bool syncing READ syncing NOTIFY changed)
    Q_PROPERTY(QDateTime lastSynced READ lastSynced NOTIFY changed)
    /// What went wrong with the last sync; empty when it went well.
    Q_PROPERTY(QString problem READ problem NOTIFY changed)
    /// A bucket is being checked before it is synced with.
    Q_PROPERTY(bool checking READ checking NOTIFY changed)
    /// How sync stands, in a few words: "Synced 3 minutes ago", "Syncing...".
    Q_PROPERTY(QString summary READ summary NOTIFY changed)

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
    /// Stops syncing this vault, forgetting what the last sync knew; its files stay as they are,
    /// here and there.
    Q_INVOKABLE void stop();
    Q_INVOKABLE void syncNow();

    [[nodiscard]] bool configured() const { return !m_remote.isEmpty(); }
    [[nodiscard]] QString remote() const { return remoteLabel(m_remote); }
    [[nodiscard]] bool syncing() const { return m_thread != nullptr && !m_checking; }
    [[nodiscard]] bool checking() const { return m_checking; }
    [[nodiscard]] QDateTime lastSynced() const { return m_lastSynced; }
    [[nodiscard]] QString problem() const { return m_problem; }
    [[nodiscard]] QString summary() const;

    /// How long after a change here a sync follows, and how often one runs regardless.
    static constexpr int kAfterChangeMs = 10 * 1000;
    static constexpr int kEveryMs = 5 * 60 * 1000;

Q_SIGNALS:
    void changed();
    /// A sync finished; `report` says what it did.
    void finished(const jot::SyncReport &report);

private:
    void start();
    void done(const SyncReport &report);

    Settings &m_settings;
    QString m_root;
    QString m_remote;
    /// The remote's key, read from the keyring when first needed.
    QString m_secret;
    std::function<void()> m_save;
    QPointer<QThread> m_thread;
    bool m_again = false;
    bool m_checking = false;
    QDateTime m_lastSynced;
    QString m_problem;
    QTimer m_soon;
    QTimer m_every;
    /// Says "3 minutes ago" anew each minute.
    QTimer m_clock;
};

} // namespace jot
