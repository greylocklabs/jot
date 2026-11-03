#include "jot/FolderTransport.h"
#include "jot/Sync.h"

#include "greylock/Identity.h"

#include <QCryptographicHash>
#include <QDir>
#include <QStandardPaths>

using namespace Qt::StringLiterals;

namespace jot {

std::unique_ptr<Transport> transportFor(const QString &remote, QString *error)
{
    const QString kind = remote.section(u':', 0, 0);
    const QString where = remote.section(u':', 1);
    if (kind == u"folder" && !where.isEmpty())
        return std::make_unique<FolderTransport>(where);
    *error = QObject::tr("Jot does not know how to sync with %1").arg(remote);
    return nullptr;
}

QString remoteLabel(const QString &remote)
{
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
