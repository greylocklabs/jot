#include "SyncController.h"

#include "jot/DriveTransport.h"
#include "jot/DropboxTransport.h"
#include "jot/GitSync.h"
#include "jot/Logging.h"
#include "jot/S3Transport.h"
#include "jot/Secrets.h"

#include <QDesktopServices>
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
    // A sync under way finishes its file and its base, then the app goes; a sign-in waiting
    // for the browser stops waiting.
    m_quitting = true;
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
    const QString remote = u"folder:"_s + QDir(path).absolutePath();
    if (takenByAnotherVault(remote)) {
        m_problem = tr("Another vault on this computer syncs with that folder; choose another.");
        Q_EMIT changed();
        return false;
    }
    m_settings.setSyncRemote(m_root, remote);
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
    connectRemote(location.remote(), credentials, [location](QString *secret, QString *) {
        QString error;
        S3Transport bucket(location, *secret);
        bucket.verify(&error);
        return error;
    });
}

QUrl SyncController::startDropbox()
{
    const DropboxSignIn signIn = DropboxSignIn::start(dropboxAppKey());
    m_dropboxVerifier = signIn.verifier;
    return signIn.url;
}

void SyncController::connectDropbox(const QString &code, const QString &folder)
{
    if (m_root.isEmpty() || m_thread || m_dropboxVerifier.isEmpty() || code.trimmed().isEmpty()) {
        m_problem = tr("Open Dropbox to let Jot in, then paste the code it shows.");
        Q_EMIT changed();
        return;
    }
    QString where = folder.trimmed();
    while (where.endsWith(u'/'))
        where.chop(1);
    if (!where.isEmpty() && !where.startsWith(u'/'))
        where.prepend(u'/');
    const QString verifier = m_dropboxVerifier;
    // The code is traded for a lasting token on the thread, which then is the secret kept.
    connectRemote(u"dropbox:"_s + where, code, [verifier, where](QString *secret, QString *) {
        QString error;
        DropboxSignIn signIn;
        signIn.verifier = verifier;
        const std::optional<QString> token = signIn.finish(dropboxAppKey(), *secret, &error);
        if (!token)
            return error;
        *secret = *token;
        DropboxTransport dropbox(dropboxAppKey(), *token, where);
        dropbox.list(&error);
        return error;
    });
}

void SyncController::connectDrive(const QString &folder)
{
    if (m_root.isEmpty() || m_thread)
        return;
    QString name = folder.trimmed();
    if (name.isEmpty())
        name = QDir(m_root).dirName();
    if (name.contains(u'/')) {
        m_problem = tr("Give a folder name without a slash.");
        Q_EMIT changed();
        return;
    }
    connectRemote({}, {}, [this, name](QString *secret, QString *remote) {
        jot::DriveSignIn signIn;
        const QUrl url = signIn.start(googleClientId());
        if (url.isEmpty())
            return tr("Could not listen for Google's answer on this computer.");
        QMetaObject::invokeMethod(
            this, [url] { QDesktopServices::openUrl(url); }, Qt::QueuedConnection);
        QString error;
        const std::optional<QString> token =
            signIn.finish(googleClientId(), googleClientSecret(), kSignInMs, &error, &m_quitting);
        if (!token)
            return error;
        *secret = *token;
        // Found by its name from every computer, as Jot can see only what it made; a folder
        // for each vault, so two vaults never share their notes.
        DriveTransport drive(googleClientId(), googleClientSecret(), *token, {});
        const std::optional<QString> jot = drive.folderNamed(u"Jot"_s, &error);
        if (!jot)
            return error;
        const std::optional<QString> vault = drive.folderNamed(name, &error, *jot);
        if (!vault)
            return error;
        *remote = u"gdrive:"_s + *vault;
        return QString();
    });
}

bool SyncController::gitAvailable()
{
    static const bool available = GitSync::available();
    return available;
}

void SyncController::connectGit(const QString &url, const QString &branch)
{
    const QString repository = url.trimmed();
    const QString on = branch.trimmed().isEmpty() ? u"main"_s : branch.trimmed();
    if (m_root.isEmpty() || m_thread || repository.isEmpty()) {
        m_problem = tr("Give the repository's address.");
        Q_EMIT changed();
        return;
    }
    connectRemote(u"git:"_s + repository + u'#' + on, {}, [repository](QString *, QString *) {
        QString error;
        GitSync::reachable(repository, &error);
        return error;
    });
}

void SyncController::connectRemote(const QString &remote, const QString &secret,
                                   std::function<QString(QString *secret, QString *remote)> check)
{
    m_checking = true;
    m_problem.clear();
    Q_EMIT changed();
    const QString root = m_root;
    m_thread = QThread::create([this, root, remote, secret, check] {
        QString kept = secret;
        QString named = remote;
        const QString error = check(&kept, &named);
        QMetaObject::invokeMethod(
            this,
            [this, root, remote = named, kept, error] {
                m_thread->wait();
                m_thread = nullptr;
                m_checking = false;
                // Another vault opened meanwhile: this one was not asked to sync with it.
                if (m_root != root) {
                    Q_EMIT changed();
                    return;
                }
                QString problem = error;
                if (problem.isEmpty() && takenByAnotherVault(remote))
                    problem = tr("Another vault on this computer syncs there; give this one "
                                 "another folder.");
                if (problem.isEmpty() && remoteNeedsSecret(remote) &&
                    !secrets::write(remoteSecretKey(remote), kept, &problem))
                    problem = tr("Could not keep the key in the keyring: %1").arg(problem);
                if (!problem.isEmpty()) {
                    m_problem = problem;
                    Q_EMIT changed();
                    return;
                }
                m_dropboxVerifier.clear();
                m_settings.setSyncRemote(m_root, remote);
                setVault(m_root, m_save);
            },
            Qt::QueuedConnection);
    });
    connect(m_thread, &QThread::finished, m_thread, &QObject::deleteLater);
    m_thread->start(QThread::LowPriority);
}

bool SyncController::takenByAnotherVault(const QString &remote) const
{
    const int own = m_settings.syncRemote(m_root) == remote ? 1 : 0;
    return m_settings.syncRemoteUses(remote) > own;
}

void SyncController::stop()
{
    if (m_root.isEmpty())
        return;
    // A bucket's key leaves the keyring with it, unless another vault syncs there too.
    if (remoteNeedsSecret(m_remote) && m_settings.syncRemoteUses(m_remote) <= 1) {
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
        const SyncReport report = syncVault(root, remote, secret, state, device);
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
        return tr("Checking...");
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
