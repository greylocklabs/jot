#include "jot/FrontMatter.h"
#include "jot/Vault.h"
#include "jot/VaultTrash.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>

#include <unistd.h>

using namespace jot;
using namespace Qt::StringLiterals;

class TestVault : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void init();
    void listsAndRestoresItsOwnTrash();
    void readsEveryLayoutOfItsOwnTrash();
    void findsTheVaultsNotesInTheDesktopTrash();

    void createRecordsTheFormat();
    void removesEmptyAttachmentFolders();
    void listsNotebooksAndNotesInNaturalOrder();
    void leavesHiddenFilesAndAttachmentsOut();
    void listsDataFilesBesideNotes();
    void ordersNamesNaturally_data();
    void ordersNamesNaturally();
    void namesFilesSafely_data();
    void namesFilesSafely();
    void newNotesNeverOverwrite();
    void createdNotesCarryTheirDateAndTitle();
    void movingANoteTakesItsAttachments();
    void keepsItsOwnTrashWhereTheDriveHasNone();
    void trashesANotebookWithEverythingInIt();
    void relativePathsStayInside();
    void keepsAttachmentsBesideTheirNote();
    void writesLinkTargetsOtherToolsRead_data();
    void writesLinkTargetsOtherToolsRead();
    void newerVaultsAreOnlyRead();

    void readAndWriteKeepEveryByte();
    void refusesTextThatIsNotUtf8();
    void writingThroughALinkKeepsTheLink();
    void writingAHardLinkedFileKeepsTheLinks();

private:
    void write(const QString &path, const QByteArray &bytes = "x\n");

    std::unique_ptr<QTemporaryDir> m_dir;
    std::unique_ptr<Vault> m_vault;
};

void TestVault::init()
{
    m_dir = std::make_unique<QTemporaryDir>();
    m_vault = std::make_unique<Vault>(m_dir->path());
}

void TestVault::write(const QString &path, const QByteArray &bytes)
{
    const QString file = m_vault->absolute(path);
    QVERIFY(QDir().mkpath(QFileInfo(file).path()));
    QFile out(file);
    QVERIFY(out.open(QIODevice::WriteOnly));
    out.write(bytes);
}

void TestVault::removesEmptyAttachmentFolders()
{
    write(u"Mara.md"_s);
    QVERIFY(QDir().mkpath(m_vault->absolute(u"Mara.assets"_s)));
    write(u"Ashfall/Soren.md"_s);
    QVERIFY(QDir().mkpath(m_vault->absolute(u"Ashfall/Soren.assets"_s)));
    write(u"Kit.md"_s);
    write(u"Kit.assets/cat.png"_s);
    write(u"Ines.md"_s);
    write(u"Ines.assets/.DS_Store"_s);
    // No note of that name, so it is a notebook, empty or not.
    QVERIFY(QDir().mkpath(m_vault->absolute(u"Lone.assets"_s)));

    QCOMPARE(m_vault->removeEmptyAttachmentFolders(), 2);
    QVERIFY(!QFileInfo::exists(m_vault->absolute(u"Mara.assets"_s)));
    QVERIFY(!QFileInfo::exists(m_vault->absolute(u"Ashfall/Soren.assets"_s)));
    QVERIFY(QFileInfo::exists(m_vault->absolute(u"Kit.assets/cat.png"_s)));
    QVERIFY(QFileInfo::exists(m_vault->absolute(u"Ines.assets/.DS_Store"_s)));
    QVERIFY(QFileInfo::exists(m_vault->absolute(u"Lone.assets"_s)));
    QCOMPARE(m_vault->removeEmptyAttachmentFolders(), 0);
}

void TestVault::createRecordsTheFormat()
{
    QString error;
    const std::optional<Vault> vault = Vault::create(m_dir->filePath(u"New Vault"_s), &error);
    QVERIFY2(vault, qPrintable(error));
    QVERIFY(vault->exists());
    QCOMPARE(vault->format(), 1);
    QVERIFY(vault->writable());
    QVERIFY(vault->entries().isEmpty());
}

void TestVault::listsNotebooksAndNotesInNaturalOrder()
{
    write(u"Ch 10.md"_s);
    write(u"Ch 2.md"_s);
    write(u"ashfall/Characters/Mara.md"_s);
    write(u"Recipes/pancakes.md"_s);
    write(u"Recipes/Bread.md"_s);
    write(u"Recipes/photo.png"_s);

    const QList<VaultEntry> expected{
        {u"ashfall"_s, true},
        {u"ashfall/Characters"_s, true},
        {u"ashfall/Characters/Mara.md"_s, false},
        {u"Ch 2.md"_s, false},
        {u"Ch 10.md"_s, false},
        {u"Recipes"_s, true},
        {u"Recipes/Bread.md"_s, false},
        {u"Recipes/pancakes.md"_s, false},
    };
    QCOMPARE(m_vault->entries(), expected);
    QCOMPARE(m_vault->notes().size(), 5);
}

void TestVault::leavesHiddenFilesAndAttachmentsOut()
{
    write(u".jot/vault.yaml"_s, "format: 1\n");
    write(u".git/HEAD"_s);
    write(u".hidden.md"_s);
    write(u"Mara.md"_s);
    write(u"Mara.assets/sketch.png"_s);
    // A folder ending in .assets with no note of that name is a notebook.
    write(u"Old.assets/Note.md"_s);

    QCOMPARE(m_vault->entries(), (QList<VaultEntry>{{u"Mara.md"_s, false},
                                                    {u"Old.assets"_s, true},
                                                    {u"Old.assets/Note.md"_s, false}}));
}

void TestVault::listsDataFilesBesideNotes()
{
    write(u"Cast.json"_s);
    write(u"Mara.md"_s);
    write(u"Places.YAML"_s);
    write(u"data/rows.csv"_s);
    write(u"data/rows.tsv"_s);
    write(u"data/config.yml"_s);
    // Other files are attachments, and a dot alone is no extension.
    write(u"data/notes.txt"_s);
    write(u"data/.json"_s);
    write(u"sketch.png"_s);

    QCOMPARE(m_vault->notes(),
             (QStringList{u"Cast.json"_s, u"data/config.yml"_s, u"data/rows.csv"_s,
                          u"data/rows.tsv"_s, u"Mara.md"_s, u"Places.YAML"_s}));
    QVERIFY(Vault::isDataFile(u"a/b.Json"_s));
    QVERIFY(!Vault::isDataFile(u"a.json/b"_s));
    QVERIFY(!Vault::isDataFile(u"Mara.md"_s));
    QCOMPARE(Vault::titleFromPath(u"data/rows.csv"_s), u"rows.csv"_s);
}

void TestVault::ordersNamesNaturally_data()
{
    QTest::addColumn<QString>("before");
    QTest::addColumn<QString>("after");
    QTest::newRow("numbers") << u"Ch 2"_s << u"Ch 10"_s;
    QTest::newRow("case") << u"apple"_s << u"Banana"_s;
    QTest::newRow("leading zeros") << u"v007"_s << u"v8"_s;
    QTest::newRow("prefix") << u"Ch"_s << u"Ch 1"_s;
    QTest::newRow("accents") << u"\u00e9t\u00e9"_s << u"\u00e9t\u00e9s"_s;
}

void TestVault::ordersNamesNaturally()
{
    QFETCH(QString, before);
    QFETCH(QString, after);
    QVERIFY(naturalCompare(before, after) < 0);
    QVERIFY(naturalCompare(after, before) > 0);
    QCOMPARE(naturalCompare(before, before), 0);
}

void TestVault::namesFilesSafely_data()
{
    QTest::addColumn<QString>("title");
    QTest::addColumn<QString>("name");
    QTest::newRow("plain") << u"Pancakes"_s << u"Pancakes"_s;
    QTest::newRow("slash") << u"Q&A / Mara"_s << u"Q&A - Mara"_s;
    QTest::newRow("tight slash") << u"either/or"_s << u"either - or"_s;
    QTest::newRow("refused") << u"What? \"Now\": <a|b>*"_s << u"What Now ab"_s;
    QTest::newRow("dots and spaces") << u" ..hidden.. "_s << u"hidden"_s;
    QTest::newRow("control") << u"tab\there"_s << u"tabhere"_s;
    QTest::newRow("nothing left") << u"???"_s << u"Untitled"_s;
}

void TestVault::namesFilesSafely()
{
    QFETCH(QString, title);
    QFETCH(QString, name);
    QCOMPARE(Vault::fileNameFor(title), name);
}

void TestVault::newNotesNeverOverwrite()
{
    write(u"Inbox/Pancakes.md"_s);
    write(u"Inbox/Pancakes 2.md"_s);
    QCOMPARE(m_vault->freePath(u"Inbox"_s, u"Pancakes"_s), u"Inbox/Pancakes 3.md"_s);
    QCOMPARE(m_vault->freePath({}, u"Pancakes"_s), u"Pancakes.md"_s);
}

void TestVault::createdNotesCarryTheirDateAndTitle()
{
    QString error;
    const QDateTime when(QDate(2026, 10, 14), QTime(15, 30), QTimeZone::UTC);
    const std::optional<QString> plain = m_vault->createNote(u"Inbox"_s, u"Ideas"_s, when, &error);
    QVERIFY2(plain, qPrintable(error));
    QCOMPARE(*plain, u"Inbox/Ideas.md"_s);
    QCOMPARE(notefile::read(m_vault->absolute(*plain), &error),
             std::optional(u"---\ncreated: 2026-10-14T15:30:00Z\n---\n"_s));

    const std::optional<QString> awkward = m_vault->createNote({}, u"Q&A: Mara"_s, when, &error);
    QVERIFY(awkward);
    QCOMPARE(*awkward, u"Q&A Mara.md"_s);
    const QString text = *notefile::read(m_vault->absolute(*awkward), &error);
    QCOMPARE(readFrontMatter(splitFrontMatter(text).frontMatter).value(u"title"_s).toString(),
             u"Q&A: Mara"_s);
}

void TestVault::movingANoteTakesItsAttachments()
{
    write(u"Mara.md"_s);
    write(u"Mara.assets/sketch.png"_s);
    QString error;
    QVERIFY2(m_vault->moveNote(u"Mara.md"_s, u"Cast/Mara Vey.md"_s, &error), qPrintable(error));
    QVERIFY(QFileInfo::exists(m_vault->absolute(u"Cast/Mara Vey.md"_s)));
    QVERIFY(QFileInfo::exists(m_vault->absolute(u"Cast/Mara Vey.assets/sketch.png"_s)));
    QVERIFY(!QFileInfo::exists(m_vault->absolute(u"Mara.assets"_s)));

    write(u"Other.md"_s);
    QVERIFY(!m_vault->moveNote(u"Other.md"_s, u"Cast/Mara Vey.md"_s, &error));
    QVERIFY(error.contains(u"already exists"_s));
}

void TestVault::keepsItsOwnTrashWhereTheDriveHasNone()
{
    m_vault->setSystemTrash(false);
    write(u"A/Mara.md"_s, "first\n");
    write(u"A/Mara.assets/sketch.png"_s);
    write(u"B/Mara.md"_s, "second\n");
    QString error;
    QVERIFY2(m_vault->trashNote(u"A/Mara.md"_s, &error), qPrintable(error));
    QVERIFY2(m_vault->trashNote(u"B/Mara.md"_s, &error), qPrintable(error));

    QStringList trashed;
    QDirIterator files(m_vault->absolute(u".jot/trash"_s), QDir::Files,
                       QDirIterator::Subdirectories);
    while (files.hasNext())
        trashed.append(QFileInfo(files.next()).fileName());
    trashed.sort();
    // Both notes kept, though they share a name, and the attachments with them.
    QCOMPARE(trashed.size(), 3);
    QVERIFY(trashed.contains(u"sketch.png"_s));
    QCOMPARE(trashed.filter(u"Mara.md"_s).size(), 2);
    QVERIFY(!QFileInfo::exists(m_vault->absolute(u"A/Mara.md"_s)));
    QVERIFY(!QFileInfo::exists(m_vault->absolute(u"A/Mara.assets"_s)));
    // The trash is out of sight: not a notebook, not a note.
    QCOMPARE(m_vault->entries(), (QList<VaultEntry>{{u"A"_s, true}, {u"B"_s, true}}));
}

void TestVault::listsAndRestoresItsOwnTrash()
{
    m_vault->setSystemTrash(false);
    write(u"A/Mara.md"_s, "first\n");
    write(u"A/Mara.assets/sketch.png"_s);
    write(u"B/Mara.md"_s, "second\n");
    QString error;
    QVERIFY2(m_vault->trashNote(u"A/Mara.md"_s, &error), qPrintable(error));
    QVERIFY2(m_vault->trashNote(u"B/Mara.md"_s, &error), qPrintable(error));
    const VaultTrash trash(*m_vault, {});
    QList<TrashedNote> found = trash.list();
    QCOMPARE(found.size(), 2);
    const auto at = [&found](const QString &path) {
        return *std::ranges::find_if(found,
                                     [&path](const TrashedNote &n) { return n.path == path; });
    };
    // Each keeps the folder it was in, and its attachments.
    QVERIFY(!at(u"A/Mara.md"_s).assets.isEmpty());
    QVERIFY(at(u"B/Mara.md"_s).assets.isEmpty());
    QVERIFY(at(u"A/Mara.md"_s).deleted.isValid());

    QCOMPARE(trash.restore(at(u"A/Mara.md"_s), &error), std::optional(u"A/Mara.md"_s));
    QVERIFY(QFileInfo::exists(m_vault->absolute(u"A/Mara.assets/sketch.png"_s)));
    QCOMPARE(trash.list().size(), 1);
    // Its place taken since, it comes back beside the note there.
    write(u"B/Mara.md"_s, "new\n");
    QCOMPARE(trash.restore(at(u"B/Mara.md"_s), &error), std::optional(u"B/Mara 2.md"_s));
    QFile back(m_vault->absolute(u"B/Mara 2.md"_s));
    QVERIFY(back.open(QIODevice::ReadOnly));
    QCOMPARE(back.readAll(), QByteArray("second\n"));
    QVERIFY(trash.list().isEmpty());
    // What is gone already cannot come back twice.
    QVERIFY(!trash.restore(at(u"B/Mara.md"_s), &error));
    QVERIFY(!error.isEmpty());
}

void TestVault::readsEveryLayoutOfItsOwnTrash()
{
    // As Jot 0.3 kept it, with only the name; as a sync keeps it, with the path; and a folder
    // whose name is no stamp, which goes by when it was made.
    write(u".jot/trash/20261001-120000/2 Mara.md"_s);
    write(u".jot/trash/2026-10-02 120000 2/Story/Pitch.md"_s);
    write(u".jot/trash/2026-10-02 120000 2/Story/Pitch.assets/face.png"_s);
    write(u".jot/trash/2026-10-02 120000 2/Story/Pitch.assets/inside.md"_s);
    write(u".jot/trash/odd/Notes/cast.yaml"_s);
    write(u".jot/trash/odd/Notes/picture.png"_s);
    const QList<TrashedNote> found = VaultTrash(*m_vault, {}).list();
    QStringList paths;
    for (const TrashedNote &note : found)
        paths << note.path;
    // Newest first; attachments are no notes of their own.
    QCOMPARE(paths, (QStringList{u"Notes/cast.yaml"_s, u"Story/Pitch.md"_s, u"2 Mara.md"_s}));
    QCOMPARE(found.at(1).deleted, QDateTime(QDate(2026, 10, 2), QTime(12, 0)));
    QCOMPARE(found.at(2).deleted, QDateTime(QDate(2026, 10, 1), QTime(12, 0)));
    QVERIFY(found.at(1).assets.endsWith(u"Story/Pitch.assets"_s));
}

void TestVault::findsTheVaultsNotesInTheDesktopTrash()
{
    QTemporaryDir desktop;
    const QString root = m_vault->root();
    // As the desktop records what it trashed: the old path, percent-encoded, and when.
    const auto trashed = [&desktop](const QString &name, const QString &was, const QString &when,
                                    bool folder = false) {
        const QString file = desktop.filePath(u"files/"_s + name);
        QVERIFY(QDir().mkpath(folder ? file : QFileInfo(file).path()));
        if (!folder) {
            QFile out(file);
            QVERIFY(out.open(QIODevice::WriteOnly));
            out.write("x\n");
        }
        QVERIFY(QDir().mkpath(desktop.filePath(u"info"_s)));
        QFile info(desktop.filePath(u"info/"_s + name + u".trashinfo"_s));
        QVERIFY(info.open(QIODevice::WriteOnly));
        info.write("[Trash Info]\nPath=" + QUrl::toPercentEncoding(was, "/") +
                   "\nDeletionDate=" + when.toUtf8() + "\n");
    };
    trashed(u"Big Day.md"_s, root + u"/Plans/Big Day.md"_s, u"2026-11-01T10:00:00"_s);
    trashed(u"Big Day.assets"_s, root + u"/Plans/Big Day.assets"_s, u"2026-11-01T10:00:01"_s, true);
    trashed(u"Cast"_s, root + u"/Story/Cast"_s, u"2026-11-02T09:00:00"_s, true);
    QVERIFY(QDir().mkpath(desktop.filePath(u"files/Cast/Mara.assets"_s)));
    QFile mara(desktop.filePath(u"files/Cast/Mara.md"_s));
    QVERIFY(mara.open(QIODevice::WriteOnly));
    mara.close();
    // Not from this vault, or Jot's own folder.
    trashed(u"Elsewhere.md"_s, u"/somewhere/else/Elsewhere.md"_s, u"2026-11-03T09:00:00"_s);
    trashed(u"state.md"_s, root + u"/.jot/state.md"_s, u"2026-11-03T09:00:00"_s);

    const VaultTrash trash(*m_vault, desktop.path());
    const QList<TrashedNote> found = trash.list();
    QCOMPARE(found.size(), 2);
    QCOMPARE(found.at(0).path, u"Story/Cast/Mara.md"_s);
    QVERIFY(found.at(0).assets.endsWith(u"Cast/Mara.assets"_s));
    QCOMPARE(found.at(1).path, u"Plans/Big Day.md"_s);
    QCOMPARE(found.at(1).deleted, QDateTime(QDate(2026, 11, 1), QTime(10, 0)));
    QVERIFY(!found.at(1).record.isEmpty());
    QVERIFY(!found.at(1).assetsRecord.isEmpty());

    // Back with its attachments, and the desktop's records of both gone.
    QString error;
    QCOMPARE(trash.restore(found.at(1), &error), std::optional(u"Plans/Big Day.md"_s));
    QVERIFY(QFileInfo(m_vault->absolute(u"Plans/Big Day.assets"_s)).isDir());
    QVERIFY(!QFileInfo::exists(desktop.filePath(u"info/Big Day.md.trashinfo"_s)));
    QVERIFY(!QFileInfo::exists(desktop.filePath(u"info/Big Day.assets.trashinfo"_s)));
    // One note out of a trashed notebook.
    QCOMPARE(trash.restore(found.at(0), &error), std::optional(u"Story/Cast/Mara.md"_s));
    QVERIFY(QFileInfo::exists(desktop.filePath(u"info/Cast.trashinfo"_s)));
    QCOMPARE(trash.list().size(), 0);

    // Attachments that cannot follow stay in the trash with their record, and the note says so.
    if (geteuid() == 0)
        QSKIP("root can move out of a folder it may not write");
    trashed(u"Recipe.md"_s, root + u"/Recipe.md"_s, u"2026-11-04T10:00:00"_s);
    QTemporaryDir locked;
    const QString stuck = locked.filePath(u"Recipe.assets"_s);
    QVERIFY(QDir().mkpath(stuck));
    QFile::setPermissions(locked.path(), QFile::ReadOwner | QFile::ExeOwner);
    TrashedNote recipe = trash.list().first();
    recipe.assets = stuck;
    recipe.assetsRecord = desktop.filePath(u"info/Recipe.assets.trashinfo"_s);
    QFile record(recipe.assetsRecord);
    QVERIFY(record.open(QIODevice::WriteOnly));
    record.close();
    error.clear();
    QCOMPARE(trash.restore(recipe, &error), std::optional(u"Recipe.md"_s));
    QFile::setPermissions(locked.path(), QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
    QVERIFY(error.contains(u"attachments"_s));
    QVERIFY(QFileInfo(stuck).isDir());
    QVERIFY(QFileInfo::exists(recipe.assetsRecord));
    QVERIFY(!QFileInfo::exists(desktop.filePath(u"info/Recipe.md.trashinfo"_s)));
}

void TestVault::trashesANotebookWithEverythingInIt()
{
    m_vault->setSystemTrash(false);
    write(u"Story/Cast/Mara.md"_s);
    write(u"Story/Cast/Mara.assets/face.png"_s);
    write(u"Story/Pitch.md"_s);
    QVERIFY(QDir().mkpath(m_vault->absolute(u".jot"_s)));
    QString error;

    // Only a notebook inside the vault goes: not the vault, its own folder, a path that
    // leaves it, or a note.
    for (const QString &path :
         {QString(), u".jot"_s, u"../x"_s, u"Story/Pitch.md"_s, u"Missing"_s}) {
        error.clear();
        QVERIFY2(!m_vault->trashFolder(path, &error), qPrintable(path));
        QVERIFY2(!error.isEmpty(), qPrintable(path));
    }
    QVERIFY(QFileInfo::exists(m_vault->absolute(u"Story/Pitch.md"_s)));
    QVERIFY(QFileInfo::exists(m_vault->absolute(u".jot"_s)));

    QVERIFY2(m_vault->trashFolder(u"Story/Cast"_s, &error), qPrintable(error));
    QVERIFY(!QFileInfo::exists(m_vault->absolute(u"Story/Cast"_s)));
    QVERIFY(QFileInfo::exists(m_vault->absolute(u"Story/Pitch.md"_s)));
    QStringList trashed;
    QDirIterator files(m_vault->absolute(u".jot/trash"_s), QDir::Files,
                       QDirIterator::Subdirectories);
    while (files.hasNext())
        trashed.append(QFileInfo(files.next()).fileName());
    trashed.sort();
    QCOMPARE(trashed, (QStringList{u"Mara.md"_s, u"face.png"_s}));

    // A vault from a newer Jot is only read.
    write(u".jot/vault.yaml"_s, "format: 2\n");
    const Vault newer(m_dir->path());
    error.clear();
    QVERIFY(!newer.trashFolder(u"Story"_s, &error));
    QVERIFY(error.contains(u"newer Jot"_s));
    QVERIFY(QFileInfo::exists(m_vault->absolute(u"Story/Pitch.md"_s)));
}

void TestVault::keepsAttachmentsBesideTheirNote()
{
    write(u"Cast/Mara Vey.md"_s);
    QString error;
    const std::optional<QString> first =
        m_vault->addAttachment(u"Cast/Mara Vey.md"_s, u"sketch.png"_s, "png", &error);
    QVERIFY2(first, qPrintable(error));
    QCOMPARE(*first, u"Cast/Mara Vey.assets/sketch.png"_s);
    QCOMPARE(m_vault->addAttachment(u"Cast/Mara Vey.md"_s, u"sketch.png"_s, "png2", &error),
             std::optional(u"Cast/Mara Vey.assets/sketch 2.png"_s));
    // A name with a path in it stays in the note's folder.
    QCOMPARE(m_vault->addAttachment(u"Cast/Mara Vey.md"_s, u"../../escape.sh"_s, "x", &error),
             std::optional(u"Cast/Mara Vey.assets/escape.sh"_s));
    QFile written(m_vault->absolute(u"Cast/Mara Vey.assets/sketch 2.png"_s));
    QVERIFY(written.open(QIODevice::ReadOnly));
    QCOMPARE(written.readAll(), QByteArray("png2"));
    // And the note still lists alone, its attachments under it.
    QCOMPARE(m_vault->notes(), QStringList{u"Cast/Mara Vey.md"_s});
}

void TestVault::writesLinkTargetsOtherToolsRead_data()
{
    QTest::addColumn<QString>("from");
    QTest::addColumn<QString>("to");
    QTest::addColumn<QString>("target");
    QTest::newRow("beside") << u"Cast/Mara Vey.md"_s << u"Cast/Mara Vey.assets/a b.png"_s
                            << u"Mara%20Vey.assets/a%20b.png"_s;
    QTest::newRow("from the top") << u"Pitch.md"_s << u"Cast/Mara.md"_s << u"Cast/Mara.md"_s;
    QTest::newRow("up") << u"Acts/One.md"_s << u"Cast/Mara.md"_s << u"../Cast/Mara.md"_s;
    QTest::newRow("parentheses") << u"Pitch.md"_s << u"Plan (draft).md"_s << u"<Plan (draft).md>"_s;
}

void TestVault::writesLinkTargetsOtherToolsRead()
{
    QFETCH(QString, from);
    QFETCH(QString, to);
    QFETCH(QString, target);
    QCOMPARE(Vault::linkTarget(from, to), target);
}

void TestVault::relativePathsStayInside()
{
    QCOMPARE(m_vault->relative(m_vault->absolute(u"a/b.md"_s)), u"a/b.md"_s);
    QCOMPARE(m_vault->relative(m_dir->path() + u"/../elsewhere.md"_s), QString());
    QCOMPARE(m_vault->relative(u"/etc/passwd"_s), QString());
}

void TestVault::newerVaultsAreOnlyRead()
{
    write(u".jot/vault.yaml"_s, "format: 2\n");
    QCOMPARE(m_vault->format(), 2);
    QVERIFY(!m_vault->writable());
    QString error;
    QVERIFY(!m_vault->createNote({}, u"x"_s, QDateTime::currentDateTimeUtc(), &error));
    QVERIFY(error.contains(u"newer Jot"_s));
}

void TestVault::readAndWriteKeepEveryByte()
{
    const QByteArray bytes = "\xef\xbb\xbf---\r\ntitle: \xc3\xa9t\xc3\xa9\r\n---\r\n\r\nBody  \r\n";
    write(u"n.md"_s, bytes);
    QString error;
    const std::optional<QString> text = notefile::read(m_vault->absolute(u"n.md"_s), &error);
    QVERIFY2(text, qPrintable(error));
    QVERIFY(text->startsWith(QChar(0xfeff)));
    QVERIFY(notefile::write(m_vault->absolute(u"copy.md"_s), *text, &error));
    QFile copy(m_vault->absolute(u"copy.md"_s));
    QVERIFY(copy.open(QIODevice::ReadOnly));
    QCOMPARE(copy.readAll(), bytes);
}

void TestVault::refusesTextThatIsNotUtf8()
{
    write(u"latin1.md"_s, "caf\xe9\n");
    QString error;
    QVERIFY(!notefile::read(m_vault->absolute(u"latin1.md"_s), &error));
    QVERIFY(error.contains(u"not UTF-8"_s));
}

void TestVault::writingThroughALinkKeepsTheLink()
{
    write(u"real/Note.md"_s, "old\n");
    const QString link = m_vault->absolute(u"Link.md"_s);
    QVERIFY(QFile::link(m_vault->absolute(u"real/Note.md"_s), link));
    QString error;
    QVERIFY2(notefile::write(link, u"new\n"_s, &error), qPrintable(error));
    QVERIFY(QFileInfo(link).isSymLink());
    QCOMPARE(notefile::read(m_vault->absolute(u"real/Note.md"_s), &error),
             std::optional(u"new\n"_s));
}

void TestVault::writingAHardLinkedFileKeepsTheLinks()
{
    write(u"a.md"_s, "old\n");
    QVERIFY(::link(QFile::encodeName(m_vault->absolute(u"a.md"_s)).constData(),
                   QFile::encodeName(m_vault->absolute(u"b.md"_s)).constData()) == 0);
    QString error;
    QVERIFY(notefile::write(m_vault->absolute(u"a.md"_s), u"new\n"_s, &error));
    QCOMPARE(notefile::read(m_vault->absolute(u"b.md"_s), &error), std::optional(u"new\n"_s));
}

QTEST_GUILESS_MAIN(TestVault)
#include "tst_vault.moc"
