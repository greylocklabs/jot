#include "jot/GitSync.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

using namespace Qt::StringLiterals;
using jot::GitSync;
using jot::SyncReport;
using jot::SyncRequest;

/// Two computers syncing through a bare repository on disk, as through any git host.
class TestGitSync : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void init();
    void sendsAndBringsNotes();
    void mergesEditsGitCanMerge();
    void rebasesOrMergesAsTheVaultSays();
    void commitsAndPushesByHandWhenTheVaultSays();
    void mergesFrontMatterGitCannot();
    void keepsAConflictCopyWithoutMarkers();
    void anEditBeatsADelete();
    void joinsTwoVaultsThatStartedApart();
    void leavesHiddenFilesAndTheTrashHere();
    void leavesWhatTheVaultIgnoresHere();
    void writesCommitMessagesFromTheTemplate();
    void fillsInTheCommitMessage();
    void runsHooksOnlyWhenTheVaultSays();
    void signsAndHooksMergesAsCommits();
    void commitsAsJotWhenGitHasNoUser();
    void keepsLargeFilesInLfs();
    void keepsBothPicturesWhenBothChanged();
    void refusesAnLfsRepositoryWithoutLfs();
    void failsWhenTheRepositoryRefusesThePush();
    void runsSshAsThePersonChose();
    void startsOverAMergeLeftHalfDone();
    void usesTheVaultsOwnRepositoryAsItIs();
    void readsTheVaultsOwnRepository();
    void knowsOneRemoteWrittenTwoWays_data();
    void knowsOneRemoteWrittenTwoWays();
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
    static QByteArray run(const QString &directory, const QStringList &arguments);

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

QByteArray TestGitSync::run(const QString &directory, const QStringList &arguments)
{
    QProcess git;
    git.setWorkingDirectory(directory);
    git.start(u"git"_s, arguments);
    git.waitForFinished();
    return git.readAllStandardOutput().trimmed();
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

void TestGitSync::rebasesOrMergesAsTheVaultSays()
{
    const Computer laptop = computer(u"laptop"_s);
    const Computer desk = computer(u"desk"_s);
    write(laptop, u"Mara.md"_s, "# Mara\n");
    QVERIFY(sync(laptop).failure.isEmpty());
    QVERIFY(sync(desk).failure.isEmpty());
    const auto merges = [this] {
        return run(m_dir->path(), {u"--git-dir="_s + m_remote, u"log"_s, u"--merges"_s,
                                   u"--format=%h"_s, u"main"_s});
    };

    // Rebased by default: work from both computers in a line. The first sync of each joined
    // two histories, which only a merge can.
    const QByteArray joined = merges();
    write(laptop, u"Soren.md"_s, "# Soren\n");
    write(desk, u"Ashfall.md"_s, "# Ashfall\n");
    QVERIFY(sync(laptop).failure.isEmpty());
    QVERIFY(sync(desk).failure.isEmpty());
    QCOMPARE(merges(), joined);
    QCOMPARE(read(desk, u"Soren.md"_s), QByteArray("# Soren\n"));

    // A clash stops the rebase, and the merge Jot falls back on keeps both versions.
    QVERIFY(sync(laptop).failure.isEmpty());
    write(laptop, u"Mara.md"_s, "# Mara\nlaptop\n");
    write(desk, u"Mara.md"_s, "# Mara\ndesk\n");
    QVERIFY(sync(laptop).failure.isEmpty());
    const SyncReport clashed = sync(desk);
    QVERIFY2(clashed.failure.isEmpty(), qPrintable(clashed.failure));
    QVERIFY(!clashed.conflicts.isEmpty() || !clashed.merged.isEmpty());
    QVERIFY(!QFileInfo::exists(desk.gitDir + u"/rebase-merge"_s));
    QVERIFY(!read(desk, u"Mara.md"_s).contains("<<<<<<<"));

    // Merging, as the vault says, makes merge commits again.
    for (const Computer *each : {&laptop, &desk})
        write(*each, u".jot/vault.yaml"_s, "format: 1\ngitPull: merge\n");
    QVERIFY(sync(laptop).failure.isEmpty());
    QVERIFY(sync(desk).failure.isEmpty());
    QVERIFY(sync(laptop).failure.isEmpty());
    const QByteArray before = merges();
    write(laptop, u"Soren.md"_s, "# Soren\nagain\n");
    write(desk, u"Ashfall.md"_s, "# Ashfall\nagain\n");
    QVERIFY(sync(laptop).failure.isEmpty());
    QVERIFY(sync(desk).failure.isEmpty());
    QVERIFY(merges().size() > before.size());
    QVERIFY(!GitSync::optionsOf(desk.root).rebase);
}

void TestGitSync::commitsAndPushesByHandWhenTheVaultSays()
{
    const Computer laptop = computer(u"laptop"_s);
    const Computer desk = computer(u"desk"_s);
    write(laptop, u"Mara.md"_s, "# Mara\n");
    write(laptop, u"Soren.md"_s, "# Soren\n");
    QVERIFY(sync(laptop).failure.isEmpty());
    QVERIFY(sync(desk).failure.isEmpty());
    const auto at = QDateTime(QDate(2026, 11, 3), QTime(9, 30));
    const auto syncAs = [this, at](const Computer &on, const SyncRequest &request) {
        return GitSync(on.root, m_remote, u"main"_s, on.gitDir, on.name).sync(at, request);
    };
    const auto subject = [this] {
        return run(m_dir->path(),
                   {u"--git-dir="_s + m_remote, u"log"_s, u"-1"_s, u"--format=%s"_s, u"main"_s});
    };

    // Commits by hand: a sync leaves what changed uncommitted and still brings in the rest.
    write(desk, u".jot/vault.yaml"_s, "format: 1\ngitCommit: manual\n");
    write(desk, u"Mara.md"_s, "# Mara\ndesk\n");
    write(laptop, u"Soren.md"_s, "# Soren\nlaptop\n");
    QVERIFY(sync(laptop).failure.isEmpty());
    SyncReport report = sync(desk);
    QVERIFY2(report.failure.isEmpty(), qPrintable(report.failure));
    QCOMPARE(report.uncommitted, 2);
    QCOMPARE(read(desk, u"Soren.md"_s), QByteArray("# Soren\nlaptop\n"));
    QCOMPARE(read(desk, u"Mara.md"_s), QByteArray("# Mara\ndesk\n"));
    QVERIFY(GitSync(desk.root, m_remote, u"main"_s, desk.gitDir, desk.name)
                .changes()
                .contains(u"Mara.md"_s));

    // What was pushed to a file changed here waits until that change is committed.
    write(laptop, u"Mara.md"_s, "# Mara\nlaptop\n");
    QVERIFY(sync(laptop).failure.isEmpty());
    report = sync(desk);
    QVERIFY(report.failure.contains(u"Mara.md"_s));
    QCOMPARE(read(desk, u"Mara.md"_s), QByteArray("# Mara\ndesk\n"));
    report = syncAs(desk, {u"notes: desk's Mara"_s, false});
    QVERIFY2(report.failure.isEmpty(), qPrintable(report.failure));
    QCOMPARE(report.uncommitted, 0);
    QVERIFY(!read(desk, u"Mara.md"_s).contains("<<<<<<<"));
    QVERIFY(run(m_dir->path(), {u"--git-dir="_s + m_remote, u"log"_s, u"--format=%s"_s, u"main"_s})
                .contains("notes: desk's Mara"));

    // A file renamed there is changed there too.
    QVERIFY(sync(laptop).failure.isEmpty());
    write(desk, u"Mara.md"_s, "# Mara\ndesk again\n");
    QVERIFY(QFile::rename(laptop.root + u"/Mara.md"_s, laptop.root + u"/Mara Vey.md"_s));
    // The vault's settings came to the laptop too, so it commits by hand now as well.
    QVERIFY(syncAs(laptop, {u"notes: Mara's full name"_s, false}).failure.isEmpty());
    report = sync(desk);
    QVERIFY2(report.failure.contains(u"commit the changes to Mara.md"_s),
             qPrintable(report.failure));
    QCOMPARE(read(desk, u"Mara.md"_s), QByteArray("# Mara\ndesk again\n"));
    report = syncAs(desk, {u"notes: desk's Mara again"_s, false});
    QVERIFY2(report.failure.isEmpty(), qPrintable(report.failure));
    QVERIFY(report.pulled.contains(u"Mara Vey.md"_s));

    // Pushes by hand: commits wait here until a push is asked for.
    write(desk, u".jot/vault.yaml"_s, "format: 1\ngitPush: manual\n");
    write(desk, u"Ashfall.md"_s, "# Ashfall\n");
    report = sync(desk);
    QVERIFY2(report.failure.isEmpty(), qPrintable(report.failure));
    QCOMPARE(report.unpushed, 1);
    QVERIFY(inRemote(u"Ashfall.md"_s).isEmpty());
    report = syncAs(desk, {{}, true});
    QVERIFY2(report.failure.isEmpty(), qPrintable(report.failure));
    QCOMPARE(report.unpushed, 0);
    QCOMPARE(inRemote(u"Ashfall.md"_s), QByteArray("# Ashfall\n"));
    QVERIFY(subject().startsWith("chore(sync): jot commit from desk"));
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
    write(laptop, u".jot/layout.yaml"_s, "favorites: []\n");
    write(laptop, u".jot/trash/2026-11-03 093000/Old.md"_s, "old\n");
    write(laptop, u".obsidian/app.json"_s, "{}\n");
    write(laptop, u"Ashfall/.DS_Store"_s, "x");
    write(laptop, u"Mara.md"_s, "# Mara\n");
    QVERIFY(sync(laptop).failure.isEmpty());
    QCOMPARE(inRemote(u".jot/layout.yaml"_s), QByteArray("favorites: []\n"));
    QVERIFY(inRemote(u".jot/trash/2026-11-03 093000/Old.md"_s).isEmpty());
    QVERIFY(inRemote(u".obsidian/app.json"_s).isEmpty());
    QVERIFY(inRemote(u"Ashfall/.DS_Store"_s).isEmpty());
}

void TestGitSync::leavesWhatTheVaultIgnoresHere()
{
    const Computer laptop = computer(u"laptop"_s);
    write(laptop, u".jot/vault.yaml"_s, "format: 1\nignore: [Scratch/, \"*.yaml\"]\n");
    write(laptop, u"Scratch/Idea.md"_s, "idea\n");
    write(laptop, u"Mara.md"_s, "# Mara\n");
    write(laptop, u"data.yaml"_s, "a: 1\n");
    QVERIFY(sync(laptop).failure.isEmpty());
    QVERIFY(inRemote(u"Scratch/Idea.md"_s).isEmpty());
    QVERIFY(inRemote(u"data.yaml"_s).isEmpty());
    // A broad pattern leaves Jot's own folder synced.
    QVERIFY(inRemote(u".jot/vault.yaml"_s).contains("Scratch/"));
    QCOMPARE(inRemote(u"Mara.md"_s), QByteArray("# Mara\n"));

    // The patterns are written again when they change, without piling up.
    write(laptop, u".jot/vault.yaml"_s, "format: 1\nignore: [Other/]\n");
    write(laptop, u"Mara.md"_s, "# Mara\nedited\n");
    QVERIFY(sync(laptop).failure.isEmpty());
    QFile exclude(laptop.gitDir + u"/info/exclude"_s);
    QVERIFY(exclude.open(QIODevice::ReadOnly));
    const QByteArray excluded = exclude.readAll();
    QCOMPARE(excluded.count("# Jot: the vault's ignore patterns."), 1);
    QVERIFY(excluded.contains("Other/\n"));
    QVERIFY(!excluded.contains("Scratch/\n"));
}

void TestGitSync::writesCommitMessagesFromTheTemplate()
{
    const Computer laptop = computer(u"laptop"_s);
    write(laptop, u"Mara.md"_s, "# Mara\n");
    QVERIFY(sync(laptop).failure.isEmpty());
    const auto subject = [this] {
        return run(m_dir->path(),
                   {u"--git-dir="_s + m_remote, u"log"_s, u"-1"_s, u"--format=%s"_s, u"main"_s});
    };
    QCOMPARE(subject(), QByteArray("chore(sync): jot commit from laptop, 2026-11-03 09:30"));

    write(laptop, u".jot/vault.yaml"_s,
          "format: 1\ngitMessage: \"notes: {count} from {device}\"\n");
    write(laptop, u"Mara.md"_s, "# Mara\nedited\n");
    QVERIFY(sync(laptop).failure.isEmpty());
    QCOMPARE(subject(), QByteArray("notes: 2 from laptop"));
}

void TestGitSync::fillsInTheCommitMessage()
{
    const QDateTime at(QDate(2026, 11, 3), QTime(9, 30));
    QCOMPARE(GitSync::commitMessage(u"{device} {date} {time}: {files}"_s, u"desk"_s, at,
                                    {u"a.md"_s, u"b.md"_s}),
             u"desk 2026-11-03 09:30: a.md, b.md"_s);
    QCOMPARE(GitSync::commitMessage(u"{count}: {files}"_s, u"desk"_s, at,
                                    {u"a.md"_s, u"b.md"_s, u"c.md"_s, u"d.md"_s, u"e.md"_s}),
             u"5: a.md, b.md, c.md and 2 more"_s);
    QCOMPARE(GitSync::optionsOf(m_dir->path()).message, GitSync::kDefaultMessage);
}

void TestGitSync::runsHooksOnlyWhenTheVaultSays()
{
    const Computer laptop = computer(u"laptop"_s);
    run(laptop.root, {u"init"_s, u"-q"_s, u"-b"_s, u"main"_s});
    run(laptop.root, {u"remote"_s, u"add"_s, u"origin"_s, m_remote});
    run(laptop.root, {u"config"_s, u"user.name"_s, u"Mara"_s});
    run(laptop.root, {u"config"_s, u"user.email"_s, u"mara@example.org"_s});
    QFile hook(laptop.root + u"/.git/hooks/commit-msg"_s);
    QVERIFY(hook.open(QIODevice::WriteOnly));
    hook.write("#!/bin/sh\necho 'rejected by the hook' >&2\nexit 1\n");
    hook.close();
    hook.setPermissions(hook.permissions() | QFileDevice::ExeOwner);

    write(laptop, u"Mara.md"_s, "# Mara\n");
    QVERIFY(sync(laptop).failure.isEmpty());
    write(laptop, u".jot/vault.yaml"_s, "format: 1\ngitHooks: true\n");
    write(laptop, u"Mara.md"_s, "# Mara\nedited\n");
    QVERIFY(sync(laptop).failure.contains(u"rejected by the hook"_s));
    QVERIFY(GitSync::optionsOf(laptop.root).hooks);
    QVERIFY(!GitSync::optionsOf(laptop.root).sign);
    write(laptop, u".jot/vault.yaml"_s, "format: 1\ngitSign: false\n");
    QCOMPARE(GitSync::optionsOf(laptop.root).sign, std::optional<bool>(false));
}

void TestGitSync::signsAndHooksMergesAsCommits()
{
    // The laptop's repository signs everything, with a signer that always fails, and its hook
    // turns down merges.
    const Computer laptop = computer(u"laptop"_s);
    const Computer desk = computer(u"desk"_s);
    run(laptop.root, {u"init"_s, u"-q"_s, u"-b"_s, u"main"_s});
    run(laptop.root, {u"remote"_s, u"add"_s, u"origin"_s, m_remote});
    run(laptop.root, {u"config"_s, u"user.name"_s, u"Mara"_s});
    run(laptop.root, {u"config"_s, u"user.email"_s, u"mara@example.org"_s});
    run(laptop.root, {u"config"_s, u"commit.gpgsign"_s, u"true"_s});
    run(laptop.root, {u"config"_s, u"gpg.program"_s, u"false"_s});
    QFile hook(laptop.root + u"/.git/hooks/commit-msg"_s);
    QVERIFY(hook.open(QIODevice::WriteOnly));
    hook.write(
        "#!/bin/sh\nif grep -q '^Merge' \"$1\"; then echo 'no merges here' >&2; exit 1; fi\n");
    hook.close();
    hook.setPermissions(hook.permissions() | QFileDevice::ExeOwner);

    // Signing as git says fails; not signing, as the vault says, commits.
    write(laptop, u"Mara.md"_s, "# Mara\n");
    QVERIFY(!sync(laptop).failure.isEmpty());
    write(laptop, u".jot/vault.yaml"_s, "format: 1\ngitSign: false\ngitPull: merge\n");
    SyncReport report = sync(laptop);
    QVERIFY2(report.failure.isEmpty(), qPrintable(report.failure));

    // So does a merge, which skips the hook while hooks are off.
    QVERIFY(sync(desk).failure.isEmpty());
    write(desk, u"Soren.md"_s, "# Soren\n");
    QVERIFY(sync(desk).failure.isEmpty());
    write(laptop, u"Mara.md"_s, "# Mara\nlaptop\n");
    report = sync(laptop);
    QVERIFY2(report.failure.isEmpty(), qPrintable(report.failure));
    QCOMPARE(read(laptop, u"Soren.md"_s), QByteArray("# Soren\n"));

    // With hooks on, the merge goes through the hook, which turns it down.
    write(laptop, u".jot/vault.yaml"_s,
          "format: 1\ngitSign: false\ngitPull: merge\ngitHooks: true\n");
    QVERIFY(sync(laptop).failure.isEmpty());
    write(desk, u"Ashfall.md"_s, "# Ashfall\n");
    QVERIFY(sync(desk).failure.isEmpty());
    write(laptop, u"Mara.md"_s, "# Mara\nlaptop again\n");
    QVERIFY(sync(laptop).failure.contains(u"no merges here"_s));
}

void TestGitSync::commitsAsJotWhenGitHasNoUser()
{
    // No global or system git settings, as on a computer where git was never set up.
    QTemporaryDir home;
    const QByteArray global = qgetenv("GIT_CONFIG_GLOBAL");
    const QByteArray system = qgetenv("GIT_CONFIG_NOSYSTEM");
    qputenv("GIT_CONFIG_GLOBAL", home.filePath(u"gitconfig"_s).toUtf8());
    qputenv("GIT_CONFIG_NOSYSTEM", "1");
    const Computer laptop = computer(u"laptop"_s);
    write(laptop, u"Mara.md"_s, "# Mara\n");
    const SyncReport report = sync(laptop);
    global.isNull() ? qunsetenv("GIT_CONFIG_GLOBAL") : qputenv("GIT_CONFIG_GLOBAL", global);
    system.isNull() ? qunsetenv("GIT_CONFIG_NOSYSTEM") : qputenv("GIT_CONFIG_NOSYSTEM", system);
    QVERIFY2(report.failure.isEmpty(), qPrintable(report.failure));
    QCOMPARE(run(m_dir->path(), {u"--git-dir="_s + m_remote, u"log"_s, u"-1"_s,
                                 u"--format=%an <%ae>"_s, u"main"_s}),
             QByteArray("Jot <jot@localhost>"));
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
    QVERIFY(inRemote(u".gitattributes"_s).contains("*.[Pp][Nn][Gg] filter=lfs"));
    QCOMPARE(GitSync::largeFilePattern(u"7z"_s), QByteArray("*.7[Zz]"));
    // Either case, as a camera writes it.
    write(laptop, u"Mara.assets/IMG_0001.JPG"_s, picture);
    QVERIFY(sync(laptop).failure.isEmpty());
    QVERIFY(inRemote(u"Mara.assets/IMG_0001.JPG"_s).startsWith("version https://git-lfs"));
    QVERIFY2(sync(desk).failure.isEmpty(), "desk");
    QCOMPARE(read(desk, u"Mara.assets/sketch.png"_s), picture);
}

void TestGitSync::keepsBothPicturesWhenBothChanged()
{
    if (!GitSync::lfsAvailable())
        QSKIP("Git LFS is not installed");
    const Computer laptop = computer(u"laptop"_s);
    const Computer desk = computer(u"desk"_s);
    const QByteArray png("\x89PNG\r\n\x1a\n", 8);
    write(laptop, u"sketch.png"_s, png + "first");
    sync(laptop);
    sync(desk);
    write(laptop, u"sketch.png"_s, png + "laptop's");
    write(desk, u"sketch.png"_s, png + "desk's");
    sync(laptop);
    const SyncReport report = sync(desk);
    QVERIFY2(report.failure.isEmpty(), qPrintable(report.failure));
    QCOMPARE(report.conflicts, QStringList{u"sketch (conflict desk 2026-11-03 0930).png"_s});
    // The pictures themselves, not the pointers LFS keeps in the repository.
    QCOMPARE(read(desk, u"sketch.png"_s), png + "desk's");
    QCOMPARE(read(desk, report.conflicts.first()), png + "laptop's");
}

void TestGitSync::refusesAnLfsRepositoryWithoutLfs()
{
    if (!GitSync::lfsAvailable())
        QSKIP("Git LFS is not installed");
    const Computer laptop = computer(u"laptop"_s);
    const Computer desk = computer(u"desk"_s);
    write(laptop, u"sketch.png"_s, QByteArray("\x89PNG\r\n\x1a\n", 8) + "picture");
    QVERIFY(sync(laptop).failure.isEmpty());
    write(desk, u"Mara.md"_s, "x\n");
    // A computer with git alone, without git-lfs.
    const QString bin = m_dir->filePath(u"bin"_s);
    QDir().mkpath(bin);
    QVERIFY(QFile::link(QStandardPaths::findExecutable(u"git"_s), bin + u"/git"_s));
    const QByteArray path = qgetenv("PATH");
    qputenv("PATH", bin.toUtf8());
    const bool without = !GitSync::lfsAvailable();
    const SyncReport report = sync(desk);
    qputenv("PATH", path);
    QVERIFY(without);
    QVERIFY(report.failure.contains(u"Git LFS"_s));
    QVERIFY(!exists(desk, u"sketch.png"_s));
    QVERIFY(inRemote(u"Mara.md"_s).isEmpty());
}

void TestGitSync::failsWhenTheRepositoryRefusesThePush()
{
    const QString hook = m_remote + u"/hooks/pre-receive"_s;
    QFile file(hook);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("#!/bin/sh\necho protected >&2\nexit 1\n");
    file.close();
    QVERIFY(file.setPermissions(file.permissions() | QFile::ExeOwner));
    const Computer laptop = computer(u"laptop"_s);
    write(laptop, u"Mara.md"_s, "x\n");
    const SyncReport report = sync(laptop);
    QVERIFY(report.failure.contains(u"protected"_s));
    QVERIFY(report.pushed.isEmpty());
}

void TestGitSync::runsSshAsThePersonChose()
{
    // An SSH command from the person's git settings, as for a key of their choosing, is used.
    const QString marker = m_dir->filePath(u"ran"_s);
    const QString script = m_dir->filePath(u"ssh.sh"_s);
    QFile file(script);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("#!/bin/sh\ntouch '" + marker.toUtf8() + "'\nexit 1\n");
    file.close();
    QVERIFY(file.setPermissions(file.permissions() | QFile::ExeOwner));
    const QString config = m_dir->filePath(u"gitconfig"_s);
    QFile settings(config);
    QVERIFY(settings.open(QIODevice::WriteOnly));
    settings.write("[core]\n\tsshCommand = " + script.toUtf8() + "\n");
    settings.close();
    qputenv("GIT_CONFIG_GLOBAL", config.toUtf8());
    QString error;
    const bool reached = GitSync::reachable(u"ssh://example.invalid/notes.git"_s, &error);
    qunsetenv("GIT_CONFIG_GLOBAL");
    QVERIFY(!reached);
    QVERIFY(QFileInfo::exists(marker));
}

void TestGitSync::startsOverAMergeLeftHalfDone()
{
    const Computer laptop = computer(u"laptop"_s);
    const Computer desk = computer(u"desk"_s);
    write(laptop, u"Mara.md"_s, "line\n");
    sync(laptop);
    sync(desk);
    write(laptop, u"Mara.md"_s, "laptop's line\n");
    write(desk, u"Mara.md"_s, "desk's line\n");
    sync(laptop);
    // The desk quits within its merge, leaving git's markers in the note. Jot gives git a user
    // only for its own commits, so these name one.
    const QStringList at{u"-c"_s,
                         u"user.name=Desk"_s,
                         u"-c"_s,
                         u"user.email=desk@example.org"_s,
                         u"--git-dir="_s + desk.gitDir,
                         u"--work-tree="_s + desk.root};
    const auto step = [&desk, &at](const QStringList &arguments) {
        QProcess git;
        git.setWorkingDirectory(desk.root);
        git.setProcessChannelMode(QProcess::MergedChannels);
        git.start(u"git"_s, at + arguments);
        git.waitForFinished();
        return u"git %1: %2"_s.arg(arguments.join(u' '), QString::fromUtf8(git.readAll()));
    };
    const QString committed = step({u"commit"_s, u"-qam"_s, u"desk"_s});
    const QString fetched = step({u"fetch"_s, u"-q"_s, u"origin"_s});
    const QString merged = step({u"merge"_s, u"-q"_s, u"origin/main"_s});
    QVERIFY2(read(desk, u"Mara.md"_s).contains("<<<<<<<"),
             qPrintable(committed + u'\n' + fetched + u'\n' + merged));

    const SyncReport report = sync(desk);
    QVERIFY2(report.failure.isEmpty(), qPrintable(report.failure));
    QCOMPARE(read(desk, u"Mara.md"_s), QByteArray("desk's line\n"));
    sync(laptop);
    QCOMPARE(notes(laptop), notes(desk));
    for (const QByteArray &data : notes(desk))
        QVERIFY(!data.contains("<<<<<<<"));
}

void TestGitSync::usesTheVaultsOwnRepositoryAsItIs()
{
    const Computer laptop = computer(u"laptop"_s);
    run(laptop.root, {u"init"_s, u"-q"_s, u"-b"_s, u"main"_s});
    run(laptop.root, {u"remote"_s, u"add"_s, u"origin"_s, m_remote});
    run(laptop.root, {u"config"_s, u"user.name"_s, u"Mara"_s});
    run(laptop.root, {u"config"_s, u"user.email"_s, u"mara@example.org"_s});
    write(laptop, u".obsidian/app.json"_s, "{}\n");
    write(laptop, u"Mara.md"_s, "# Mara\n");
    const SyncReport report = sync(laptop);
    QVERIFY2(report.failure.isEmpty(), qPrintable(report.failure));
    QCOMPARE(inRemote(u"Mara.md"_s), QByteArray("# Mara\n"));
    // Its own git dir, not Jot's, and its settings as the person left them.
    QVERIFY(!QFileInfo::exists(laptop.gitDir));
    QCOMPARE(run(laptop.root, {u"log"_s, u"-1"_s, u"--format=%ae"_s}),
             QByteArray("mara@example.org"));
    write(laptop, u".jot/trash/2026-11-03 093000/Old.md"_s, "old\n");
    QVERIFY(sync(laptop).failure.isEmpty());
    QCOMPARE(inRemote(u".obsidian/app.json"_s), QByteArray("{}\n"));
    QVERIFY(inRemote(u".jot/trash/2026-11-03 093000/Old.md"_s).isEmpty());
    QVERIFY(!QFileInfo::exists(laptop.root + u"/.gitattributes"_s));

    GitSync elsewhere(laptop.root, m_dir->filePath(u"other.git"_s), u"main"_s, laptop.gitDir,
                      laptop.name);
    QVERIFY(elsewhere.sync().failure.contains(u"origin"_s));
    // An origin with a token in it is shown without the token.
    run(laptop.root, {u"remote"_s, u"set-url"_s, u"origin"_s,
                      u"https://me:ghp_secret@example.org/me/notes.git"_s});
    const QString told = elsewhere.sync().failure;
    QVERIFY(told.contains(u"https://example.org/me/notes.git"_s));
    QVERIFY(!told.contains(u"ghp_secret"_s));
    run(laptop.root, {u"remote"_s, u"set-url"_s, u"origin"_s, m_remote});
    GitSync otherBranch(laptop.root, m_remote, u"notes"_s, laptop.gitDir, laptop.name);
    QVERIFY(otherBranch.sync().failure.contains(u"notes"_s));
    QCOMPARE(run(laptop.root, {u"remote"_s, u"get-url"_s, u"origin"_s}), m_remote.toUtf8());

    // The origin written another way is the same origin.
    GitSync spelled(laptop.root, m_remote + u"/"_s, u"main"_s, laptop.gitDir, laptop.name);
    write(laptop, u"Mara.md"_s, "# Mara\nagain\n");
    const SyncReport again = spelled.sync();
    QVERIFY2(again.failure.isEmpty(), qPrintable(again.failure));
}

void TestGitSync::readsTheVaultsOwnRepository()
{
    const Computer laptop = computer(u"laptop"_s);
    QVERIFY(!GitSync::ownRepository(laptop.root).exists);
    run(laptop.root, {u"init"_s, u"-q"_s, u"-b"_s, u"notes"_s});
    GitSync::OwnRepository own = GitSync::ownRepository(laptop.root);
    QVERIFY(own.exists);
    QVERIFY(own.origin.isEmpty());
    QCOMPARE(own.branch, u"notes"_s);
    GitSync noOrigin(laptop.root, m_remote, u"notes"_s, laptop.gitDir, laptop.name);
    QVERIFY(noOrigin.sync().failure.contains(u"no origin"_s));

    run(laptop.root, {u"remote"_s, u"add"_s, u"origin"_s, m_remote});
    run(laptop.root, {u"config"_s, u"user.name"_s, u"Mara"_s});
    run(laptop.root, {u"config"_s, u"user.email"_s, u"mara@example.org"_s});
    write(laptop, u"Mara.md"_s, "# Mara\n");
    run(laptop.root, {u"add"_s, u"Mara.md"_s});
    run(laptop.root, {u"commit"_s, u"-q"_s, u"-m"_s, u"first"_s});
    run(laptop.root, {u"checkout"_s, u"-q"_s, u"--detach"_s});
    own = GitSync::ownRepository(laptop.root);
    QCOMPARE(own.origin, m_remote);
    QVERIFY(own.branch.isEmpty());
    GitSync detached(laptop.root, m_remote, u"notes"_s, laptop.gitDir, laptop.name);
    QVERIFY(detached.sync().failure.contains(u"no branch"_s));
}

void TestGitSync::knowsOneRemoteWrittenTwoWays_data()
{
    QTest::addColumn<QString>("a");
    QTest::addColumn<QString>("b");
    QTest::addColumn<bool>("same");
    QTest::newRow("ssh and https")
        << u"git@github.com:me/notes.git"_s << u"https://github.com/me/notes"_s << true;
    QTest::newRow("ssh url with a port")
        << u"ssh://git@github.com:22/me/notes.git"_s << u"git@github.com:me/notes"_s << true;
    QTest::newRow("trailing slash and host case")
        << u"https://GitHub.com/me/notes/"_s << u"https://github.com/me/notes.git"_s << true;
    QTest::newRow("another repository")
        << u"git@github.com:me/notes.git"_s << u"git@github.com:me/other.git"_s << false;
    QTest::newRow("another host") << u"git@github.com:me/notes.git"_s
                                  << u"git@gitlab.com:me/notes.git"_s << false;
    QTest::newRow("local path") << u"/srv/git/notes.git/"_s << u"/srv/git/notes"_s << true;
    QTest::newRow("local paths differ") << u"/srv/git/notes"_s << u"/srv/git/other"_s << false;
}

void TestGitSync::knowsOneRemoteWrittenTwoWays()
{
    QFETCH(QString, a);
    QFETCH(QString, b);
    QFETCH(bool, same);
    QCOMPARE(GitSync::sameRemote(a, b), same);
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
