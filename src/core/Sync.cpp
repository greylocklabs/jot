#include "jot/Sync.h"

#include "jot/FrontMatter.h"
#include "jot/Merge.h"

#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QSaveFile>
#include <QSet>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStringDecoder>
#include <QUuid>

#include <algorithm>

using namespace Qt::StringLiterals;

namespace jot {

namespace {

/// What a sync knew of a file when both sides last held the same.
struct Base
{
    QByteArray hash;
    qint64 size = 0;
    qint64 mtime = 0;
    QString rev;
};

/// A file here as a scan found it.
struct Local
{
    qint64 size = 0;
    qint64 mtime = 0;
};

/// How recently written a file must be for its time to be no proof it is unchanged.
constexpr qint64 kRacyMs = 3000;

QByteArray sha256(const QByteArray &data)
{
    return QCryptographicHash::hash(data, QCryptographicHash::Sha256);
}

/// `data` as text, or nothing when it is not UTF-8.
std::optional<QString> utf8(const QByteArray &data)
{
    QStringDecoder decode(QStringDecoder::Utf8, QStringDecoder::Flag::Stateless |
                                                    QStringDecoder::Flag::ConvertInitialBom);
    QString text = decode(data);
    if (decode.hasError())
        return std::nullopt;
    return text;
}

/// Front matter values as text, for comparing, and whether each can be written back as a key.
struct Keys
{
    QVariantMap values;
    bool simple = true;
};

Keys keysOf(const QString &note)
{
    const NoteParts parts = splitFrontMatter(note);
    Keys keys{readFrontMatter(parts.frontMatter), true};
    // A value kept as raw text, such as a nested map, cannot be written back key by key.
    for (const QVariant &value : std::as_const(keys.values)) {
        if (value.typeId() == QMetaType::QString && value.toString().contains(u'\n'))
            keys.simple = false;
    }
    return keys;
}

} // namespace

struct SyncEngine::Private
{
    QString root;
    std::unique_ptr<Transport> remote;
    QString stateDir;
    QString device;
    QString connection;
    QHash<QString, Base> bases;
    QDateTime now;
    SyncReport report;

    [[nodiscard]] QString absolute(const QString &path) const { return root + u'/' + path; }
    [[nodiscard]] QString blobPath(const QByteArray &hash) const
    {
        return stateDir + u"/base/"_s + QString::fromLatin1(hash.toHex());
    }

    bool open(QString *error);
    void loadBases();
    /// Records `data` as what both sides hold at `path`, the remote as `rev`. `seen` is the
    /// file's size and time when `data` was read from it; without it, `data` was just written
    /// here.
    void keepBase(const QString &path, const QByteArray &data, const QString &rev,
                  const std::optional<Local> &seen = std::nullopt);
    void forgetBase(const QString &path);
    void collectBlobs();

    QHash<QString, Local> scan() const;
    [[nodiscard]] std::optional<Local> stat(const QString &path) const;
    [[nodiscard]] std::optional<QByteArray> readLocal(const QString &path) const;
    bool writeLocal(const QString &path, const QByteArray &data, QString *error) const;
    bool trashLocal(const QString &path, QString *error) const;
    [[nodiscard]] QString conflictPath(const QString &path) const;

    void syncPath(const QString &path, const std::optional<Local> &local,
                  const std::optional<RemoteFile> &remote);
    void push(const QString &path, const QString &expected);
    void pull(const QString &path, const std::optional<Local> &local);
    void bothChanged(const QString &path, const std::optional<Local> &local);
    void fail(const QString &path, const QString &what);
};

SyncEngine::SyncEngine(QString root, std::unique_ptr<Transport> remote, QString stateDir,
                       QString device)
    : d(std::make_unique<Private>())
{
    d->root = QDir(root).absolutePath();
    d->remote = std::move(remote);
    d->stateDir = std::move(stateDir);
    d->device = std::move(device);
    d->connection = u"jot-sync-"_s + QUuid::createUuid().toString(QUuid::WithoutBraces);
}

SyncEngine::~SyncEngine()
{
    if (QSqlDatabase::contains(d->connection)) {
        QSqlDatabase::database(d->connection).close();
        QSqlDatabase::removeDatabase(d->connection);
    }
}

bool SyncEngine::synced(const QString &path)
{
    const QStringList parts = path.split(u'/');
    if (parts.isEmpty() || path.startsWith(u".jot/trash/"_s))
        return false;
    for (qsizetype i = 0; i < parts.size(); ++i) {
        // Hidden names stay here, but for Jot's own folder at the top.
        if (parts.at(i).startsWith(u'.') && !(i == 0 && parts.at(i) == u".jot"))
            return false;
    }
    return true;
}

bool SyncEngine::mergeable(const QString &path)
{
    static const QStringList kText = {u"md"_s,  u"markdown"_s, u"txt"_s, u"yaml"_s,
                                      u"yml"_s, u"json"_s,     u"csv"_s, u"tsv"_s};
    return kText.contains(QFileInfo(path).suffix(), Qt::CaseInsensitive);
}

std::optional<QString> SyncEngine::mergeNote(const QString &base, const QString &mine,
                                             const QString &theirs)
{
    const Keys was = keysOf(base);
    const Keys ours = keysOf(mine);
    const Keys other = keysOf(theirs);
    if (!was.simple || !ours.simple || !other.simple)
        return mergeText(base, mine, theirs);

    // The keys theirs changed and ours did not, to set on ours; both changing one differently
    // is a conflict.
    QVariantMap take;
    QSet<QString> names;
    for (const auto *map : {&was.values, &ours.values, &other.values}) {
        for (auto it = map->cbegin(); it != map->cend(); ++it)
            names.insert(it.key());
    }
    for (const QString &name : std::as_const(names)) {
        const QVariant b = was.values.value(name);
        const QVariant m = ours.values.value(name);
        const QVariant t = other.values.value(name);
        if (m == t || t == b)
            continue;
        if (m != b)
            return std::nullopt;
        take.insert(name, t);
    }

    const NoteParts baseParts = splitFrontMatter(base);
    const NoteParts mineParts = splitFrontMatter(mine);
    const NoteParts theirParts = splitFrontMatter(theirs);
    const std::optional<QString> body =
        mergeText(baseParts.body.toString(), mineParts.body.toString(), theirParts.body.toString());
    if (!body)
        return std::nullopt;
    QString merged =
        QStringView(mine).first(mine.size() - mineParts.body.size()).toString() + *body;
    for (auto it = take.cbegin(); it != take.cend(); ++it)
        merged = setFrontMatterKey(merged, it.key(), it.value());
    return merged;
}

bool SyncEngine::Private::open(QString *error)
{
    if (QSqlDatabase::contains(connection))
        return true;
    if (!QDir().mkpath(stateDir + u"/base"_s)) {
        *error = QObject::tr("could not create %1").arg(stateDir);
        return false;
    }
    QSqlDatabase db = QSqlDatabase::addDatabase(u"QSQLITE"_s, connection);
    db.setDatabaseName(stateDir + u"/state.sqlite"_s);
    if (!db.open()) {
        *error = db.lastError().text();
        return false;
    }
    QSqlQuery query(db);
    if (!query.exec(u"CREATE TABLE IF NOT EXISTS base (path TEXT PRIMARY KEY, hash BLOB, size "
                    u"INTEGER, mtime INTEGER, rev TEXT)"_s)) {
        *error = query.lastError().text();
        return false;
    }
    return true;
}

void SyncEngine::Private::loadBases()
{
    bases.clear();
    QSqlQuery query(u"SELECT path, hash, size, mtime, rev FROM base"_s,
                    QSqlDatabase::database(connection));
    while (query.next())
        bases.insert(query.value(0).toString(),
                     {query.value(1).toByteArray(), query.value(2).toLongLong(),
                      query.value(3).toLongLong(), query.value(4).toString()});
}

void SyncEngine::Private::keepBase(const QString &path, const QByteArray &data, const QString &rev,
                                   const std::optional<Local> &seen)
{
    // The file's size and time stand for `data` only while it is as it was when read, or still
    // holds what was written, and was written long enough ago that a later write would show in
    // its time. Otherwise the base keeps a time of 0, and the next sync reads the file rather
    // than trust it, as after an edit during a slow upload.
    const QByteArray hash = sha256(data);
    const std::optional<Local> here = stat(path);
    bool vouched = false;
    if (here && here->mtime >= QDateTime::currentMSecsSinceEpoch() - kRacyMs) {
        // So recent, another write could share its time: no proof either way.
    } else if (here && seen) {
        vouched = here->size == seen->size && here->mtime == seen->mtime;
    } else if (here) {
        const std::optional<QByteArray> back = readLocal(path);
        vouched = back && sha256(*back) == hash;
    }
    const Base base{hash, vouched ? here->size : data.size(), vouched ? here->mtime : 0, rev};
    // A text file's base is what the next merge starts from.
    if (mergeable(path)) {
        QSaveFile blob(blobPath(base.hash));
        if (!QFileInfo::exists(blob.fileName()) && blob.open(QIODevice::WriteOnly)) {
            blob.write(data);
            blob.commit();
        }
    }
    QSqlQuery query(QSqlDatabase::database(connection));
    query.prepare(u"INSERT OR REPLACE INTO base VALUES (?, ?, ?, ?, ?)"_s);
    query.addBindValue(path);
    query.addBindValue(base.hash);
    query.addBindValue(base.size);
    query.addBindValue(base.mtime);
    query.addBindValue(rev);
    query.exec();
    bases.insert(path, base);
}

void SyncEngine::Private::forgetBase(const QString &path)
{
    QSqlQuery query(QSqlDatabase::database(connection));
    query.prepare(u"DELETE FROM base WHERE path = ?"_s);
    query.addBindValue(path);
    query.exec();
    bases.remove(path);
}

void SyncEngine::Private::collectBlobs()
{
    QSet<QString> kept;
    for (const Base &base : std::as_const(bases))
        kept.insert(QString::fromLatin1(base.hash.toHex()));
    QDir blobs(stateDir + u"/base"_s);
    for (const QString &name : blobs.entryList(QDir::Files)) {
        if (!kept.contains(name))
            blobs.remove(name);
    }
}

QHash<QString, Local> SyncEngine::Private::scan() const
{
    QHash<QString, Local> found;
    QDirIterator files(root, QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot,
                       QDirIterator::Subdirectories);
    while (files.hasNext()) {
        const QFileInfo info = files.nextFileInfo();
        const QString path = info.filePath().mid(root.size() + 1);
        if (synced(path))
            found.insert(path, {info.size(), info.lastModified().toMSecsSinceEpoch()});
    }
    return found;
}

std::optional<Local> SyncEngine::Private::stat(const QString &path) const
{
    const QFileInfo info(absolute(path));
    if (!info.isFile())
        return std::nullopt;
    return Local{info.size(), info.lastModified().toMSecsSinceEpoch()};
}

std::optional<QByteArray> SyncEngine::Private::readLocal(const QString &path) const
{
    QFile file(absolute(path));
    if (!file.open(QIODevice::ReadOnly))
        return std::nullopt;
    return file.readAll();
}

bool SyncEngine::Private::writeLocal(const QString &path, const QByteArray &data,
                                     QString *error) const
{
    const QString target = absolute(path);
    if (!QDir().mkpath(QFileInfo(target).path())) {
        *error = QObject::tr("could not create its folder");
        return false;
    }
    QSaveFile file(target);
    if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit()) {
        *error = file.errorString();
        return false;
    }
    return true;
}

bool SyncEngine::Private::trashLocal(const QString &path, QString *error) const
{
    // A folder for the moment, and another after it when that already holds this path.
    const QString stamp = now.toString(u"yyyy-MM-dd HHmmss"_s);
    QString to = root + u"/.jot/trash/"_s + stamp + u'/' + path;
    for (int n = 2; QFileInfo::exists(to); ++n)
        to = root + u"/.jot/trash/"_s + stamp + u' ' + QString::number(n) + u'/' + path;
    if (!QDir().mkpath(QFileInfo(to).path()) || !QFile::rename(absolute(path), to)) {
        *error = QObject::tr("could not move it to the trash");
        return false;
    }
    return true;
}

QString SyncEngine::Private::conflictPath(const QString &path) const
{
    const QFileInfo info(path);
    const QString folder = path.contains(u'/') ? path.section(u'/', 0, -2) + u'/' : QString();
    const QString suffix = info.suffix().isEmpty() ? QString() : u'.' + info.suffix();
    const QString stem = info.suffix().isEmpty() ? info.fileName() : info.completeBaseName();
    const QString label =
        u"%1 (conflict %2 %3)"_s.arg(stem, device, now.toString(u"yyyy-MM-dd HHmm"_s));
    QString candidate = folder + label + suffix;
    for (int n = 2; QFileInfo::exists(absolute(candidate)); ++n)
        candidate = folder + label + u' ' + QString::number(n) + suffix;
    return candidate;
}

void SyncEngine::Private::fail(const QString &path, const QString &what)
{
    report.errors << u"%1: %2"_s.arg(path, what);
}

void SyncEngine::Private::push(const QString &path, const QString &expected)
{
    const std::optional<Local> seen = stat(path);
    const std::optional<QByteArray> data = readLocal(path);
    if (!data) {
        fail(path, QObject::tr("could not read it"));
        return;
    }
    QString error;
    bool stale = false;
    const std::optional<QString> rev = remote->put(path, *data, expected, &stale, &error);
    if (!rev) {
        fail(path, stale ? QObject::tr("it changed on the remote meanwhile") : error);
        return;
    }
    keepBase(path, *data, *rev, seen);
    report.pushed << path;
}

void SyncEngine::Private::pull(const QString &path, const std::optional<Local> &local)
{
    QString error;
    QString rev;
    const std::optional<QByteArray> data = remote->get(path, &rev, &error);
    if (!data) {
        fail(path, error);
        return;
    }
    // An edit made here since the scan wins this round; the next sync merges it.
    const std::optional<Local> current = stat(path);
    if (current.has_value() != local.has_value() ||
        (current && (current->size != local->size || current->mtime != local->mtime))) {
        fail(path, QObject::tr("it changed here meanwhile"));
        return;
    }
    if (!writeLocal(path, *data, &error)) {
        fail(path, error);
        return;
    }
    keepBase(path, *data, rev);
    report.pulled << path;
}

void SyncEngine::Private::bothChanged(const QString &path, const std::optional<Local> &local)
{
    QString error;
    QString rev;
    const std::optional<QByteArray> theirs = remote->get(path, &rev, &error);
    const std::optional<Local> seen = stat(path);
    const std::optional<QByteArray> mine = readLocal(path);
    if (!theirs || !mine) {
        fail(path, theirs ? QObject::tr("could not read it") : error);
        return;
    }
    if (sha256(*theirs) == sha256(*mine)) {
        keepBase(path, *mine, rev, seen);
        return;
    }

    // Text both changed from a base both had is merged.
    const auto base = bases.constFind(path);
    std::optional<QByteArray> was;
    if (base != bases.cend() && mergeable(path)) {
        QFile blob(blobPath(base->hash));
        if (blob.open(QIODevice::ReadOnly))
            was = blob.readAll();
    }
    const std::optional<QString> baseText = was ? utf8(*was) : std::nullopt;
    const std::optional<QString> mineText = utf8(*mine);
    const std::optional<QString> theirText = utf8(*theirs);
    if (baseText && mineText && theirText) {
        if (const std::optional<QString> merged = mergeNote(*baseText, *mineText, *theirText)) {
            const QByteArray bytes = merged->toUtf8();
            const std::optional<Local> current = stat(path);
            if (!current || current->size != local->size || current->mtime != local->mtime) {
                fail(path, QObject::tr("it changed here meanwhile"));
                return;
            }
            bool stale = false;
            if (!writeLocal(path, bytes, &error)) {
                fail(path, error);
                return;
            }
            const std::optional<QString> newRev = remote->put(path, bytes, rev, &stale, &error);
            if (!newRev) {
                // Here holds the merge; the next sync takes it up as an edit made here.
                fail(path, stale ? QObject::tr("it changed on the remote meanwhile") : error);
                return;
            }
            keepBase(path, bytes, *newRev);
            report.merged << path;
            return;
        }
    }

    // No merge: this side's version stays at the path, and the other is kept beside it, on
    // both sides, so neither is lost.
    const QString copy = conflictPath(path);
    if (!writeLocal(copy, *theirs, &error)) {
        fail(path, error);
        return;
    }
    bool stale = false;
    if (const std::optional<QString> copyRev = remote->put(copy, *theirs, {}, &stale, &error))
        keepBase(copy, *theirs, *copyRev);
    report.conflicts << copy;
    const std::optional<QString> newRev = remote->put(path, *mine, rev, &stale, &error);
    if (!newRev) {
        fail(path, stale ? QObject::tr("it changed on the remote meanwhile") : error);
        return;
    }
    keepBase(path, *mine, *newRev, seen);
}

void SyncEngine::Private::syncPath(const QString &path, const std::optional<Local> &local,
                                   const std::optional<RemoteFile> &remoteFile)
{
    const auto found = bases.constFind(path);
    const std::optional<Base> base =
        found == bases.cend() ? std::nullopt : std::optional<Base>(*found);

    if (!local && !remoteFile) {
        if (base)
            forgetBase(path);
        return;
    }
    if (!base) {
        if (local && !remoteFile)
            push(path, {});
        else if (!local)
            pull(path, local);
        else
            bothChanged(path, local);
        return;
    }

    // Unchanged here when its size and time are as the base left them, or its bytes are. A file
    // written in the last seconds is read anyway, as a second edit within the clock's step
    // would leave its time as it was.
    bool localSame = false;
    if (local) {
        localSame = local->size == base->size && local->mtime == base->mtime &&
                    local->mtime < now.toMSecsSinceEpoch() - kRacyMs;
        if (!localSame) {
            const std::optional<Local> seen = stat(path);
            const std::optional<QByteArray> data = readLocal(path);
            localSame = data && sha256(*data) == base->hash;
            // Touched but not changed: the base takes the new time, so it is not read again.
            if (localSame)
                keepBase(path, *data, base->rev, seen);
        }
    }
    bool remoteSame = false;
    if (remoteFile) {
        remoteSame = remoteFile->rev == base->rev ||
                     (!remoteFile->hash.isEmpty() && remoteFile->hash == base->hash);
        if (remoteSame && remoteFile->rev != base->rev && localSame) {
            const std::optional<Local> seen = stat(path);
            if (const std::optional<QByteArray> data = readLocal(path);
                data && sha256(*data) == base->hash)
                keepBase(path, *data, remoteFile->rev, seen);
        }
    }

    QString error;
    bool stale = false;
    if (local && remoteFile && localSame && remoteSame)
        return;
    if (localSame && !remoteFile) {
        // Deleted there, untouched here.
        if (!trashLocal(path, &error)) {
            fail(path, error);
            return;
        }
        forgetBase(path);
        report.pulled << path;
        return;
    }
    if (!local && remoteSame) {
        // Deleted here, untouched there.
        if (!remote->remove(path, remoteFile->rev, &stale, &error)) {
            fail(path, stale ? QObject::tr("it changed on the remote meanwhile") : error);
            return;
        }
        forgetBase(path);
        report.pushed << path;
        return;
    }
    if (local && !localSame && (remoteSame || !remoteFile)) {
        push(path, remoteFile ? remoteFile->rev : QString());
        return;
    }
    if ((!local || localSame) && remoteFile && !remoteSame) {
        // Changed there; restored here if it was deleted, as an edit beats a delete.
        pull(path, local);
        return;
    }
    bothChanged(path, local);
}

SyncReport SyncEngine::sync(const QDateTime &now)
{
    d->report = {};
    d->now = now;
    QString error;
    if (!d->open(&error)) {
        d->report.failure = error;
        return d->report;
    }
    d->loadBases();
    const std::optional<QList<RemoteFile>> listed = d->remote->list(&error);
    if (!listed) {
        d->report.failure = error;
        return d->report;
    }
    QHash<QString, RemoteFile> remote;
    for (const RemoteFile &file : *listed) {
        if (synced(file.path))
            remote.insert(file.path, file);
    }
    const QHash<QString, Local> local = d->scan();

    QStringList paths = remote.keys() + local.keys() + d->bases.keys();
    std::ranges::sort(paths);
    paths.erase(std::unique(paths.begin(), paths.end()), paths.end());
    for (const QString &path : std::as_const(paths)) {
        const auto here = local.constFind(path);
        const auto there = remote.constFind(path);
        d->syncPath(path, here == local.cend() ? std::nullopt : std::optional<Local>(*here),
                    there == remote.cend() ? std::nullopt : std::optional<RemoteFile>(*there));
    }
    d->collectBlobs();
    return d->report;
}

} // namespace jot
