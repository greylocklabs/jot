#include "jot/VaultTrash.h"

#include "jot/Logging.h"
#include "jot/Vault.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QUrl>

#include <algorithm>

using namespace Qt::StringLiterals;

namespace jot {

namespace {

// How far apart a note and its attachments can go to the desktop trash and still be paired.
constexpr qint64 kPairSeconds = 60;

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

struct Record
{
    QString path;
    QDateTime deleted;
    QString file;
    QString info;
};

// A desktop trash record: where the item was and when it went.
std::optional<Record> readRecord(const QString &info)
{
    QFile file(info);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return std::nullopt;
    Record record;
    record.info = info;
    while (!file.atEnd()) {
        const QByteArray line = file.readLine().trimmed();
        if (line.startsWith("Path="))
            record.path = QUrl::fromPercentEncoding(line.mid(5));
        else if (line.startsWith("DeletionDate="))
            record.deleted = QDateTime::fromString(QString::fromUtf8(line.mid(13)), Qt::ISODate);
    }
    if (record.path.isEmpty())
        return std::nullopt;
    return record;
}

} // namespace

VaultTrash::VaultTrash(const Vault &vault, QString desktopTrash)
    : m_vault(vault), m_desktopTrash(std::move(desktopTrash))
{}

QString VaultTrash::homeTrash()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + u"/Trash"_s;
}

QList<TrashedNote> VaultTrash::list() const
{
    QList<TrashedNote> found;
    listDesktop(found);
    listOwn(found);
    std::ranges::stable_sort(
        found, [](const TrashedNote &a, const TrashedNote &b) { return a.deleted > b.deleted; });
    return found;
}

void VaultTrash::listDesktop(QList<TrashedNote> &into) const
{
    if (m_desktopTrash.isEmpty())
        return;
    const QString root = m_vault.root() + u'/';
    QList<Record> assets;
    QList<qsizetype> notes;
    const QDir infos(m_desktopTrash + u"/info"_s);
    for (const QFileInfo &info : infos.entryInfoList({u"*.trashinfo"_s}, QDir::Files)) {
        std::optional<Record> record = readRecord(info.filePath());
        // Resolved first, so a path that climbs out with ".." is not taken for one inside.
        if (record)
            record->path = QDir::cleanPath(record->path);
        if (!record || !record->path.startsWith(root))
            continue;
        const QString relative = record->path.mid(root.size());
        if (relative.startsWith(u".jot/"_s))
            continue;
        record->path = relative;
        record->file = m_desktopTrash + u"/files/"_s + info.completeBaseName();
        const QFileInfo trashed(record->file);
        if (!trashed.exists())
            continue;
        if (trashed.isDir() && relative.endsWith(u".assets"_s)) {
            assets << *record;
        } else if (trashed.isDir()) {
            // A notebook: each note in it can come back on its own.
            for (const QString &inner : notesIn(record->file)) {
                const QString assetsInside = record->file + u'/' + Vault::assetsFolderFor(inner);
                into << TrashedNote{record->file + u'/' + inner,
                                    relative + u'/' + inner,
                                    record->deleted,
                                    QFileInfo(assetsInside).isDir() ? assetsInside : QString(),
                                    {},
                                    {}};
            }
        } else if (Vault::isListed(relative)) {
            notes << into.size();
            into << TrashedNote{record->file, relative, record->deleted, {}, record->info, {}};
        }
    }
    // A note's attachments go to the trash on their own, a moment after it.
    for (const qsizetype index : std::as_const(notes)) {
        TrashedNote &note = into[index];
        const QString wanted = Vault::assetsFolderFor(note.path);
        const auto match = std::ranges::find_if(assets, [&](const Record &record) {
            return record.path == wanted &&
                   std::abs(record.deleted.secsTo(note.deleted)) <= kPairSeconds;
        });
        if (match == assets.end())
            continue;
        note.assets = match->file;
        note.assetsRecord = match->info;
        assets.erase(match);
    }
}

void VaultTrash::listOwn(QList<TrashedNote> &into) const
{
    const QDir trash(m_vault.absolute(u".jot/trash"_s));
    for (const QFileInfo &stamp : trash.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        QDateTime deleted = stampTime(stamp.fileName());
        if (!deleted.isValid())
            deleted = stamp.lastModified();
        const QString folder = stamp.filePath();
        for (const QString &relative : notesIn(folder)) {
            const QString assets = folder + u'/' + Vault::assetsFolderFor(relative);
            into << TrashedNote{folder + u'/' + relative,
                                relative,
                                deleted,
                                QFileInfo(assets).isDir() ? assets : QString(),
                                {},
                                {}};
        }
    }
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
    bool assetsBack = true;
    if (!note.assets.isEmpty() && QFileInfo(note.assets).isDir() &&
        !QDir().rename(note.assets, m_vault.absolute(Vault::assetsFolderFor(path)))) {
        assetsBack = false;
        qCWarning(lcVault) << "could not bring the attachments of" << path << "back";
        *error = QObject::tr("%1 is back, but its attachments are still in the trash").arg(path);
    }
    if (!note.record.isEmpty())
        QFile::remove(note.record);
    if (assetsBack && !note.assetsRecord.isEmpty())
        QFile::remove(note.assetsRecord);
    return path;
}

} // namespace jot
