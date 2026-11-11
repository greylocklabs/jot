#include "jot/VaultWatcher.h"

#include "jot/Logging.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>

#include <sys/stat.h>

using namespace Qt::StringLiterals;

namespace jot {

namespace {

const QString kNoteSuffix = u".md"_s;

QString join(const QString &folder, const QString &name)
{
    return folder.isEmpty() ? name : folder + u'/' + name;
}

QString parentOf(const QString &path)
{
    const qsizetype slash = path.lastIndexOf(u'/');
    return slash < 0 ? QString() : path.left(slash);
}

bool isUnder(const QString &path, const QString &folder)
{
    return folder.isEmpty() || path.startsWith(folder + u'/');
}

QByteArray hashOf(const QByteArray &bytes)
{
    return QCryptographicHash::hash(bytes, QCryptographicHash::Sha256);
}

} // namespace

VaultWatcher::VaultWatcher(Vault vault, QObject *parent)
    : QObject(parent), m_vault(std::move(vault)), m_ignore(m_vault)
{
    m_settle.setSingleShot(true);
    m_settle.setInterval(300);
    connect(&m_settle, &QTimer::timeout, this, &VaultWatcher::flush);
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, [this](const QString &dir) {
        const QString folder = dir == m_vault.root() ? QString() : m_vault.relative(dir);
        m_dirty.insert(folder);
        m_settle.start();
    });
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, [this](const QString &file) {
        // The watch may now follow a file that was renamed over, so it is renewed once seen.
        const QString path = m_vault.relative(file);
        m_watcher.removePath(file);
        m_watchedNotes.remove(path);
        m_dirty.insert(parentOf(path));
        m_settle.start();
    });
}

void VaultWatcher::watchNote(const QString &path)
{
    if (!m_watchedNotes.contains(path) && m_watcher.addPath(m_vault.absolute(path)))
        m_watchedNotes.insert(path);
}

void VaultWatcher::start()
{
    m_notes.clear();
    m_folders.clear();
    m_watchedNotes.clear();
    m_gitignores.clear();
    m_ignore = VaultIgnore(m_vault);
    if (!m_watcher.directories().isEmpty())
        m_watcher.removePaths(m_watcher.directories());
    if (!m_watcher.files().isEmpty())
        m_watcher.removePaths(m_watcher.files());

    QStringList queue{QString()};
    while (!queue.isEmpty()) {
        const QString folder = queue.takeFirst();
        QSet<QString> folders;
        snapshot(folder, m_notes, folders);
        noteGitignore(folder);
        m_folders.insert(folder);
        m_watcher.addPath(m_vault.absolute(folder));
        queue.append(folders.values());
    }
    for (auto it = m_notes.cbegin(); it != m_notes.cend(); ++it)
        watchNote(it.key());
    qCInfo(lcVault) << "watching" << m_folders.size() << "folders and" << m_notes.size()
                    << "notes in" << m_vault.root();
}

void VaultWatcher::rescan()
{
    for (const QString &folder : std::as_const(m_folders))
        m_dirty.insert(folder);
    flush();
}

void VaultWatcher::expectWrite(const QString &path, const QString &text)
{
    m_expected.insert(path, hashOf(text.toUtf8()));
}

void VaultWatcher::snapshot(const QString &folder, QHash<QString, Seen> &notes,
                            QSet<QString> &folders) const
{
    const QFileInfoList all = QDir(m_vault.absolute(folder))
                                  .entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot);
    QSet<QString> noteNames;
    for (const QFileInfo &info : all) {
        if (info.isFile() && info.fileName().endsWith(kNoteSuffix))
            noteNames.insert(info.fileName().chopped(kNoteSuffix.size()));
    }
    for (const QFileInfo &info : all) {
        const QString name = info.fileName();
        if (name.startsWith(u'.') || m_ignore.ignoredIn(folder, name, info.isDir()))
            continue;
        const QString path = join(folder, name);
        if (info.isDir()) {
            const bool attachments =
                name.endsWith(u".assets"_s) && noteNames.contains(name.chopped(7));
            if (!attachments && !info.isSymLink())
                folders.insert(path);
        } else if (Vault::isListed(name)) {
            struct stat status{};
            if (::stat(QFile::encodeName(info.filePath()).constData(), &status) != 0)
                continue;
            notes.insert(path, {qint64(status.st_size),
                                qint64(status.st_mtim.tv_sec) * 1000000000 + status.st_mtim.tv_nsec,
                                qint64(status.st_ino)});
        }
    }
}

void VaultWatcher::compare(const QString &folder)
{
    // A folder that is gone takes everything under it.
    if (!QFileInfo(m_vault.absolute(folder)).isDir()) {
        for (auto it = m_notes.begin(); it != m_notes.end();) {
            if (isUnder(it.key(), folder)) {
                m_pending.insert(it.key());
                m_watchedNotes.remove(it.key());
                it = m_notes.erase(it);
            } else {
                ++it;
            }
        }
        for (auto it = m_folders.begin(); it != m_folders.end();) {
            if (*it == folder || isUnder(*it, folder)) {
                m_watcher.removePath(m_vault.absolute(*it));
                it = m_folders.erase(it);
                m_foldersChanged = true;
            } else {
                ++it;
            }
        }
        return;
    }

    if (noteGitignore(folder)) {
        Q_EMIT ignoreChanged();
        return;
    }
    QHash<QString, Seen> now;
    QSet<QString> folders;
    snapshot(folder, now, folders);

    for (auto it = m_notes.begin(); it != m_notes.end();) {
        if (parentOf(it.key()) == folder && !now.contains(it.key())) {
            m_pending.insert(it.key());
            m_watchedNotes.remove(it.key());
            it = m_notes.erase(it);
        } else {
            ++it;
        }
    }
    for (auto it = now.cbegin(); it != now.cend(); ++it) {
        watchNote(it.key());
        const auto old = m_notes.constFind(it.key());
        if (old != m_notes.cend() && *old == it.value())
            continue;
        m_notes.insert(it.key(), it.value());
        if (!expected(it.key()))
            m_pending.insert(it.key());
    }

    // Folders that appeared are read whole, since their notes made no events of their own.
    for (const QString &added : std::as_const(folders)) {
        if (m_folders.contains(added))
            continue;
        m_foldersChanged = true;
        QStringList queue{added};
        while (!queue.isEmpty()) {
            const QString next = queue.takeFirst();
            QHash<QString, Seen> notes;
            QSet<QString> below;
            snapshot(next, notes, below);
            noteGitignore(next);
            m_folders.insert(next);
            m_watcher.addPath(m_vault.absolute(next));
            for (auto it = notes.cbegin(); it != notes.cend(); ++it) {
                m_notes.insert(it.key(), it.value());
                m_pending.insert(it.key());
                watchNote(it.key());
            }
            queue.append(below.values());
        }
    }
    const QSet<QString> known = m_folders;
    for (const QString &existing : known) {
        if (!existing.isEmpty() && parentOf(existing) == folder && !folders.contains(existing))
            compare(existing);
    }
}

void VaultWatcher::flush()
{
    const QSet<QString> dirty = std::exchange(m_dirty, {});
    for (const QString &folder : dirty)
        compare(folder);
    if (std::exchange(m_foldersChanged, false))
        Q_EMIT foldersChanged();
    if (m_pending.isEmpty())
        return;
    QStringList notes = std::exchange(m_pending, {}).values();
    notes.sort();
    qCDebug(lcVault) << "changed on disk:" << notes;
    Q_EMIT changed(notes);
}

bool VaultWatcher::noteGitignore(const QString &folder)
{
    struct stat status{};
    Seen seen;
    const QString file = m_vault.absolute(join(folder, u".gitignore"_s));
    if (::stat(QFile::encodeName(file).constData(), &status) == 0) {
        // Written in place, it changes no folder, so it is watched itself.
        if (!m_watcher.files().contains(file))
            m_watcher.addPath(file);
        seen = {qint64(status.st_size),
                qint64(status.st_mtim.tv_sec) * 1000000000 + status.st_mtim.tv_nsec,
                qint64(status.st_ino)};
    }
    const auto old = m_gitignores.constFind(folder);
    const bool changed = old != m_gitignores.cend() && *old != seen;
    m_gitignores.insert(folder, seen);
    return changed;
}

bool VaultWatcher::expected(const QString &path)
{
    const auto it = m_expected.constFind(path);
    if (it == m_expected.cend())
        return false;
    QFile file(m_vault.absolute(path));
    const bool same = file.open(QIODevice::ReadOnly) && hashOf(file.readAll()) == *it;
    m_expected.erase(it);
    return same;
}

} // namespace jot
