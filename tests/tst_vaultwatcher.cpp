#include "jot/VaultWatcher.h"

#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace jot;
using namespace Qt::StringLiterals;

class TestVaultWatcher : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void init();

    void seesNewNotes();
    void seesSavesByRenameAndInPlace();
    void seesRemovals();
    void seesWholeNewFolders();
    void seesRemovedFolders();
    void ignoresItsOwnWrites();
    void ignoresHiddenFilesAndOtherFiles();
    void leavesOutWhatTheVaultIgnores();
    void rescanFindsWhatEventsMissed();

private:
    void write(const QString &path, const QByteArray &bytes, bool atomic = false);
    QStringList nextChange();

    std::unique_ptr<QTemporaryDir> m_dir;
    std::unique_ptr<Vault> m_vault;
    std::unique_ptr<VaultWatcher> m_watcher;
    std::unique_ptr<QSignalSpy> m_changed;
    std::unique_ptr<QSignalSpy> m_folders;
};

void TestVaultWatcher::init()
{
    m_dir = std::make_unique<QTemporaryDir>();
    m_vault = std::make_unique<Vault>(m_dir->path());
    write(u"Old.md"_s, "old\n");
    write(u"Recipes/Bread.md"_s, "bread\n");
    m_watcher = std::make_unique<VaultWatcher>(*m_vault);
    m_watcher->setDelay(50);
    m_watcher->start();
    m_changed = std::make_unique<QSignalSpy>(m_watcher.get(), &VaultWatcher::changed);
    m_folders = std::make_unique<QSignalSpy>(m_watcher.get(), &VaultWatcher::foldersChanged);
}

void TestVaultWatcher::write(const QString &path, const QByteArray &bytes, bool atomic)
{
    const QString file = m_vault->absolute(path);
    QVERIFY(QDir().mkpath(QFileInfo(file).path()));
    if (atomic) {
        QString error;
        QVERIFY2(notefile::write(file, QString::fromUtf8(bytes), &error), qPrintable(error));
        return;
    }
    QFile out(file);
    QVERIFY(out.open(QIODevice::WriteOnly | QIODevice::Truncate));
    out.write(bytes);
}

QStringList TestVaultWatcher::nextChange()
{
    if (m_changed->isEmpty() && !m_changed->wait(3000))
        return {};
    return m_changed->takeFirst().first().toStringList();
}

void TestVaultWatcher::seesNewNotes()
{
    write(u"New.md"_s, "new\n");
    QCOMPARE(nextChange(), QStringList{u"New.md"_s});
}

void TestVaultWatcher::seesSavesByRenameAndInPlace()
{
    write(u"Old.md"_s, "renamed into place\n", true);
    QCOMPARE(nextChange(), QStringList{u"Old.md"_s});
    write(u"Recipes/Bread.md"_s, "written in place, longer\n");
    QCOMPARE(nextChange(), QStringList{u"Recipes/Bread.md"_s});
    // The watch on a note replaced by a rename follows the new file.
    write(u"Old.md"_s, "then written in place\n");
    QCOMPARE(nextChange(), QStringList{u"Old.md"_s});
}

void TestVaultWatcher::seesRemovals()
{
    QVERIFY(QFile::remove(m_vault->absolute(u"Old.md"_s)));
    QCOMPARE(nextChange(), QStringList{u"Old.md"_s});
}

void TestVaultWatcher::seesWholeNewFolders()
{
    // As a sync tool or a move brings a folder in with its notes already there.
    QTemporaryDir outside;
    QVERIFY(QDir().mkpath(outside.filePath(u"Cast/Leads"_s)));
    QFile mara(outside.filePath(u"Cast/Leads/Mara.md"_s));
    QVERIFY(mara.open(QIODevice::WriteOnly));
    mara.close();
    QVERIFY(QDir().rename(outside.filePath(u"Cast"_s), m_vault->absolute(u"Cast"_s)));

    QCOMPARE(nextChange(), QStringList{u"Cast/Leads/Mara.md"_s});
    QVERIFY(!m_folders->isEmpty());

    // And the new folders are watched too.
    write(u"Cast/Leads/Soren.md"_s, "soren\n");
    QCOMPARE(nextChange(), QStringList{u"Cast/Leads/Soren.md"_s});
}

void TestVaultWatcher::seesRemovedFolders()
{
    QVERIFY(QDir(m_vault->absolute(u"Recipes"_s)).removeRecursively());
    QCOMPARE(nextChange(), QStringList{u"Recipes/Bread.md"_s});
    QVERIFY(!m_folders->isEmpty());
}

void TestVaultWatcher::ignoresItsOwnWrites()
{
    m_watcher->expectWrite(u"Old.md"_s, u"mine\n"_s);
    write(u"Old.md"_s, "mine\n", true);
    QVERIFY(!m_changed->wait(500));

    // The next change, by someone else, is news again.
    write(u"Old.md"_s, "theirs\n", true);
    QCOMPARE(nextChange(), QStringList{u"Old.md"_s});
}

void TestVaultWatcher::ignoresHiddenFilesAndOtherFiles()
{
    write(u".hidden.md"_s, "x\n");
    write(u"photo.png"_s, "x\n");
    write(u"Old.assets/sketch.png"_s, "x\n");
    QVERIFY(!m_changed->wait(500));
    QVERIFY(m_folders->isEmpty());
}

void TestVaultWatcher::leavesOutWhatTheVaultIgnores()
{
    write(u".gitignore"_s, "Scratch/\n");
    QSignalSpy ignoreChanged(m_watcher.get(), &VaultWatcher::ignoreChanged);
    QVERIFY(ignoreChanged.wait(2000));
    m_watcher->start();
    write(u"Scratch/Idea.md"_s, "idea\n");
    QVERIFY(!m_changed->wait(500));
    QVERIFY(m_folders->isEmpty());

    // A change to what is ignored asks for the vault to be read again.
    write(u".gitignore"_s, "Other/\n");
    QVERIFY(ignoreChanged.wait(2000));
}

void TestVaultWatcher::rescanFindsWhatEventsMissed()
{
    // Stand in for an event lost to an overflow or a network mount.
    VaultWatcher quiet(*m_vault);
    quiet.start();
    QSignalSpy changed(&quiet, &VaultWatcher::changed);
    write(u"Old.md"_s, "changed while nobody listened\n");
    quiet.rescan();
    QCOMPARE(changed.size(), 1);
    QCOMPARE(changed.first().first().toStringList(), QStringList{u"Old.md"_s});
}

QTEST_GUILESS_MAIN(TestVaultWatcher)
#include "tst_vaultwatcher.moc"
