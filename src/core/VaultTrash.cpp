#include "jot/VaultTrash.h"

#include "jot/Logging.h"
#include "jot/Vault.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QRegularExpression>

#include <algorithm>

using namespace Qt::StringLiterals;

namespace jot {

namespace {

// Whether `relative`, inside a trashed notebook or stamp folder, is a note rather than a file
// in some note's attachments.
bool isNoteIn(const QString &relative)
{
    if (!Vault::isListed(relative))
        return false;
    const QStringList folders = relative.split(u'/').mid(0, relative.count(u'/'));
    return std::ranges::none_of(
        folders, [](const QString &folder) { return folder.endsWith(u".assets"_s); });
}

// The notes in `folder`, by their path inside it.
QStringList notesIn(const QString &folder)
{
    QStringList found;
    QDirIterator files(folder, QDir::Files | QDir::Hidden, QDirIterator::Subdirectories);
    while (files.hasNext()) {
        const QString relative = files.next().mid(folder.size() + 1);
        if (isNoteIn(relative))
            found << relative;
    }
    return found;
}

// When a folder of the vault's own trash was made, from its name: Jot's own stamp or a sync's,
// either with a number after it when the same second was taken.
QDateTime stampTime(const QString &name)
{
    static const QRegularExpression stamp(
        u"^(\\d{8}-\\d{6}|\\d{4}-\\d{2}-\\d{2} \\d{6})(?: \\d+)?$"_s);
    const QRegularExpressionMatch match = stamp.match(name);
    if (!match.hasMatch())
        return {};
    const QString at = match.captured(1);
    return QDateTime::fromString(at,
                                 at.contains(u' ') ? u"yyyy-MM-dd HHmmss"_s : u"yyyyMMdd-HHmmss"_s);
}

// When what is in a folder of the trash was deleted: by its stamp, or else when it was made.
QDateTime deletedAt(const QFileInfo &stamp)
{
    const QDateTime at = stampTime(stamp.fileName());
    return at.isValid() ? at : stamp.lastModified();
}

} // namespace

VaultTrash::VaultTrash(const Vault &vault) : m_vault(vault) {}

QString VaultTrash::folder() const
{
    return m_vault.absolute(u".jot/trash"_s);
}

QList<TrashedNote> VaultTrash::list() const
{
    QList<TrashedNote> found;
    const QDir trash(folder());
    for (const QFileInfo &stamp : trash.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        const QDateTime deleted = deletedAt(stamp);
        const QString inside = stamp.filePath();
        for (const QString &relative : notesIn(inside)) {
            const QString assets = inside + u'/' + Vault::assetsFolderFor(relative);
            found << TrashedNote{inside + u'/' + relative,
                                 relative,
                                 deleted,
                                 QFileInfo(assets).isDir() ? assets : QString(),
                                 {}};
        }
    }
    std::ranges::stable_sort(
        found, [](const TrashedNote &a, const TrashedNote &b) { return a.deleted > b.deleted; });
    return found;
}

void VaultTrash::readText(QList<TrashedNote> &notes, const QList<TrashedNote> &known)
{
    QHash<QString, QString> read;
    for (const TrashedNote &note : known)
        read.insert(note.location, note.text);
    for (TrashedNote &note : notes) {
        if (const auto it = read.constFind(note.location); it != read.cend()) {
            note.text = *it;
            continue;
        }
        QString why;
        note.text = notefile::read(note.location, &why).value_or(QString());
    }
}

QList<TrashedNote> VaultTrash::matching(const QList<TrashedNote> &notes, const QString &query)
{
    const QStringList words = query.simplified().split(u' ', Qt::SkipEmptyParts);
    if (words.isEmpty())
        return notes;
    QList<TrashedNote> found;
    for (const TrashedNote &note : notes) {
        // Its title and folder, not the file's extension, which every note shares.
        const QString haystack = Vault::notebookOf(note.path) + u'\n' +
                                 Vault::titleFromPath(note.path) + u'\n' + note.text;
        if (std::ranges::all_of(words, [&haystack](const QString &word) {
                return haystack.contains(word, Qt::CaseInsensitive);
            }))
            found << note;
    }
    return found;
}

std::optional<QString> VaultTrash::restore(const TrashedNote &note, QString *error) const
{
    if (!m_vault.writable()) {
        *error = QObject::tr("this vault is from a newer Jot, so this one only reads it");
        return std::nullopt;
    }
    if (!QFileInfo(note.location).isFile()) {
        *error = QObject::tr("%1 is not in the trash any more").arg(note.path);
        return std::nullopt;
    }
    QString path = note.path;
    // Never anywhere but inside the vault, whatever the trash's record says.
    const QString inside = QDir::cleanPath(m_vault.absolute(path));
    if (path.isEmpty() || !inside.startsWith(m_vault.root() + u'/') ||
        inside.mid(m_vault.root().size() + 1) != path) {
        *error = QObject::tr("%1 is not a place in this vault").arg(note.path);
        return std::nullopt;
    }
    const QString folder = Vault::notebookOf(path);
    // Where another note has the place now, it comes back beside it.
    if (QFileInfo::exists(m_vault.absolute(path)) ||
        QFileInfo::exists(m_vault.absolute(Vault::assetsFolderFor(path))))
        path = m_vault.freePathLike(path, folder, Vault::titleFromPath(path));
    if (!QDir().mkpath(m_vault.absolute(folder)) ||
        !QFile::rename(note.location, m_vault.absolute(path))) {
        *error = QObject::tr("could not bring %1 back from the trash").arg(note.path);
        return std::nullopt;
    }
    if (!note.assets.isEmpty() && QFileInfo(note.assets).isDir() &&
        !QDir().rename(note.assets, m_vault.absolute(Vault::assetsFolderFor(path)))) {
        qCWarning(lcVault) << "could not bring the attachments of" << path << "back";
        *error = QObject::tr("%1 is back, but its attachments are still in the trash").arg(path);
    }
    prune(QFileInfo(note.location).path());
    return path;
}

bool VaultTrash::remove(const TrashedNote &note, QString *error) const
{
    if (!m_vault.writable()) {
        *error = QObject::tr("this vault is from a newer Jot, so this one only reads it");
        return false;
    }
    // Only what is inside the trash, whatever `note` says.
    const QString inside = folder() + u'/';
    if (!QDir::cleanPath(note.location).startsWith(inside) ||
        (!note.assets.isEmpty() && !QDir::cleanPath(note.assets).startsWith(inside))) {
        *error = QObject::tr("%1 is not in the trash").arg(note.path);
        return false;
    }
    if (!QFileInfo(note.location).isFile()) {
        *error = QObject::tr("%1 is not in the trash any more").arg(note.path);
        return false;
    }
    if (!QFile::remove(note.location) ||
        (!note.assets.isEmpty() && !QDir(note.assets).removeRecursively())) {
        *error = QObject::tr("could not delete %1").arg(note.path);
        return false;
    }
    prune(QFileInfo(note.location).path());
    return true;
}

bool VaultTrash::empty(QString *error) const
{
    if (!m_vault.writable()) {
        *error = QObject::tr("this vault is from a newer Jot, so this one only reads it");
        return false;
    }
    if (!QDir(folder()).removeRecursively()) {
        *error = QObject::tr("could not empty the trash");
        return false;
    }
    return true;
}

int VaultTrash::expire(const QDateTime &now, int days, const Discard &discard) const
{
    if (days <= 0 || !m_vault.writable())
        return 0;
    const QDateTime cutoff = now.addDays(-days);
    int moved = 0;
    for (const TrashedNote &note : list()) {
        if (!note.deleted.isValid() || note.deleted > cutoff)
            continue;
        if (!discard(note.location))
            continue;
        ++moved;
        if (!note.assets.isEmpty())
            discard(note.assets);
        prune(QFileInfo(note.location).path());
    }
    // What is left of an old enough folder, attachments no note claims, goes as it is.
    for (const QFileInfo &stamp : QDir(folder()).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        if (deletedAt(stamp) <= cutoff)
            discard(stamp.filePath());
    }
    return moved;
}

void VaultTrash::prune(const QString &from) const
{
    const QString trash = folder();
    QString at = QDir::cleanPath(from);
    while (at.startsWith(trash + u'/') && QDir(at).isEmpty(QDir::AllEntries | QDir::NoDotAndDotDot |
                                                           QDir::Hidden | QDir::System)) {
        if (!QDir().rmdir(at))
            break;
        at = QFileInfo(at).path();
    }
}

} // namespace jot
