#include "jot/GitSync.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QProcess>
#include <QTemporaryDir>
#include <QTest>

using namespace Qt::StringLiterals;
using jot::GitSync;
using jot::SyncReport;

/// Two computers syncing through a bare repository on disk, as through any git host.
class TestGitSync : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void init();
    void sendsAndBringsNotes();
    void mergesEditsGitCanMerge();
    void mergesFrontMatterGitCannot();
    void keepsAConflictCopyWithoutMarkers();
    void anEditBeatsADelete();
    void joinsTwoVaultsThatStartedApart();
    void leavesHiddenFilesAndTheTrashHere();
    void keepsLargeFilesInLfs();
    void failsPlainlyWithoutTheRepository();

private:
    struct Computer
    {
        QString root;
        QString gitDir;
        QString name;
    };
    Computer computer(const QString &name) const;
    SyncReport sync(const Computer &on) const;
    static void write(const Computer &on, const QString &path, const QByteArray &data);
    static QByteArray read(const Computer &on, const QString &path);
    static bool exists(const Computer &on, const QString &path);
    /// The files a computer holds, but for hidden ones.
    static QMap<QString, QByteArray> notes(const Computer &on);
    QByteArray inRemote(const QString &path) const;

    std::unique_ptr<QTemporaryDir> m_dir;
    QString m_remote;
};

void TestGitSync::init()
{
    if (!GitSync::available())
        QSKIP("git is not installed");
    m_dir = std::make_unique<QTemporaryDir>();
    m_remote = m_dir->filePath(u"remote.git"_s);
    QProcess::execute(u"git"_s, {u"init"_s, u"-q"_s, u"--bare"_s, u"-b"_s, u"main"_s, m_remote});
}

TestGitSync::Computer TestGitSync::computer(const QString &name) const
{
    const QString root = m_dir->filePath(name + u"/vault"_s);
    QDir().mkpath(root);
    return {root, m_dir->filePath(name + u"/git"_s), name};
}

SyncReport TestGitSync::sync(const Computer &on) const
{
    return GitSync(on.root, m_remote, u"main"_s, on.gitDir, on.name)
        .sync(QDateTime(QDate(2026, 11, 3), QTime(9, 30)));
}

void TestGitSync::write(const Computer &on, const QString &path, const QByteArray &data)
{
    QDir().mkpath(QFileInfo(on.root + u'/' + path).path());
    QFile file(on.root + u'/' + path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(data);
}

QByteArray TestGitSync::read(const Computer &on, const QString &path)
{
    QFile file(on.root + u'/' + path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

bool TestGitSync::exists(const Computer &on, const QString &path)
{
    return QFileInfo::exists(on.root + u'/' + path);
}

QMap<QString, QByteArray> TestGitSync::notes(const Computer &on)
{
    QMap<QString, QByteArray> found;
    QDirIterator it(on.root, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString path = it.next().mid(on.root.size() + 1);
        found.insert(path, read(on, path));
    }
    return found;
}

QByteArray TestGitSync::inRemote(const QString &path) const
{
    QProcess git;
    git.start(u"git"_s, {u"--git-dir="_s + m_remote, u"show"_s, u"main:"_s + path});
    git.waitForFinished();
    return git.exitCode() == 0 ? git.readAllStandardOutput() : QByteArray();
}

void TestGitSync::sendsAndBringsNotes()
{
    const Computer laptop = computer(u"laptop"_s);
    const Computer desk = computer(u"desk"_s);
    write(laptop, u"Ashfall/Mara Vey.md"_s, "# Mara\n");
    SyncReport report = sync(laptop);
    QVERIFY2(report.failure.isEmpty(), qPrintable(report.failure));
    QVERIFY(report.pushed.contains(u"Ashfall/Mara Vey.md"_s));
    QCOMPARE(inRemote(u"Ashfall/Mara Vey.md"_s), QByteArray("# Mara\n"));
    report = sync(desk);
    QVERIFY2(report.failure.isEmpty(), qPrintable(report.failure));
    QVERIFY(report.pulled.contains(u"Ashfall/Mara Vey.md"_s));
    QCOMPARE(read(desk, u"Ashfall/Mara Vey.md"_s), QByteArray("# Mara\n"));
    // The vault stays a plain folder; the repository is kept elsewhere.
    QVERIFY(!exists(desk, u".git"_s));
    // Nothing new, nothing done.
    QVERIFY(sync(desk).pushed.isEmpty());
}

void TestGitSync::mergesEditsGitCanMerge()
{
    const Computer laptop = computer(u"laptop"_s);
    const Computer desk = computer(u"desk"_s);
    write(laptop, u"Mara.md"_s, "# Mara\n\nfirst\n\nmiddle\n\nlast\n");
    sync(laptop);
    sync(desk);
    write(laptop, u"Mara.md"_s, "# Mara\n\nFIRST\n\nmiddle\n\nlast\n");
    write(desk, u"Mara.md"_s, "# Mara\n\nfirst\n\nmiddle\n\nLAST\n");
    QVERIFY(sync(laptop).failure.isEmpty());
    QVERIFY(sync(desk).failure.isEmpty());
    sync(laptop);
    QCOMPARE(read(laptop, u"Mara.md"_s), QByteArray("# Mara\n\nFIRST\n\nmiddle\n\nLAST\n"));
    QCOMPARE(notes(laptop), notes(desk));
}

void TestGitSync::mergesFrontMatterGitCannot()
{
    const Computer laptop = computer(u"laptop"_s);
    const Computer desk = computer(u"desk"_s);
    write(laptop, u"Fence.md"_s, "---\ntask: open\ndue: 2026-11-02\n---\nBody\n");
    sync(laptop);
    sync(desk);
    // Keys on lines next to each other, which git's merge cannot tell apart.
    write(laptop, u"Fence.md"_s, "---\ntask: done\ndue: 2026-11-02\n---\nBody\n");
    write(desk, u"Fence.md"_s, "---\ntask: open\ndue: 2026-11-05\n---\nBody\n");
    sync(laptop);
    const SyncReport report = sync(desk);
    QVERIFY2(report.failure.isEmpty(), qPrintable(report.failure));
    QCOMPARE(report.merged, QStringList{u"Fence.md"_s});
    QCOMPARE(read(desk, u"Fence.md"_s),
             QByteArray("---\ntask: done\ndue: 2026-11-05\n---\nBody\n"));
    sync(laptop);
    QCOMPARE(notes(laptop), notes(desk));
}

void TestGitSync::keepsAConflictCopyWithoutMarkers()
{
    const Computer laptop = computer(u"laptop"_s);
    const Computer desk = computer(u"desk"_s);
    write(laptop, u"Mara.md"_s, "line\n");
    sync(laptop);
    sync(desk);
    write(laptop, u"Mara.md"_s, "laptop's line\n");
    write(desk, u"Mara.md"_s, "desk's line\n");
    sync(laptop);
    const SyncReport report = sync(desk);
    QVERIFY2(report.failure.isEmpty(), qPrintable(report.failure));
    QCOMPARE(report.conflicts, QStringList{u"Mara (conflict desk 2026-11-03 0930).md"_s});
    QCOMPARE(read(desk, u"Mara.md"_s), QByteArray("desk's line\n"));
    QCOMPARE(read(desk, report.conflicts.first()), QByteArray("laptop's line\n"));
    sync(laptop);
    QCOMPARE(notes(laptop), notes(desk));
    for (const QByteArray &data : notes(laptop))
        QVERIFY(!data.contains("<<<<<<<"));
}

void TestGitSync::anEditBeatsADelete()
{
    const Computer laptop = computer(u"laptop"_s);
    const Computer desk = computer(u"desk"_s);
    write(laptop, u"Mara.md"_s, "one\n");
    sync(laptop);
    sync(desk);
    QVERIFY(QFile::remove(laptop.root + u"/Mara.md"_s));
    write(desk, u"Mara.md"_s, "one\ntwo\n");
    sync(laptop);
    QVERIFY(sync(desk).failure.isEmpty());
    sync(laptop);
    QCOMPARE(read(laptop, u"Mara.md"_s), QByteArray("one\ntwo\n"));
    QCOMPARE(notes(laptop), notes(desk));
}

void TestGitSync::joinsTwoVaultsThatStartedApart()
{
    const Computer laptop = computer(u"laptop"_s);
    const Computer desk = computer(u"desk"_s);
    write(laptop, u"Laptop.md"_s, "from the laptop\n");
    write(laptop, u"Shared.md"_s, "same\n");
    write(desk, u"Desk.md"_s, "from the desk\n");
    write(desk, u"Shared.md"_s, "same\n");
    QVERIFY(sync(laptop).failure.isEmpty());
    const SyncReport report = sync(desk);
    QVERIFY2(report.failure.isEmpty(), qPrintable(report.failure));
    sync(laptop);
    QCOMPARE(notes(laptop), notes(desk));
    QCOMPARE(notes(laptop).keys(), (QStringList{u"Desk.md"_s, u"Laptop.md"_s, u"Shared.md"_s}));
}

void TestGitSync::leavesHiddenFilesAndTheTrashHere()
{
    const Computer laptop = computer(u"laptop"_s);
    write(laptop, u".jot/layout.yaml"_s, "pinned: []\n");
    write(laptop, u".jot/trash/2026-11-03 093000/Old.md"_s, "old\n");
    write(laptop, u".obsidian/app.json"_s, "{}\n");
    write(laptop, u"Ashfall/.DS_Store"_s, "x");
    write(laptop, u"Mara.md"_s, "# Mara\n");
    QVERIFY(sync(laptop).failure.isEmpty());
    QCOMPARE(inRemote(u".jot/layout.yaml"_s), QByteArray("pinned: []\n"));
    QVERIFY(inRemote(u".jot/trash/2026-11-03 093000/Old.md"_s).isEmpty());
    QVERIFY(inRemote(u".obsidian/app.json"_s).isEmpty());
    QVERIFY(inRemote(u"Ashfall/.DS_Store"_s).isEmpty());
}

void TestGitSync::keepsLargeFilesInLfs()
{
    if (!GitSync::lfsAvailable())
        QSKIP("Git LFS is not installed");
    const Computer laptop = computer(u"laptop"_s);
    const Computer desk = computer(u"desk"_s);
    const QByteArray picture = QByteArray("\x89PNG\r\n\x1a\n", 8) + QByteArray(4096, 'x');
    write(laptop, u"Mara.assets/sketch.png"_s, picture);
    const SyncReport report = sync(laptop);
    QVERIFY2(report.failure.isEmpty(), qPrintable(report.failure));
    // The repository holds a pointer; LFS holds the picture.
    QVERIFY(inRemote(u"Mara.assets/sketch.png"_s).startsWith("version https://git-lfs"));
    QVERIFY(inRemote(u".gitattributes"_s).contains("*.png filter=lfs"));
    QVERIFY2(sync(desk).failure.isEmpty(), "desk");
    QCOMPARE(read(desk, u"Mara.assets/sketch.png"_s), picture);
}

void TestGitSync::failsPlainlyWithoutTheRepository()
{
    const Computer laptop = computer(u"laptop"_s);
    write(laptop, u"Mara.md"_s, "x\n");
    GitSync nowhere(laptop.root, m_dir->filePath(u"missing.git"_s), u"main"_s, laptop.gitDir,
                    laptop.name);
    const SyncReport report = nowhere.sync();
    QVERIFY(!report.failure.isEmpty());
    QString error;
    QVERIFY(!GitSync::reachable(m_dir->filePath(u"missing.git"_s), &error));
    QVERIFY(!error.isEmpty());
    QVERIFY(GitSync::reachable(m_remote, &error));
}

QTEST_GUILESS_MAIN(TestGitSync)
#include "tst_gitsync.moc"
