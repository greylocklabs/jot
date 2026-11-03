#include "FakeTransport.h"

#include "jot/Conflicts.h"
#include "jot/FolderTransport.h"
#include "jot/Sync.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QRandomGenerator>
#include <QSet>
#include <QTemporaryDir>
#include <QTest>

using namespace Qt::StringLiterals;
using jot::SyncEngine;
using jot::SyncReport;

class TestSync : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void init();
    void cleanup();
    void sendsAndBringsNewFiles();
    void deletesGoToTheTrash();
    void anEditBeatsADelete();
    void mergesEditsToDifferentLines();
    void mergesFrontMatterKeyByKey();
    void keepsAConflictCopyWhenEditsClash();
    void keepsBothVersionsOfAnAttachment();
    void syncsOnlyTheVaultsOwnFiles();
    void triesAgainWhenTheRemoteChangesMeanwhile();
    void sendsAnEditMadeDuringAnUpload();
    void doesNothingWhenTheRemoteIsDown();
    void refusesARemoteThatIsNotTheOneSyncedWith();
    void syncsThroughAFolder();
    void readsRemoteSettings();
    void readsConflictCopyNames();
    void keepsEverythingAcrossRandomEdits_data();
    void keepsEverythingAcrossRandomEdits();

private:
    /// A computer with its own copy of the vault, syncing with the shared remote.
    struct Device
    {
        QString root;
        std::unique_ptr<SyncEngine> engine;
    };
    Device device(const QString &name);
    static void write(const Device &device, const QString &path, const QByteArray &data);
    static QByteArray read(const Device &device, const QString &path);
    static bool exists(const Device &device, const QString &path);
    /// Every synced file and its bytes; `trash` gives what syncs moved to the trash instead.
    static QMap<QString, QByteArray> files(const Device &device, bool trash = false);
    /// What the remote holds, but for its marker.
    QStringList remoteFiles() const
    {
        QStringList paths = m_remote->files.keys();
        paths.removeAll(SyncEngine::kMarker.toString());
        return paths;
    }
    /// Syncs each device in turn until a round changes nothing.
    void settle(QList<Device *> devices);

    std::unique_ptr<QTemporaryDir> m_dir;
    std::shared_ptr<FakeRemote> m_remote;
};

TestSync::Device TestSync::device(const QString &name)
{
    const QString root = m_dir->filePath(name + u"/vault"_s);
    QDir().mkpath(root);
    return {root, std::make_unique<SyncEngine>(root, std::make_unique<FakeTransport>(m_remote),
                                               m_dir->filePath(name + u"/state"_s), name)};
}

void TestSync::write(const Device &device, const QString &path, const QByteArray &data)
{
    const QString file = device.root + u'/' + path;
    QDir().mkpath(QFileInfo(file).path());
    QFile out(file);
    QVERIFY(out.open(QIODevice::WriteOnly));
    out.write(data);
}

QByteArray TestSync::read(const Device &device, const QString &path)
{
    QFile in(device.root + u'/' + path);
    return in.open(QIODevice::ReadOnly) ? in.readAll() : QByteArray();
}

bool TestSync::exists(const Device &device, const QString &path)
{
    return QFileInfo::exists(device.root + u'/' + path);
}

QMap<QString, QByteArray> TestSync::files(const Device &device, bool trash)
{
    QMap<QString, QByteArray> found;
    QDirIterator it(device.root, QDir::Files | QDir::Hidden, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString path = it.next().mid(device.root.size() + 1);
        const bool trashed = path.startsWith(u".jot/trash/"_s);
        if (trashed == trash && (trash || SyncEngine::synced(path)))
            found.insert(path, read(device, path));
    }
    return found;
}

void TestSync::settle(QList<Device *> devices)
{
    for (int round = 0; round < 6; ++round) {
        bool quiet = true;
        for (Device *each : devices) {
            const SyncReport report = each->engine->sync();
            QVERIFY2(report.failure.isEmpty(), qPrintable(report.failure));
            QVERIFY2(report.errors.isEmpty(), qPrintable(report.errors.join(u"; ")));
            quiet = quiet && report.pushed.isEmpty() && report.pulled.isEmpty() &&
                    report.merged.isEmpty() && report.conflicts.isEmpty();
        }
        if (quiet && round > 0)
            return;
    }
    QFAIL("the devices did not settle");
}

void TestSync::init()
{
    m_dir = std::make_unique<QTemporaryDir>();
    m_remote = std::make_shared<FakeRemote>();
}

void TestSync::cleanup()
{
    m_remote.reset();
    m_dir.reset();
}

void TestSync::sendsAndBringsNewFiles()
{
    Device laptop = device(u"laptop"_s);
    Device desk = device(u"desk"_s);
    write(laptop, u"Ashfall/Mara.md"_s, "# Mara\n");
    write(desk, u"Inbox.md"_s, "- [ ] rope\n");
    settle({&laptop, &desk});
    QCOMPARE(read(desk, u"Ashfall/Mara.md"_s), QByteArray("# Mara\n"));
    QCOMPARE(read(laptop, u"Inbox.md"_s), QByteArray("- [ ] rope\n"));
    QCOMPARE(remoteFiles().size(), 2);

    // The same new file on both, before either synced, is no conflict.
    write(laptop, u"Same.md"_s, "same\n");
    write(desk, u"Same.md"_s, "same\n");
    settle({&laptop, &desk});
    QCOMPARE(files(laptop), files(desk));
    QCOMPARE(files(laptop).size(), 3);
}

void TestSync::deletesGoToTheTrash()
{
    Device laptop = device(u"laptop"_s);
    Device desk = device(u"desk"_s);
    write(laptop, u"Old.md"_s, "old\n");
    settle({&laptop, &desk});
    QVERIFY(QFile::remove(laptop.root + u"/Old.md"_s));
    settle({&laptop, &desk});
    QVERIFY(!exists(desk, u"Old.md"_s));
    QVERIFY(remoteFiles().isEmpty());
    // Kept in the trash here and there, not gone.
    QCOMPARE(files(desk, true).values(), QList<QByteArray>{"old\n"});
    QVERIFY(files(desk, true).firstKey().endsWith(u"/Old.md"_s));
    QCOMPARE(m_remote->trash.value(u"Old.md"_s), QByteArray("old\n"));
}

void TestSync::anEditBeatsADelete()
{
    Device laptop = device(u"laptop"_s);
    Device desk = device(u"desk"_s);
    write(laptop, u"Mara.md"_s, "one\n");
    write(laptop, u"Soren.md"_s, "one\n");
    settle({&laptop, &desk});
    // Deleted here and edited there, either way round.
    QVERIFY(QFile::remove(laptop.root + u"/Mara.md"_s));
    write(desk, u"Mara.md"_s, "one\ntwo\n");
    write(laptop, u"Soren.md"_s, "one\nthree\n");
    QVERIFY(QFile::remove(desk.root + u"/Soren.md"_s));
    settle({&laptop, &desk});
    QCOMPARE(read(laptop, u"Mara.md"_s), QByteArray("one\ntwo\n"));
    QCOMPARE(read(desk, u"Soren.md"_s), QByteArray("one\nthree\n"));
    QCOMPARE(files(laptop), files(desk));
}

void TestSync::mergesEditsToDifferentLines()
{
    Device laptop = device(u"laptop"_s);
    Device desk = device(u"desk"_s);
    write(laptop, u"Mara.md"_s, "# Mara\n\nfirst\n\nmiddle\n\nlast\n");
    settle({&laptop, &desk});
    write(laptop, u"Mara.md"_s, "# Mara\n\nFIRST\n\nmiddle\n\nlast\n");
    write(desk, u"Mara.md"_s, "# Mara\n\nfirst\n\nmiddle\n\nLAST\n");
    QCOMPARE(laptop.engine->sync().pushed, QStringList{u"Mara.md"_s});
    QCOMPARE(desk.engine->sync().merged, QStringList{u"Mara.md"_s});
    settle({&laptop, &desk});
    QCOMPARE(read(laptop, u"Mara.md"_s), QByteArray("# Mara\n\nFIRST\n\nmiddle\n\nLAST\n"));
    QCOMPARE(files(laptop), files(desk));
}

void TestSync::mergesFrontMatterKeyByKey()
{
    // Lines next to each other changed on both sides: a line merge gives up, keys do not.
    const QString base = u"---\ntitle: Mara\ntask: open\ndue: 2026-11-02\n---\nBody\n"_s;
    const QString mine = u"---\ntitle: Mara\ntask: done\ndue: 2026-11-02\n---\nBody\n"_s;
    const QString theirs = u"---\ntitle: Mara\ntask: open\ndue: 2026-11-05\n---\nBody\nMore\n"_s;
    QCOMPARE(SyncEngine::mergeNote(base, mine, theirs),
             std::optional(u"---\ntitle: Mara\ntask: done\ndue: 2026-11-05\n---\nBody\nMore\n"_s));
    // A key added on one side, and one taken away on the other.
    QCOMPARE(SyncEngine::mergeNote(u"---\na: 1\nb: 2\n---\n"_s, u"---\na: 1\nb: 2\nc: 3\n---\n"_s,
                                   u"---\na: 1\n---\n"_s),
             std::optional(u"---\na: 1\nc: 3\n---\n"_s));
    // A comment written on one side and the body changed on the other both stay.
    QCOMPARE(SyncEngine::mergeNote(u"---\ntask: open\n---\nBody\n"_s,
                                   u"---\ntask: open\n---\nBody\nMine\n"_s,
                                   u"---\n# why\ntask: open\n---\nBody\n"_s),
             std::optional(u"---\n# why\ntask: open\n---\nBody\nMine\n"_s));
    // Beside a key changed on the other side, where only the keys could be merged, a comment
    // would be lost: that is a conflict.
    QCOMPARE(SyncEngine::mergeNote(u"---\ntask: open\n---\n"_s, u"---\ntask: done\n---\n"_s,
                                   u"---\n# why\ntask: open\n---\n"_s),
             std::nullopt);
    // The same key changed differently is a conflict.
    QCOMPARE(SyncEngine::mergeNote(base, mine,
                                   u"---\ntitle: Mara\ntask: cancelled\ndue: 2026-11-02\n---\n"
                                   "Body\n"_s),
             std::nullopt);
}

void TestSync::keepsAConflictCopyWhenEditsClash()
{
    Device laptop = device(u"laptop"_s);
    Device desk = device(u"desk"_s);
    write(laptop, u"Ashfall/Mara.md"_s, "line\n");
    settle({&laptop, &desk});
    write(laptop, u"Ashfall/Mara.md"_s, "laptop's line\n");
    write(desk, u"Ashfall/Mara.md"_s, "desk's line\n");
    laptop.engine->sync();
    const SyncReport report = desk.engine->sync(QDateTime(QDate(2026, 11, 3), QTime(9, 30)));
    QCOMPARE(report.conflicts, QStringList{u"Ashfall/Mara (conflict desk 2026-11-03 0930).md"_s});
    settle({&laptop, &desk});
    // Each side keeps its own at the note and the other beside it.
    QCOMPARE(read(desk, u"Ashfall/Mara.md"_s), QByteArray("desk's line\n"));
    QCOMPARE(read(desk, report.conflicts.first()), QByteArray("laptop's line\n"));
    QCOMPARE(files(laptop), files(desk));
}

void TestSync::keepsBothVersionsOfAnAttachment()
{
    Device laptop = device(u"laptop"_s);
    Device desk = device(u"desk"_s);
    write(laptop, u"Mara.assets/sketch.png"_s, QByteArray("\x89PNG one", 8));
    settle({&laptop, &desk});
    write(laptop, u"Mara.assets/sketch.png"_s, QByteArray("\x89PNG two", 8));
    write(desk, u"Mara.assets/sketch.png"_s, QByteArray("\x89PNG three", 10));
    settle({&laptop, &desk});
    const QMap<QString, QByteArray> all = files(laptop);
    QCOMPARE(all.size(), 2);
    QVERIFY(all.values().contains(QByteArray("\x89PNG two", 8)));
    QVERIFY(all.values().contains(QByteArray("\x89PNG three", 10)));
    QVERIFY(all.lastKey().endsWith(u").png"_s) || all.firstKey().endsWith(u").png"_s));
    QCOMPARE(files(desk), all);
}

void TestSync::syncsOnlyTheVaultsOwnFiles()
{
    QVERIFY(SyncEngine::synced(u"Mara.md"_s));
    QVERIFY(SyncEngine::synced(u".jot/layout.yaml"_s));
    QVERIFY(SyncEngine::synced(u".jot/history/Mara.md/2026-11-03 093000.md"_s));
    QVERIFY(!SyncEngine::synced(u".jot/trash/2026-11-03 093000/Mara.md"_s));
    QVERIFY(!SyncEngine::synced(u".git/config"_s));
    QVERIFY(!SyncEngine::synced(u"Ashfall/.DS_Store"_s));
    QVERIFY(!SyncEngine::synced(u"Ashfall/.hidden/x.md"_s));

    Device laptop = device(u"laptop"_s);
    write(laptop, u".git/HEAD"_s, "ref\n");
    write(laptop, u".jot/vault.yaml"_s, "format: 1\n");
    laptop.engine->sync();
    QCOMPARE(remoteFiles(), QStringList{u".jot/vault.yaml"_s});
}

void TestSync::triesAgainWhenTheRemoteChangesMeanwhile()
{
    Device laptop = device(u"laptop"_s);
    Device desk = device(u"desk"_s);
    write(laptop, u"Mara.md"_s, "one\n");
    settle({&laptop, &desk});
    write(laptop, u"Mara.md"_s, "one\nlaptop\n");
    // Another device writes just before this one does.
    m_remote->beforePut = [this](const QString &path) {
        m_remote->beforePut = nullptr;
        m_remote->files.insert(path, {"elsewhere\none\n", m_remote->nextRev()});
    };
    const SyncReport report = laptop.engine->sync();
    QCOMPARE(report.errors.size(), 1);
    QVERIFY(report.pushed.isEmpty());
    // The next sync sees the other write and merges with it.
    settle({&laptop, &desk});
    QCOMPARE(read(laptop, u"Mara.md"_s), QByteArray("elsewhere\none\nlaptop\n"));
    QCOMPARE(files(laptop), files(desk));
}

void TestSync::sendsAnEditMadeDuringAnUpload()
{
    Device laptop = device(u"laptop"_s);
    write(laptop, u"Mara.md"_s, "one\n");
    // Saved again while the first version is on its way, with the same length.
    m_remote->beforePut = [&laptop](const QString &path) {
        if (path == u"Mara.md")
            write(laptop, u"Mara.md"_s, "two\n");
    };
    QCOMPARE(laptop.engine->sync().pushed, QStringList{u"Mara.md"_s});
    m_remote->beforePut = nullptr;
    QCOMPARE(m_remote->files.value(u"Mara.md"_s).data, QByteArray("one\n"));
    // Later, when a file's time alone would pass for unchanged, the edit still goes.
    const QDateTime later = QDateTime::currentDateTime().addSecs(60);
    QCOMPARE(laptop.engine->sync(later).pushed, QStringList{u"Mara.md"_s});
    QCOMPARE(m_remote->files.value(u"Mara.md"_s).data, QByteArray("two\n"));

    // The same with times long past, where only the time changing tells the edit.
    const auto age = [&laptop](const QString &path, int days) {
        QFile file(laptop.root + u'/' + path);
        QVERIFY(file.open(QIODevice::ReadWrite));
        QVERIFY(file.setFileTime(QDateTime::currentDateTime().addDays(-days),
                                 QFileDevice::FileModificationTime));
    };
    write(laptop, u"Soren.md"_s, "one\n");
    age(u"Soren.md"_s, 3);
    m_remote->beforePut = [&](const QString &path) {
        if (path != u"Soren.md")
            return;
        write(laptop, u"Soren.md"_s, "two\n");
        age(u"Soren.md"_s, 2);
    };
    QCOMPARE(laptop.engine->sync().pushed, QStringList{u"Soren.md"_s});
    m_remote->beforePut = nullptr;
    QCOMPARE(laptop.engine->sync(later).pushed, QStringList{u"Soren.md"_s});
    QCOMPARE(m_remote->files.value(u"Soren.md"_s).data, QByteArray("two\n"));
}

void TestSync::doesNothingWhenTheRemoteIsDown()
{
    Device laptop = device(u"laptop"_s);
    write(laptop, u"Mara.md"_s, "one\n");
    m_remote->down = true;
    const SyncReport report = laptop.engine->sync();
    QCOMPARE(report.failure, u"unreachable"_s);
    QVERIFY(report.pushed.isEmpty());
    m_remote->down = false;
    QCOMPARE(laptop.engine->sync().pushed, QStringList{u"Mara.md"_s});
}

void TestSync::refusesARemoteThatIsNotTheOneSyncedWith()
{
    Device laptop = device(u"laptop"_s);
    Device desk = device(u"desk"_s);
    write(laptop, u"Mara.md"_s, "one\n");
    settle({&laptop, &desk});
    QVERIFY(m_remote->files.contains(SyncEngine::kMarker.toString()));
    // Deleting the last note empties the remote but keeps its mark, and goes through.
    QVERIFY(QFile::remove(laptop.root + u"/Mara.md"_s));
    settle({&laptop, &desk});
    QVERIFY(!exists(desk, u"Mara.md"_s));
    write(desk, u"Soren.md"_s, "two\n");
    settle({&laptop, &desk});

    // Without its mark, as an empty mount point, or with another's, nothing changes here.
    const FakeRemote::File mark = m_remote->files.take(SyncEngine::kMarker.toString());
    const QMap<QString, FakeRemote::File> held = m_remote->files;
    m_remote->files.clear();
    QVERIFY(!laptop.engine->sync().failure.isEmpty());
    QVERIFY(exists(laptop, u"Soren.md"_s));
    m_remote->files.insert(SyncEngine::kMarker.toString(), {"someone else's\n", u"x"_s});
    QVERIFY(laptop.engine->sync().failure.contains(u"not the remote"_s));
    QVERIFY(exists(laptop, u"Soren.md"_s));
    m_remote->files = held;
    m_remote->files.insert(SyncEngine::kMarker.toString(), mark);
    QVERIFY(laptop.engine->sync().failure.isEmpty());
}

void TestSync::syncsThroughAFolder()
{
    const QString shared = m_dir->filePath(u"nas/Notes"_s);
    QVERIFY(QDir().mkpath(shared));
    const auto onFolder = [&](const QString &name) {
        const QString root = m_dir->filePath(name + u"/vault"_s);
        QDir().mkpath(root);
        return Device{
            root, std::make_unique<SyncEngine>(root, std::make_unique<jot::FolderTransport>(shared),
                                               m_dir->filePath(name + u"/state"_s), name)};
    };
    Device laptop = onFolder(u"laptop"_s);
    Device desk = onFolder(u"desk"_s);
    write(laptop, u"Ashfall/Mara.md"_s, "# Mara\n\none\n\ntwo\n");
    settle({&laptop, &desk});
    QFile copy(shared + u"/Ashfall/Mara.md"_s);
    QVERIFY(copy.open(QIODevice::ReadOnly));
    QCOMPARE(copy.readAll(), QByteArray("# Mara\n\none\n\ntwo\n"));

    // Edits to different lines on each merge there too; a delete goes to the folder's trash.
    write(laptop, u"Ashfall/Mara.md"_s, "# Mara\n\nONE\n\ntwo\n");
    write(desk, u"Ashfall/Mara.md"_s, "# Mara\n\none\n\nTWO\n");
    write(desk, u"Gone.md"_s, "gone\n");
    settle({&laptop, &desk});
    QCOMPARE(read(desk, u"Ashfall/Mara.md"_s), QByteArray("# Mara\n\nONE\n\nTWO\n"));
    QVERIFY(QFile::remove(desk.root + u"/Gone.md"_s));
    settle({&laptop, &desk});
    QVERIFY(!exists(laptop, u"Gone.md"_s));
    QVERIFY(!QFileInfo::exists(shared + u"/Gone.md"_s));
    QVERIFY(QDir(shared + u"/.jot/trash"_s).exists());
    QCOMPARE(files(laptop), files(desk));

    // An unmounted share whose mount point stays behind, empty, changes nothing either.
    const QMap<QString, QByteArray> before = files(laptop);
    const QString aside = m_dir->filePath(u"aside"_s);
    QVERIFY(QDir().rename(shared, aside));
    QVERIFY(QDir().mkpath(shared));
    QVERIFY(!laptop.engine->sync().failure.isEmpty());
    QCOMPARE(files(laptop), before);
    QVERIFY(QDir(shared).removeRecursively());
    QVERIFY(QDir().rename(aside, shared));

    // An unmounted share is no remote at all, not one where everything was deleted.
    QVERIFY(QDir(shared).removeRecursively());
    write(laptop, u"New.md"_s, "new\n");
    const QMap<QString, QByteArray> trashed = files(laptop, true);
    const SyncReport report = laptop.engine->sync();
    QVERIFY(!report.failure.isEmpty());
    QVERIFY(exists(laptop, u"Ashfall/Mara.md"_s));
    QCOMPARE(files(laptop, true), trashed);
}

void TestSync::readsRemoteSettings()
{
    QString error;
    QVERIFY(jot::transportFor(u"folder:/mnt/nas/Notes"_s, {}, &error));
    QCOMPARE(jot::remoteLabel(u"folder:/mnt/nas/Notes"_s), u"/mnt/nas/Notes"_s);
    QCOMPARE(jot::remoteLabel(u"git:git@example.org:mara/notes.git#main"_s),
             u"git@example.org:mara/notes.git"_s);
    QCOMPARE(jot::remoteLabel(u"git:https://mara:s3cret@example.org/notes.git#main"_s),
             u"https://example.org/notes.git"_s);
    QCOMPARE(jot::remoteLabel(u"git:https://ghp_token@example.org/notes.git#main"_s),
             u"https://example.org/notes.git"_s);
    QCOMPARE(jot::remoteLabel(u"git:ssh://git@example.org/notes.git#main"_s),
             u"ssh://git@example.org/notes.git"_s);
    QVERIFY(!jot::transportFor(u"carrier-pigeon:coop"_s, {}, &error));
    QVERIFY(error.contains(u"carrier-pigeon"_s));
    // One base for each vault and remote.
    QVERIFY(jot::syncStateDir(u"/a"_s, u"folder:/x"_s) !=
            jot::syncStateDir(u"/a"_s, u"folder:/y"_s));
    QVERIFY(jot::syncStateDir(u"/a"_s, u"folder:/x"_s) !=
            jot::syncStateDir(u"/b"_s, u"folder:/x"_s));
}

void TestSync::readsConflictCopyNames()
{
    // As sync names them, and as the editor does, with no computer.
    const std::optional<jot::ConflictCopy> synced =
        jot::readConflictCopy(u"Ashfall/Mara (conflict desk 2026-11-03 0930).md"_s);
    QVERIFY(synced);
    QCOMPARE(synced->original, u"Ashfall/Mara.md"_s);
    QCOMPARE(synced->from, u"desk"_s);
    QCOMPARE(synced->when, QDateTime(QDate(2026, 11, 3), QTime(9, 30)));
    const std::optional<jot::ConflictCopy> edited =
        jot::readConflictCopy(u"Mara Vey (conflict 2026-11-03 0930) 2.md"_s);
    QVERIFY(edited);
    QCOMPARE(edited->original, u"Mara Vey.md"_s);
    QVERIFY(edited->from.isEmpty());
    QCOMPARE(jot::readConflictCopy(u"a/sketch (conflict desk 2026-11-03 0930).png"_s)->original,
             u"a/sketch.png"_s);
    // As the apps that sync folders name them.
    const std::optional<jot::ConflictCopy> syncthing =
        jot::readConflictCopy(u"Ashfall/Mara.sync-conflict-20261103-093012-ABCD123.md"_s);
    QVERIFY(syncthing);
    QCOMPARE(syncthing->original, u"Ashfall/Mara.md"_s);
    QCOMPARE(syncthing->from, u"ABCD123"_s);
    QCOMPARE(syncthing->when, QDateTime(QDate(2026, 11, 3), QTime(9, 30, 12)));
    const std::optional<jot::ConflictCopy> nextcloud =
        jot::readConflictCopy(u"Mara (conflicted copy 2026-11-03 093012).md"_s);
    QVERIFY(nextcloud);
    QCOMPARE(nextcloud->original, u"Mara.md"_s);
    QCOMPARE(nextcloud->when, QDateTime(QDate(2026, 11, 3), QTime(9, 30, 12)));
    QCOMPARE(jot::readConflictCopy(u"Mara (conflicted copy lara 2026-11-03 093012).md"_s)->from,
             u"lara"_s);
    QCOMPARE(jot::readConflictCopy(u"Mara_conflict-20261103-093012.md"_s)->original, u"Mara.md"_s);
    const std::optional<jot::ConflictCopy> dropbox =
        jot::readConflictCopy(u"Mara (Lara's laptop's conflicted copy 2026-11-03).md"_s);
    QVERIFY(dropbox);
    QCOMPARE(dropbox->original, u"Mara.md"_s);
    QCOMPARE(dropbox->from, u"Lara's laptop"_s);
    QCOMPARE(dropbox->when.date(), QDate(2026, 11, 3));
    QVERIFY(!jot::readConflictCopy(u"Notes on conflict.md"_s));
    QVERIFY(!jot::readConflictCopy(u"Mara.sync-conflict-notes.md"_s));
    QVERIFY(!jot::readConflictCopy(u"Mara (conflict of interest).md"_s));

    QCOMPARE(jot::conflictCopiesOf(u"Mara.md"_s,
                                   {u"Mara (conflict desk 2026-11-04 0930).md"_s, u"Mara.md"_s,
                                    u"Soren (conflict desk 2026-11-01 0930).md"_s,
                                    u"Mara (conflict laptop 2026-11-02 1200).md"_s}),
             (QStringList{u"Mara (conflict laptop 2026-11-02 1200).md"_s,
                          u"Mara (conflict desk 2026-11-04 0930).md"_s}));
}

void TestSync::keepsEverythingAcrossRandomEdits_data()
{
    QTest::addColumn<quint32>("seed");
    for (quint32 seed = 1; seed <= 150; ++seed)
        QTest::newRow(qPrintable(QString::number(seed))) << seed;
}

void TestSync::keepsEverythingAcrossRandomEdits()
{
    // Two devices make random edits, each line a token of its own, and sync now and then. At
    // the end both hold the same, and every token is still somewhere unless a person deleted
    // the file it was in.
    QFETCH(quint32, seed);
    QRandomGenerator random(seed);
    Device a = device(u"a"_s);
    Device b = device(u"b"_s);
    const QStringList paths = {u"one.md"_s, u"two.md"_s, u"n/three.md"_s, u"blob.bin"_s,
                               u".jot/layout.yaml"_s};
    QSet<QByteArray> made;
    QSet<QByteArray> deleted;
    int next = 0;
    for (int step = 0; step < 40; ++step) {
        Device &on = random.bounded(2) == 0 ? a : b;
        const QString path = paths.at(random.bounded(int(paths.size())));
        switch (random.bounded(5)) {
        case 0:
        case 1: {
            // Write a new line at the top or the bottom.
            const QByteArray token = "t" + QByteArray::number(++next) + "\n";
            made.insert(token.trimmed());
            const QByteArray was = read(on, path);
            write(on, path, random.bounded(2) == 0 ? token + was : was + token);
            break;
        }
        case 2:
            if (exists(on, path)) {
                for (const QByteArray &line : read(on, path).split('\n'))
                    deleted.insert(line);
                QFile::remove(on.root + u'/' + path);
            }
            break;
        default: {
            const SyncReport report = on.engine->sync();
            QVERIFY(report.failure.isEmpty());
        }
        }
    }
    settle({&a, &b});
    QCOMPARE(files(a), files(b));

    QSet<QByteArray> kept;
    for (const Device *each : {&a, &b}) {
        for (const QMap<QString, QByteArray> &set : {files(*each), files(*each, true)}) {
            for (const QByteArray &data : set) {
                for (const QByteArray &line : data.split('\n'))
                    kept.insert(line);
            }
        }
    }
    for (const QByteArray &data : std::as_const(m_remote->trash)) {
        for (const QByteArray &line : data.split('\n'))
            kept.insert(line);
    }
    for (const QByteArray &token : std::as_const(made))
        QVERIFY2(kept.contains(token) || deleted.contains(token), token.constData());
}

QTEST_GUILESS_MAIN(TestSync)
#include "tst_sync.moc"
