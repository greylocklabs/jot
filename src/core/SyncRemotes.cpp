#include "jot/DriveTransport.h"
#include "jot/DropboxTransport.h"
#include "jot/FolderTransport.h"
#include "jot/GitSync.h"
#include "jot/S3Transport.h"
#include "jot/Sync.h"

#include "greylock/Identity.h"

#include <QCryptographicHash>
#include <QDir>
#include <QStandardPaths>
#include <QUrl>

using namespace Qt::StringLiterals;

namespace jot {

std::unique_ptr<Transport> transportFor(const QString &remote, const QString &secret,
                                        QString *error)
{
    const QString kind = remote.section(u':', 0, 0);
    const QString where = remote.section(u':', 1);
    if (kind == u"folder" && !where.isEmpty())
        return std::make_unique<FolderTransport>(where);
    if (kind == u"gdrive" && !where.isEmpty()) {
        if (googleClientId().isEmpty()) {
            *error = QObject::tr("this build of Jot cannot sync with Google Drive");
            return nullptr;
        }
        if (secret.isEmpty()) {
            *error = QObject::tr("Google's sign-in is not in the keyring");
            return nullptr;
        }
        return std::make_unique<DriveTransport>(googleClientId(), googleClientSecret(), secret,
                                                where);
    }
    if (kind == u"dropbox") {
        if (dropboxAppKey().isEmpty()) {
            *error = QObject::tr("this build of Jot cannot sync with Dropbox");
            return nullptr;
        }
        if (secret.isEmpty()) {
            *error = QObject::tr("Dropbox's sign-in is not in the keyring");
            return nullptr;
        }
        return std::make_unique<DropboxTransport>(dropboxAppKey(), secret, where);
    }
    if (const std::optional<S3Location> bucket = S3Location::parse(remote)) {
        if (secret.isEmpty()) {
            *error =
                QObject::tr("the access key for %1 is not in the keyring").arg(remoteLabel(remote));
            return nullptr;
        }
        return std::make_unique<S3Transport>(*bucket, secret);
    }
    *error = QObject::tr("Jot does not know how to sync with %1").arg(remote);
    return nullptr;
}

std::pair<QString, QString> gitRemote(const QString &remote)
{
    const QString rest = remote.mid(4);
    const qsizetype hash = rest.lastIndexOf(u'#');
    if (hash < 0)
        return {rest, u"main"_s};
    return {rest.left(hash), rest.mid(hash + 1)};
}

SyncReport syncVault(const QString &root, const QString &remote, const QString &secret,
                     const QString &stateDir, const QString &device, const SyncRequest &request)
{
    if (remote.startsWith(u"git:"_s)) {
        const auto [url, branch] = gitRemote(remote);
        return GitSync(root, url, branch, stateDir + u"/git"_s, device)
            .sync(QDateTime::currentDateTime(), request);
    }
    QString error;
    std::unique_ptr<Transport> transport = transportFor(remote, secret, &error);
    if (!transport) {
        SyncReport report;
        report.failure = error;
        return report;
    }
    return SyncEngine(root, std::move(transport), stateDir, device).sync();
}

QStringList uncommittedChanges(const QString &root, const QString &remote, const QString &stateDir)
{
    if (!remote.startsWith(u"git:"_s))
        return {};
    const auto [url, branch] = gitRemote(remote);
    return GitSync(root, url, branch, stateDir + u"/git"_s, {}).changes();
}

bool remoteNeedsSecret(const QString &remote)
{
    return remote.startsWith(u"s3:"_s) || remote.startsWith(u"dropbox:"_s) ||
           remote.startsWith(u"gdrive:"_s);
}

QString remoteSecretKey(const QString &remote)
{
    return u"sync/"_s + remote;
}

QString remoteLabel(const QString &remote)
{
    if (remote.startsWith(u"git:"_s)) {
        // Never a password or token, should one have been saved before Jot refused them: over
        // HTTP that is the user name as well.
        QUrl url(gitRemote(remote).first);
        if (url.isValid() && url.scheme().startsWith(u"http"_s))
            url.setUserInfo({});
        else if (url.isValid() && !url.scheme().isEmpty())
            url.setPassword({});
        else
            return gitRemote(remote).first;
        return url.toString();
    }
    if (remote.startsWith(u"gdrive:"_s))
        return u"Google Drive"_s;
    if (remote.startsWith(u"dropbox:"_s))
        return u"Dropbox"_s + remote.section(u':', 1);
    if (const std::optional<S3Location> bucket = S3Location::parse(remote)) {
        QString label = bucket->endpoint.host() + u'/' + bucket->bucket;
        if (!bucket->prefix.isEmpty())
            label += u'/' + bucket->prefix;
        return label;
    }
    return remote.section(u':', 1);
}

QString syncStateDir(const QString &vaultRoot, const QString &remote)
{
    const QByteArray pair = QDir(vaultRoot).absolutePath().toUtf8() + '\n' + remote.toUtf8();
    return QStandardPaths::writableLocation(QStandardPaths::GenericStateLocation) + u'/' +
           greylock::identity::dirName() + u"/sync/"_s +
           QString::fromLatin1(
               QCryptographicHash::hash(pair, QCryptographicHash::Sha256).toHex().left(16));
}

} // namespace jot
