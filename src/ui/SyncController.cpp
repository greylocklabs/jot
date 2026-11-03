#include "SyncController.h"

#include "jot/Logging.h"
#include "jot/S3Transport.h"
#include "jot/Secrets.h"

#include <QDir>
#include <QFileInfo>
#include <QLocale>
#include <QSysInfo>

using namespace Qt::StringLiterals;

namespace jot {

namespace {

/// Whether one of two folders holds the other, or they are the same.
bool nested(const QString &a, const QString &b)
{
    const QString x = QDir(a).canonicalPath() + u'/';
    const QString y = QDir(b).canonicalPath() + u'/';
    return x.startsWith(y) || y.startsWith(x);
}

} // namespace

SyncController::SyncController(Settings &settings, QObject *parent)
    : QObject(parent), m_settings(settings)
{
    m_soon.setSingleShot(true);
    connect(&m_soon, &QTimer::timeout, this, &SyncController::start);
    m_every.setInterval(kEveryMs);
    connect(&m_every, &QTimer::timeout, this, &SyncController::start);
    m_clock.setInterval(60 * 1000);
    connect(&m_clock, &QTimer::timeout, this, &SyncController::changed);
}

SyncController::~SyncController()
{
    // A sync under way finishes its file and its base, then the app goes.
    if (m_thread)
        m_thread->wait();
}

void SyncController::setVault(const QString &root, std::function<void()> save)
{
    m_root = root;
    m_save = std::move(save);
    m_remote = root.isEmpty() ? QString() : m_settings.syncRemote(root);
    m_secret.clear();
    m_problem.clear();
    m_lastSynced = {};
    m_soon.stop();
    if (configured()) {
        m_every.start();
        m_clock.start();
        m_soon.start(0);
    } else {
        m_every.stop();
        m_clock.stop();
    }
    Q_EMIT changed();
}

void SyncController::localChanged()
{
    if (configured() && !m_soon.isActive())
        m_soon.start(kAfterChangeMs);
}

bool SyncController::syncWithFolder(const QUrl &folder)
{
    const QString path = folder.isLocalFile() ? folder.toLocalFile() : folder.toString();
    if (m_root.isEmpty() || !QFileInfo(path).isDir()) {
        m_problem = tr("Choose a folder to sync with.");
        Q_EMIT changed();
        return false;
    }
    if (nested(path, m_root)) {
        m_problem = tr("Choose a folder outside the notes folder, and not one that holds it.");
        Q_EMIT changed();
        return false;
    }
    m_settings.setSyncRemote(m_root, u"folder:"_s + QDir(path).absolutePath());
    setVault(m_root, m_save);
    return true;
}

void SyncController::syncWithBucket(const QString &endpoint, const QString &region,
                                    const QString &bucket, const QString &folder,
                                    const QString &keyId, const QString &secret)
{
    const QUrl url(endpoint.trimmed());
    if (m_root.isEmpty() || m_thread || !url.isValid() || url.host().isEmpty() ||
        (url.scheme() != u"https" && url.scheme() != u"http") || region.trimmed().isEmpty() ||
        bucket.trimmed().isEmpty() || keyId.trimmed().isEmpty() || secret.isEmpty()) {
        m_problem = tr("Give the bucket's address, region, name and access key.");
        Q_EMIT changed();
        return;
    }
    S3Location location;
    location.endpoint = url.adjusted(QUrl::RemovePath | QUrl::RemoveQuery);
    location.region = region.trimmed();
    location.bucket = bucket.trimmed();
    QString prefix = folder.trimmed();
    while (prefix.startsWith(u'/'))
        prefix.remove(0, 1);
    while (prefix.endsWith(u'/'))
        prefix.chop(1);
    location.prefix = prefix;
    // AWS's own addresses take the bucket in the host name; the rest take it in the path.
    location.pathStyle = !location.endpoint.host().endsWith(u".amazonaws.com"_s);
    const QString credentials = keyId.trimmed() + u':' + secret;
    m_checking = true;
    m_problem.clear();
    Q_EMIT changed();
    m_thread = QThread::create([this, location, credentials] {
        QString error;
        S3Transport bucket(location, credentials);
        const bool ok = bucket.verify(&error);
        QMetaObject::invokeMethod(
            this,
            [this, location, credentials, ok, error] {
                m_thread->wait();
                m_thread = nullptr;
                m_checking = false;
                QString problem = error;
                const QString remote = location.remote();
                if (ok && !secrets::write(remoteSecretKey(remote), credentials, &problem))
                    problem = tr("Could not keep the key in the keyring: %1").arg(problem);
                if (!problem.isEmpty()) {
                    m_problem = problem;
                    Q_EMIT changed();
                    return;
                }
                m_settings.setSyncRemote(m_root, remote);
                setVault(m_root, m_save);
            },
            Qt::QueuedConnection);
    });
    connect(m_thread, &QThread::finished, m_thread, &QObject::deleteLater);
    m_thread->start(QThread::LowPriority);
}

void SyncController::stop()
{
    if (m_root.isEmpty())
        return;
    // A bucket's key leaves the keyring with it.
    if (remoteNeedsSecret(m_remote)) {
        QString error;
        if (!secrets::remove(remoteSecretKey(m_remote), &error))
            qCWarning(lcVault) << "could not take the sync key out of the keyring:" << error;
    }
    // What the last sync knew goes too, so choosing the same remote again starts afresh.
    if (configured() && !m_thread)
        QDir(syncStateDir(m_root, m_remote)).removeRecursively();
    m_settings.setSyncRemote(m_root, {});
    setVault(m_root, m_save);
}

void SyncController::syncNow()
{
    m_soon.stop();
    start();
}

void SyncController::start()
{
    if (!configured())
        return;
    if (m_thread) {
        m_again = true;
        return;
    }
    if (m_save)
        m_save();
    // The key is read once from the keyring, here, as the keyring may ask to be unlocked.
    if (remoteNeedsSecret(m_remote) && m_secret.isEmpty()) {
        QString error;
        const std::optional<QString> secret = secrets::read(remoteSecretKey(m_remote), &error);
        if (!secret) {
            m_problem = tr("The key for %1 is not in the keyring: %2").arg(remote(), error);
            Q_EMIT changed();
            return;
        }
        m_secret = *secret;
    }
    const QString root = m_root;
    const QString remote = m_remote;
    const QString secret = m_secret;
    const QString state = syncStateDir(root, remote);
    const QString device = QSysInfo::machineHostName();
    m_thread = QThread::create([this, root, remote, secret, state, device] {
        SyncReport report;
        QString error;
        if (std::unique_ptr<Transport> transport = transportFor(remote, secret, &error)) {
            SyncEngine engine(root, std::move(transport), state, device);
            report = engine.sync();
        } else {
            report.failure = error;
        }
        QMetaObject::invokeMethod(this, [this, report] { done(report); }, Qt::QueuedConnection);
    });
    connect(m_thread, &QThread::finished, m_thread, &QObject::deleteLater);
    m_thread->start(QThread::LowPriority);
    Q_EMIT changed();
}

void SyncController::done(const SyncReport &report)
{
    if (m_thread) {
        m_thread->wait();
        m_thread = nullptr;
    }
    if (!report.failure.isEmpty()) {
        qCWarning(lcVault) << "sync failed:" << report.failure;
        m_problem = report.failure;
    } else {
        qCInfo(lcVault) << "synced: pulled" << report.pulled.size() << "pushed"
                        << report.pushed.size() << "merged" << report.merged.size() << "conflicts"
                        << report.conflicts;
        for (const QString &error : report.errors)
            qCWarning(lcVault) << "could not sync" << error;
        m_lastSynced = QDateTime::currentDateTime();
        m_problem = report.errors.isEmpty()
                        ? QString()
                        : tr("%n file(s) did not sync; Jot tries again shortly.", nullptr,
                             int(report.errors.size()));
        if (!report.errors.isEmpty() && !m_soon.isActive())
            m_soon.start(kAfterChangeMs);
    }
    Q_EMIT changed();
    Q_EMIT finished(report);
    if (m_again) {
        m_again = false;
        start();
    }
}

QString SyncController::summary() const
{
    if (m_checking)
        return tr("Checking the bucket...");
    if (!configured())
        return tr("Not syncing");
    if (syncing())
        return tr("Syncing...");
    if (!m_problem.isEmpty())
        return tr("Sync: %1").arg(m_problem);
    if (!m_lastSynced.isValid())
        return tr("Not synced yet");
    const qint64 minutes = m_lastSynced.secsTo(QDateTime::currentDateTime()) / 60;
    if (minutes < 1)
        return tr("Synced just now");
    if (minutes < 60)
        return tr("Synced %n minute(s) ago", nullptr, int(minutes));
    return tr("Synced at %1").arg(QLocale().toString(m_lastSynced.time(), QLocale::ShortFormat));
}

} // namespace jot
