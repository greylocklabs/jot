#include "jot/Vault.h"
#include "jot/VaultIgnore.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using namespace jot;
using namespace Qt::StringLiterals;

class TestVaultIgnore : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void readsPatternsAsGitDoes_data();
    void readsPatternsAsGitDoes();
    void deeperGitignoreFilesWin();
    void neverIgnoresJotsOwnFolder();
    void readsTheVaultSettings();
    void leavesIgnoredFilesOutOfTheVault();
    void setsAVaultSettingKeepingTheRest();

private:
    static void write(const QString &path, const QByteArray &data);
};

void TestVaultIgnore::write(const QString &path, const QByteArray &data)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(data);
}

void TestVaultIgnore::readsPatternsAsGitDoes_data()
{
    QTest::addColumn<QStringList>("patterns");
    QTest::addColumn<QString>("path");
    QTest::addColumn<bool>("folder");
    QTest::addColumn<bool>("ignored");

    const QStringList modules{u"node_modules/"_s};
    QTest::newRow("folder at any depth") << modules << u"a/node_modules"_s << true << true;
    QTest::newRow("inside an ignored folder")
        << modules << u"node_modules/x/README.md"_s << false << true;
    QTest::newRow("folder-only pattern skips a file")
        << modules << u"node_modules"_s << false << false;
    QTest::newRow("name glob") << QStringList{u"*.log"_s} << u"a/b/run.log"_s << false << true;
    QTest::newRow("glob stays in one name")
        << QStringList{u"a*b.md"_s} << u"a/x/b.md"_s << false << false;
    QTest::newRow("anchored at the top")
        << QStringList{u"/Drafts"_s} << u"Drafts"_s << true << true;
    QTest::newRow("anchored, not deeper")
        << QStringList{u"/Drafts"_s} << u"Old/Drafts"_s << true << false;
    QTest::newRow("slash in the middle anchors")
        << QStringList{u"Old/Drafts"_s} << u"x/Old/Drafts"_s << true << false;
    QTest::newRow("leading **") << QStringList{u"**/build"_s} << u"a/b/build"_s << true << true;
    QTest::newRow("middle **") << QStringList{u"a/**/z.md"_s} << u"a/z.md"_s << false << true;
    QTest::newRow("middle ** deeper")
        << QStringList{u"a/**/z.md"_s} << u"a/b/c/z.md"_s << false << true;
    QTest::newRow("trailing **") << QStringList{u"Old/**"_s} << u"Old/a/b.md"_s << false << true;
    QTest::newRow("question mark") << QStringList{u"ch?.md"_s} << u"ch1.md"_s << false << true;
    QTest::newRow("class") << QStringList{u"ch[0-9].md"_s} << u"chx.md"_s << false << false;
    QTest::newRow("negated class") << QStringList{u"ch[!0-9].md"_s} << u"chx.md"_s << false << true;
    QTest::newRow("negation brings one back")
        << QStringList{u"*.md"_s, u"!Keep.md"_s} << u"Keep.md"_s << false << false;
    QTest::newRow("last rule wins")
        << QStringList{u"!Keep.md"_s, u"*.md"_s} << u"Keep.md"_s << false << true;
    QTest::newRow("no way back out of an ignored folder")
        << QStringList{u"Old/"_s, u"!Old/Keep.md"_s} << u"Old/Keep.md"_s << false << true;
    QTest::newRow("comment") << QStringList{u"# Draft.md"_s} << u"# Draft.md"_s << false << false;
    QTest::newRow("escaped hash") << QStringList{u"\\#Draft.md"_s} << u"#Draft.md"_s << false
                                  << true;
    QTest::newRow("escaped bang") << QStringList{u"\\!Draft.md"_s} << u"!Draft.md"_s << false
                                  << true;
    QTest::newRow("trailing spaces trimmed")
        << QStringList{u"Draft.md   "_s} << u"Draft.md"_s << false << true;
    QTest::newRow("dots are literal") << QStringList{u"a.md"_s} << u"abmd"_s << false << false;
}

void TestVaultIgnore::readsPatternsAsGitDoes()
{
    QFETCH(QStringList, patterns);
    QFETCH(QString, path);
    QFETCH(bool, folder);
    QFETCH(bool, ignored);
    QTemporaryDir dir;
    const VaultIgnore ignore(dir.path(), patterns, false);
    QCOMPARE(ignore.ignored(path, folder), ignored);
}

void TestVaultIgnore::deeperGitignoreFilesWin()
{
    QTemporaryDir dir;
    write(dir.filePath(u".gitignore"_s), "*.log\nbuild/\n");
    write(dir.filePath(u"Ferry/.gitignore"_s), "!keep.log\n/Notes.md\n");
    const VaultIgnore ignore(dir.path(), {}, true);
    QVERIFY(ignore.ignored(u"run.log"_s, false));
    QVERIFY(ignore.ignored(u"Ferry/run.log"_s, false));
    QVERIFY(!ignore.ignored(u"Ferry/keep.log"_s, false));
    // A .gitignore's anchored pattern is from its own folder.
    QVERIFY(ignore.ignored(u"Ferry/Notes.md"_s, false));
    QVERIFY(!ignore.ignored(u"Notes.md"_s, false));
    QVERIFY(ignore.ignored(u"Ferry/build/out.md"_s, false));

    // Not respected, the .gitignore files count for nothing.
    const VaultIgnore without(dir.path(), {}, false);
    QVERIFY(!without.ignored(u"run.log"_s, false));
    QVERIFY(without.empty());
}

void TestVaultIgnore::neverIgnoresJotsOwnFolder()
{
    QTemporaryDir dir;
    write(dir.filePath(u".gitignore"_s), ".jot/\n*.yaml\n");
    const VaultIgnore ignore(dir.path(), {u".jot"_s}, true);
    QVERIFY(!ignore.ignored(u".jot"_s, true));
    QVERIFY(!ignore.ignored(u".jot/vault.yaml"_s, false));
    QVERIFY(ignore.ignored(u"data.yaml"_s, false));
}

void TestVaultIgnore::readsTheVaultSettings()
{
    QTemporaryDir dir;
    write(dir.filePath(u".gitignore"_s), "*.log\n");
    write(dir.filePath(u".jot/vault.yaml"_s), "format: 1\nignore: [Scratch/, \"*.tmp\"]\n");
    const Vault vault(dir.path());
    QCOMPARE(VaultIgnore::patternsOf(vault), (QStringList{u"Scratch/"_s, u"*.tmp"_s}));
    QVERIFY(VaultIgnore::respectsGitignore(vault));
    const VaultIgnore ignore(vault);
    QVERIFY(ignore.ignored(u"Scratch/a.md"_s, false));
    QVERIFY(ignore.ignored(u"x.tmp"_s, false));
    QVERIFY(ignore.ignored(u"x.log"_s, false));

    write(dir.filePath(u".jot/vault.yaml"_s), "format: 1\nignore: Scratch/\ngitignore: false\n");
    QCOMPARE(VaultIgnore::patternsOf(vault), QStringList{u"Scratch/"_s});
    QVERIFY(!VaultIgnore::respectsGitignore(vault));
    QVERIFY(!VaultIgnore(vault).ignored(u"x.log"_s, false));
}

void TestVaultIgnore::leavesIgnoredFilesOutOfTheVault()
{
    QTemporaryDir dir;
    write(dir.filePath(u"Ferry.md"_s), "# Ferry\n");
    write(dir.filePath(u"node_modules/pkg/README.md"_s), "# pkg\n");
    write(dir.filePath(u"Drafts/Old.md"_s), "old\n");
    write(dir.filePath(u"Drafts/New.md"_s), "new\n");
    write(dir.filePath(u".gitignore"_s), "node_modules/\n");
    write(dir.filePath(u".jot/vault.yaml"_s), "format: 1\nignore:\n  - Drafts/Old.md\n");
    const Vault vault(dir.path());
    QCOMPARE(vault.notes(), (QStringList{u"Drafts/New.md"_s, u"Ferry.md"_s}));

    QString error;
    QVERIFY(vault.setSetting(u"gitignore"_s, false, &error));
    QVERIFY(vault.notes().contains(u"node_modules/pkg/README.md"_s));
}

void TestVaultIgnore::setsAVaultSettingKeepingTheRest()
{
    QTemporaryDir dir;
    write(dir.filePath(u".jot/vault.yaml"_s), "format: 1\n# where to-dos go\ninbox: Inbox.md\n");
    const Vault vault(dir.path());
    QString error;
    QVERIFY(vault.setSetting(u"ignore"_s, QStringList{u"node_modules/"_s, u"*.log"_s}, &error));
    QFile file(dir.filePath(u".jot/vault.yaml"_s));
    QVERIFY(file.open(QIODevice::ReadOnly));
    QCOMPARE(file.readAll(),
             QByteArray("format: 1\n# where to-dos go\ninbox: Inbox.md\nignore: [node_modules/, "
                        "\"*.log\"]\n"));
    file.close();
    QCOMPARE(VaultIgnore::patternsOf(vault), (QStringList{u"node_modules/"_s, u"*.log"_s}));
    QVERIFY(vault.setSetting(u"ignore"_s, QVariant(), &error));
    QVERIFY(file.open(QIODevice::ReadOnly));
    QCOMPARE(file.readAll(), QByteArray("format: 1\n# where to-dos go\ninbox: Inbox.md\n"));

    // A vault with no settings yet gets its format with the key.
    QTemporaryDir fresh;
    QVERIFY(Vault(fresh.path()).setSetting(u"gitignore"_s, false, &error));
    QFile made(fresh.filePath(u".jot/vault.yaml"_s));
    QVERIFY(made.open(QIODevice::ReadOnly));
    QCOMPARE(made.readAll(), QByteArray("format: 1\ngitignore: false\n"));
}

QTEST_GUILESS_MAIN(TestVaultIgnore)
#include "tst_vaultignore.moc"
