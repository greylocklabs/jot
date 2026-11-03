#include "jot/FolderTransport.h"
#include "jot/S3Transport.h"
#include "jot/Sync.h"

#include "greylock/Identity.h"

#include <QCryptographicHash>
#include <QDir>
#include <QStandardPaths>

using namespace Qt::StringLiterals;

namespace jot {

std::unique_ptr<Transport> transportFor(const QString &remote, const QString &secret,
                                        QString *error)
{
    const QString kind = remote.section(u':', 0, 0);
    const QString where = remote.section(u':', 1);
    if (kind == u"folder" && !where.isEmpty())
        return std::make_unique<FolderTransport>(where);
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

bool remoteNeedsSecret(const QString &remote)
{
    return remote.startsWith(u"s3:"_s);
}

QString remoteSecretKey(const QString &remote)
{
    return u"sync/"_s + remote;
}

QString remoteLabel(const QString &remote)
{
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
