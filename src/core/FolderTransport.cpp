#include "jot/FolderTransport.h"

#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>

#include <filesystem>

using namespace Qt::StringLiterals;

namespace jot {

FolderTransport::FolderTransport(QString root) : m_root(QDir(root).absolutePath()) {}

QString FolderTransport::revOf(const QString &path) const
{
    // Size and time to the nanosecond, so two writes in one millisecond still differ.
    std::error_code failed;
    const std::filesystem::path file = QFile::encodeName(m_root + u'/' + path).toStdString();
    if (!std::filesystem::is_regular_file(file, failed))
        return {};
    const auto size = std::filesystem::file_size(file, failed);
    const auto time = std::filesystem::last_write_time(file, failed);
    if (failed)
        return {};
    return u"%1-%2"_s.arg(size).arg(time.time_since_epoch().count());
}

std::optional<QList<RemoteFile>> FolderTransport::list(QString *error)
{
    // A missing folder is an unplugged drive or an unmounted share, not an empty remote, which
    // would read as every file deleted there.
    if (!QFileInfo(m_root).isDir()) {
        *error = QObject::tr("%1 is not there").arg(m_root);
        return std::nullopt;
    }
    QList<RemoteFile> files;
    QDirIterator it(m_root, QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString path = it.next().mid(m_root.size() + 1);
        if (SyncEngine::synced(path))
            files.append({path, revOf(path), {}});
    }
    return files;
}

std::optional<QByteArray> FolderTransport::get(const QString &path, QString *rev, QString *error)
{
    QFile file(m_root + u'/' + path);
    if (!file.open(QIODevice::ReadOnly)) {
        *error = file.errorString();
        return std::nullopt;
    }
    *rev = revOf(path);
    return file.readAll();
}

std::optional<QString> FolderTransport::put(const QString &path, const QByteArray &data,
                                            const QString &expected, bool *stale, QString *error)
{
    if (revOf(path) != expected) {
        *stale = true;
        return std::nullopt;
    }
    const QString target = m_root + u'/' + path;
    if (!QDir().mkpath(QFileInfo(target).path())) {
        *error = QObject::tr("could not create its folder");
        return std::nullopt;
    }
    QSaveFile file(target);
    if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit()) {
        *error = file.errorString();
        return std::nullopt;
    }
    return revOf(path);
}

bool FolderTransport::remove(const QString &path, const QString &expected, bool *stale,
                             QString *error)
{
    if (revOf(path) != expected) {
        *stale = true;
        return false;
    }
    const QString stamp = QDateTime::currentDateTime().toString(u"yyyy-MM-dd HHmmss"_s);
    QString to = m_root + u"/.jot/trash/"_s + stamp + u'/' + path;
    for (int n = 2; QFileInfo::exists(to); ++n)
        to = m_root + u"/.jot/trash/"_s + stamp + u' ' + QString::number(n) + u'/' + path;
    if (!QDir().mkpath(QFileInfo(to).path()) || !QFile::rename(m_root + u'/' + path, to)) {
        *error = QObject::tr("could not move it to the trash");
        return false;
    }
    return true;
}

} // namespace jot
