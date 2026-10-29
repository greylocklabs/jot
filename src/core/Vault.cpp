#include "jot/Vault.h"

#include "jot/FrontMatter.h"
#include "jot/Logging.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStringDecoder>

#include <algorithm>

#include <sys/stat.h>

using namespace Qt::StringLiterals;

namespace jot {

namespace {

const QString kSettingsFolder = u".jot"_s;
const QString kNoteSuffix = u".md"_s;
const QString kAssetsSuffix = u".assets"_s;
const QString kHistoryFolder = u".jot/history"_s;

QString join(const QString &folder, const QString &name)
{
    return folder.isEmpty() ? name : folder + u'/' + name;
}

/// Whether `folder` names a notebook inside the vault: not above it, not absolute, and not
/// in a hidden folder such as `.jot`. Empty is the vault itself.
bool isNotebookPath(const QString &folder)
{
    const QString clean = QDir::cleanPath(folder);
    return folder.isEmpty() ||
           !(clean == u".." || clean.startsWith(u"../"_s) || QDir::isAbsolutePath(clean) ||
             clean.split(u'/').contains(QString()) ||
             std::ranges::any_of(clean.split(u'/'),
                                 [](const QString &part) { return part.startsWith(u'.'); }));
}

} // namespace

int naturalCompare(QStringView a, QStringView b)
{
    // Not QCollator: without ICU, as in minimal systems, it has no numeric mode.
    qsizetype i = 0;
    qsizetype j = 0;
    while (i < a.size() && j < b.size()) {
        if (a[i].isDigit() && b[j].isDigit()) {
            qsizetype endA = i;
            qsizetype endB = j;
            while (endA < a.size() && a[endA].isDigit())
                ++endA;
            while (endB < b.size() && b[endB].isDigit())
                ++endB;
            // Compared as numbers by length once leading zeros are gone, then digit by digit.
            QStringView numberA = a.sliced(i, endA - i);
            QStringView numberB = b.sliced(j, endB - j);
            while (numberA.size() > 1 && numberA.front() == u'0')
                numberA = numberA.sliced(1);
            while (numberB.size() > 1 && numberB.front() == u'0')
                numberB = numberB.sliced(1);
            if (numberA.size() != numberB.size())
                return numberA.size() < numberB.size() ? -1 : 1;
            if (const int order = numberA.compare(numberB); order != 0)
                return order;
            i = endA;
            j = endB;
            continue;
        }
        const QChar ca = a[i].toCaseFolded();
        const QChar cb = b[j].toCaseFolded();
        if (ca != cb)
            return ca < cb ? -1 : 1;
        ++i;
        ++j;
    }
    if (i < a.size() || j < b.size())
        return i < a.size() ? 1 : -1;
    // Equal but for case: a stable order all the same.
    return a.compare(b);
}

Vault::Vault(QString root) : m_root(QDir::cleanPath(std::move(root))) {}

std::optional<Vault> Vault::create(const QString &root, QString *error)
{
    Vault vault(root);
    if (!QDir().mkpath(vault.absolute(kSettingsFolder))) {
        *error = QObject::tr("could not create %1").arg(vault.absolute(kSettingsFolder));
        return std::nullopt;
    }
    const QString settings = vault.absolute(kSettingsFolder + u"/vault.yaml"_s);
    if (!QFileInfo::exists(settings) &&
        !notefile::write(settings, u"format: %1\n"_s.arg(kFormat), error))
        return std::nullopt;
    return vault;
}

bool Vault::exists() const
{
    return QFileInfo(m_root).isDir();
}

int Vault::format() const
{
    QString ignored;
    const std::optional<QString> settings =
        notefile::read(absolute(kSettingsFolder + u"/vault.yaml"_s), &ignored);
    if (!settings)
        return 1;
    bool ok = false;
    const int format = readFrontMatter(*settings).value(u"format"_s).toString().toInt(&ok);
    return ok ? format : 1;
}

QList<VaultEntry> Vault::entries() const
{
    QList<VaultEntry> found;
    collect({}, found);
    return found;
}

QStringList Vault::notes() const
{
    QStringList paths;
    for (const VaultEntry &entry : entries()) {
        if (!entry.folder)
            paths.append(entry.path);
    }
    return paths;
}

void Vault::collect(const QString &folder, QList<VaultEntry> &into) const
{
    const QDir dir(absolute(folder));
    const QFileInfoList all = dir.entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot);
    QSet<QString> noteNames;
    for (const QFileInfo &info : all) {
        if (info.isFile() && info.fileName().endsWith(kNoteSuffix))
            noteNames.insert(info.fileName().chopped(kNoteSuffix.size()));
    }

    QList<QFileInfo> kept;
    for (const QFileInfo &info : all) {
        const QString name = info.fileName();
        if (name.startsWith(u'.'))
            continue;
        if (info.isDir()) {
            // A note's attachments show under the note, not as a notebook.
            if (name.endsWith(kAssetsSuffix) &&
                noteNames.contains(name.chopped(kAssetsSuffix.size())))
                continue;
            kept.append(info);
        } else if (name.endsWith(kNoteSuffix)) {
            kept.append(info);
        }
    }
    std::sort(kept.begin(), kept.end(), [](const QFileInfo &a, const QFileInfo &b) {
        return naturalCompare(a.fileName(), b.fileName()) < 0;
    });

    for (const QFileInfo &info : std::as_const(kept)) {
        const QString path = join(folder, info.fileName());
        into.append({path, info.isDir()});
        // A link to a folder is shown but not followed, so a loop cannot hang the scan.
        if (info.isDir() && !info.isSymLink())
            collect(path, into);
    }
}

QString Vault::absolute(const QString &path) const
{
    return path.isEmpty() ? m_root : m_root + u'/' + path;
}

QString Vault::relative(const QString &absolutePath) const
{
    const QString relative = QDir(m_root).relativeFilePath(absolutePath);
    return relative.startsWith(u"../"_s) || relative == u".." || QDir::isAbsolutePath(relative)
               ? QString()
               : relative;
}

QString Vault::titleFromPath(const QString &path)
{
    const QString name = path.section(u'/', -1);
    return name.endsWith(kNoteSuffix) ? name.chopped(kNoteSuffix.size()) : name;
}

QString Vault::wikilinkName(const QString &path, bool shared)
{
    if (!shared)
        return titleFromPath(path);
    return path.endsWith(kNoteSuffix) ? path.chopped(kNoteSuffix.size()) : path;
}

QString Vault::notebookOf(const QString &path)
{
    return path.contains(u'/') ? path.section(u'/', 0, -2) : QString();
}

QString Vault::fileNameFor(const QString &title)
{
    QString name;
    for (const QChar c : title) {
        if (c == u'/' || c == u'\\')
            name += u" - "_s;
        else if (c.unicode() < 0x20 || c.unicode() == 0x7f || u":*?\"<>|"_s.contains(c))
            continue;
        else
            name += c;
    }
    name = name.simplified();
    qsizetype start = 0;
    qsizetype end = name.size();
    while (start < end && (name[start] == u'.' || name[start].isSpace()))
        ++start;
    while (end > start && (name[end - 1] == u'.' || name[end - 1].isSpace()))
        --end;
    name = name.sliced(start, end - start);
    return name.isEmpty() ? QObject::tr("Untitled") : name;
}

QString Vault::assetsFolderFor(const QString &notePath)
{
    const QString stem =
        notePath.endsWith(kNoteSuffix) ? notePath.chopped(kNoteSuffix.size()) : notePath;
    return stem + kAssetsSuffix;
}

QString Vault::historyFolderFor(const QString &path)
{
    return kHistoryFolder + u'/' + path;
}

namespace {

/// Where an attachment called `fileName` of the note at `notePath` can go without taking
/// another's place, its folder made.
std::optional<QString> attachmentPath(const Vault &vault, const QString &notePath,
                                      const QString &fileName, QString *error)
{
    if (!vault.writable()) {
        *error = QObject::tr("this vault is from a newer Jot, so this one only reads it");
        return std::nullopt;
    }
    const QString folder = Vault::assetsFolderFor(notePath);
    if (!QDir().mkpath(vault.absolute(folder))) {
        *error = QObject::tr("could not create %1").arg(folder);
        return std::nullopt;
    }
    // Only the name: a path in it could reach outside the folder.
    const QFileInfo name(fileName.section(u'/', -1));
    const QString base = Vault::fileNameFor(name.completeBaseName());
    const QString suffix = name.suffix().isEmpty() ? QString() : u'.' + name.suffix();
    QString path = folder + u'/' + base + suffix;
    for (int n = 2; QFileInfo::exists(vault.absolute(path)); ++n)
        path = folder + u'/' + u"%1 %2"_s.arg(base).arg(n) + suffix;
    return path;
}

} // namespace

std::optional<QString> Vault::addAttachment(const QString &notePath, const QString &fileName,
                                            const QByteArray &bytes, QString *error) const
{
    const std::optional<QString> path = attachmentPath(*this, notePath, fileName, error);
    if (!path)
        return std::nullopt;
    QSaveFile file(absolute(*path));
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) {
        *error = file.errorString();
        return std::nullopt;
    }
    return path;
}

std::optional<QString> Vault::copyAttachment(const QString &notePath, const QString &source,
                                             QString *error) const
{
    if (!QFileInfo(source).isFile() || !QFileInfo(source).isReadable()) {
        *error = QObject::tr("%1 cannot be read").arg(QFileInfo(source).fileName());
        return std::nullopt;
    }
    const std::optional<QString> path =
        attachmentPath(*this, notePath, QFileInfo(source).fileName(), error);
    if (!path)
        return std::nullopt;
    QFile file(source);
    if (!file.copy(absolute(*path))) {
        *error = file.errorString();
        return std::nullopt;
    }
    return path;
}

QString Vault::linkTarget(const QString &from, const QString &to)
{
    const QString folder = from.contains(u'/') ? from.section(u'/', 0, -2) : QString();
    QString target = folder.isEmpty() ? to : QDir(u"/"_s + folder).relativeFilePath(u"/"_s + to);
    if (target.contains(u'(') || target.contains(u')'))
        return u'<' + target + u'>';
    return target.replace(u' ', u"%20"_s);
}

QString Vault::freePath(const QString &folder, const QString &title) const
{
    const QString base = fileNameFor(title);
    QString path = join(folder, base + kNoteSuffix);
    for (int n = 2; QFileInfo::exists(absolute(path)); ++n)
        path = join(folder, u"%1 %2"_s.arg(base).arg(n) + kNoteSuffix);
    return path;
}

std::optional<QString> Vault::createNote(const QString &folder, const QString &title,
                                         const QDateTime &created, QString *error) const
{
    if (!writable()) {
        *error = QObject::tr("this vault is from a newer Jot, so this one only reads it");
        return std::nullopt;
    }
    if (!isNotebookPath(folder)) {
        *error = QObject::tr("%1 is not a notebook in this vault").arg(folder);
        return std::nullopt;
    }
    if (!QDir().mkpath(absolute(folder))) {
        *error = QObject::tr("could not create %1").arg(absolute(folder));
        return std::nullopt;
    }
    const QString path = freePath(folder, title);
    QString text;
    if (fileNameFor(title) != title)
        text = setFrontMatterKey(text, u"title"_s, title);
    text = setFrontMatterKey(text, u"created"_s, created.toUTC().toString(Qt::ISODate));
    if (!notefile::write(absolute(path), text, error))
        return std::nullopt;
    return path;
}

bool Vault::moveToTrash(const QString &relative, const QString &stamp) const
{
    // Where the desktop has no trash, as on some removable or network drives, the vault
    // keeps its own, so nothing is deleted outright.
    const QString file = absolute(relative);
    if (m_systemTrash && QFile::moveToTrash(file))
        return true;
    const QString folder = absolute(kSettingsFolder + u"/trash/"_s + stamp);
    const QString name = relative.section(u'/', -1);
    QString target = folder + u'/' + name;
    // Two of one name trashed in the same second keep both.
    for (int n = 2; QFileInfo::exists(target); ++n)
        target = folder + u'/' + u"%1 %2"_s.arg(n).arg(name);
    return QDir().mkpath(folder) && QDir().rename(file, target);
}

bool Vault::trashFolder(const QString &path, QString *error) const
{
    if (!writable()) {
        *error = QObject::tr("this vault is from a newer Jot, so this one only reads it");
        return false;
    }
    if (path.isEmpty() || !isNotebookPath(path) || !QFileInfo(absolute(path)).isDir()) {
        *error = QObject::tr("%1 is not a notebook in this vault").arg(path);
        return false;
    }
    if (!moveToTrash(path, QDateTime::currentDateTime().toString(u"yyyyMMdd-HHmmss"_s))) {
        *error = QObject::tr("could not move %1 to the trash").arg(path);
        return false;
    }
    return true;
}

bool Vault::trashNote(const QString &path, QString *error, bool keepAttachments) const
{
    const QString stamp = QDateTime::currentDateTime().toString(u"yyyyMMdd-HHmmss"_s);
    const auto toTrash = [this, &stamp](const QString &relative) {
        return moveToTrash(relative, stamp);
    };
    if (!toTrash(path)) {
        *error = QObject::tr("could not move %1 to the trash").arg(path);
        return false;
    }
    if (!keepAttachments && QFileInfo(absolute(assetsFolderFor(path))).isDir() &&
        !toTrash(assetsFolderFor(path))) {
        *error = QObject::tr("could not move %1 to the trash").arg(assetsFolderFor(path));
        return false;
    }
    return true;
}

bool Vault::moveNote(const QString &from, const QString &to, QString *error) const
{
    if (!writable()) {
        *error = QObject::tr("this vault is from a newer Jot, so this one only reads it");
        return false;
    }
    if (QFileInfo::exists(absolute(to))) {
        *error = QObject::tr("%1 already exists").arg(to);
        return false;
    }
    const QString toFolder = to.contains(u'/') ? to.section(u'/', 0, -2) : QString();
    if (!QDir().mkpath(absolute(toFolder)) || !QFile::rename(absolute(from), absolute(to))) {
        *error = QObject::tr("could not move %1 to %2").arg(from, to);
        return false;
    }
    const QString assets = absolute(assetsFolderFor(from));
    if (QFileInfo(assets).isDir() && !QDir().rename(assets, absolute(assetsFolderFor(to)))) {
        *error = QObject::tr("moved %1, but not its attachments").arg(from);
        return false;
    }
    // The note has moved, so links to it must follow whatever becomes of its history.
    if (!moveHistory(from, to))
        qCWarning(lcVault) << "moved" << from << "to" << to << "but not its history";
    return true;
}

std::optional<QString> Vault::createFolder(const QString &parent, const QString &name,
                                           QString *error) const
{
    if (!writable()) {
        *error = QObject::tr("this vault is from a newer Jot, so this one only reads it");
        return std::nullopt;
    }
    const QString base = fileNameFor(name.trimmed()).remove(u'/');
    if (base.isEmpty() || base.startsWith(u'.') || !isNotebookPath(parent)) {
        *error = QObject::tr("%1 cannot be a notebook").arg(name);
        return std::nullopt;
    }
    QString path = join(parent, base);
    for (int n = 2; QFileInfo::exists(absolute(path)); ++n)
        path = join(parent, u"%1 %2"_s.arg(base).arg(n));
    if (!QDir().mkpath(absolute(path))) {
        *error = QObject::tr("could not create %1").arg(path);
        return std::nullopt;
    }
    return path;
}

bool Vault::moveFolder(const QString &from, const QString &to, QString *error) const
{
    if (!writable()) {
        *error = QObject::tr("this vault is from a newer Jot, so this one only reads it");
        return false;
    }
    if (from.isEmpty() || to.isEmpty() || !isNotebookPath(from) || !isNotebookPath(to)) {
        *error = QObject::tr("%1 cannot be moved to %2").arg(from, to);
        return false;
    }
    if (to == from || to.startsWith(from + u'/')) {
        *error = QObject::tr("%1 cannot go inside itself").arg(from);
        return false;
    }
    if (QFileInfo::exists(absolute(to))) {
        *error = QObject::tr("%1 already exists").arg(to);
        return false;
    }
    const QString toParent = to.contains(u'/') ? to.section(u'/', 0, -2) : QString();
    if (!QDir().mkpath(absolute(toParent)) || !QDir().rename(absolute(from), absolute(to))) {
        *error = QObject::tr("could not move %1 to %2").arg(from, to);
        return false;
    }
    if (!moveHistory(from, to))
        qCWarning(lcVault) << "moved" << from << "to" << to << "but not the history of its notes";
    return true;
}

bool Vault::moveHistory(const QString &from, const QString &to) const
{
    const QString source = absolute(historyFolderFor(from));
    if (!QFileInfo(source).isDir())
        return true;
    const QString target = absolute(historyFolderFor(to));
    if (!QFileInfo::exists(target))
        return QDir().mkpath(QFileInfo(target).path()) && QDir().rename(source, target);
    // History left by a note deleted from there joins this one's, file by file.
    QDirIterator files(source, QDir::Files | QDir::Hidden, QDirIterator::Subdirectories);
    while (files.hasNext()) {
        const QString file = files.next();
        const QString relativePath = QDir(source).relativeFilePath(file);
        const QFileInfo wanted(target + u'/' + relativePath);
        // A version's name is its time and, after the first in a second, a number; a clash
        // takes the next free number, so the name still reads as a version.
        static const QRegularExpression numbered(u"^(\\d{4}-\\d\\d-\\d\\d \\d{6}) \\d+$"_s);
        const QRegularExpressionMatch version = numbered.match(wanted.completeBaseName());
        const QString base = version.hasMatch() ? version.captured(1) : wanted.completeBaseName();
        QString path = wanted.filePath();
        for (int n = 2; QFileInfo::exists(path); ++n)
            path = u"%1/%2 %3.%4"_s.arg(wanted.path(), base).arg(n).arg(wanted.suffix());
        if (!QDir().mkpath(wanted.path()) || !QFile::rename(file, path))
            return false;
    }
    return QDir(source).removeRecursively();
}

namespace notefile {

std::optional<QString> read(const QString &path, QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        *error = file.errorString();
        return std::nullopt;
    }
    const QByteArray bytes = file.readAll();
    QStringDecoder decode(QStringDecoder::Utf8, QStringDecoder::Flag::Stateless);
    QString text = decode(bytes);
    if (decode.hasError()) {
        *error = QObject::tr("%1 is not UTF-8 text").arg(path);
        return std::nullopt;
    }
    // Qt drops a byte order mark when decoding; the note keeps the one it had.
    if (bytes.startsWith("\xef\xbb\xbf") && !text.startsWith(QChar(0xfeff)))
        text.prepend(QChar(0xfeff));
    return text;
}

bool write(const QString &path, const QString &text, QString *error)
{
    const QFileInfo info(path);
    // Writing to the link's target keeps the link; a new file in its place would not.
    const QString target = info.isSymLink() ? info.symLinkTarget() : path;
    struct stat status{};
    const bool hardLinked =
        ::stat(QFile::encodeName(target).constData(), &status) == 0 && status.st_nlink > 1;
    const QByteArray bytes = text.toUtf8();

    if (hardLinked) {
        QFile file(target);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate) ||
            file.write(bytes) != bytes.size() || !file.flush()) {
            *error = file.errorString();
            return false;
        }
        return true;
    }
    QSaveFile file(target);
    file.setDirectWriteFallback(false);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) {
        *error = file.errorString();
        return false;
    }
    return true;
}

} // namespace notefile

} // namespace jot
