#include "jot/VaultIndex.h"

#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTest>

#include <utime.h>

using namespace jot;
using namespace Qt::StringLiterals;

class TestVaultIndex : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void init();
    void cleanup();

    void readsTitlesTagsAndTasks();
    void refreshReadsOnlyWhatChanged();
    void dropsNotesThatAreGone();
    void searchesWordsAtTheStartOfWords();
    void searchFiltersByTagFolderTaskAndType();
    void searchRanksTitlesFirst();
    void searchTakesAwkwardInput();
    void resolvesLinksTheWayObsidianDoes();
    void backlinksFollowRenames();
    void listsUnresolvedLinks();
    void nestedTagsCountForTheirParents();
    void countsTagsWithTheParentsOfNestedOnes();
    void listsOpenTasksAndTaskNotes();
    void rebuildsAnIndexFromAnotherSchema();
    void sortsNotesWithThePinnedFirst();

private:
    void write(const QString &path, const QString &text);
    void open();

    std::unique_ptr<QTemporaryDir> m_dir;
    std::unique_ptr<Vault> m_vault;
    std::unique_ptr<VaultIndex> m_index;
};

void TestVaultIndex::init()
{
    // The index logs what it could not store, so any warning is a failure here.
    QTest::failOnWarning(QRegularExpression(u".*"_s));
    m_dir = std::make_unique<QTemporaryDir>();
    m_vault = std::make_unique<Vault>(m_dir->filePath(u"vault"_s));
    QVERIFY(QDir().mkpath(m_vault->root()));
}

void TestVaultIndex::cleanup()
{
    m_index.reset();
}

void TestVaultIndex::write(const QString &path, const QString &text)
{
    const QString file = m_vault->absolute(path);
    QVERIFY(QDir().mkpath(QFileInfo(file).path()));
    QString error;
    QVERIFY2(notefile::write(file, text, &error), qPrintable(error));
}

void TestVaultIndex::open()
{
    m_index = std::make_unique<VaultIndex>(*m_vault, m_dir->filePath(u"index.sqlite"_s));
    QString error;
    QVERIFY2(m_index->open(&error), qPrintable(error));
    m_index->refresh();
}

void TestVaultIndex::readsTitlesTagsAndTasks()
{
    write(u"Cast/Mara.md"_s, u"---\ntitle: Mara Vey\ntags: [ashfall, \"#cast/lead\"]\n"
                             u"aliases:\n  - the Ferrywoman\ntype: character\n---\n"
                             u"Keeps the #ferry log.\n"_s);
    write(u"Errand.md"_s, u"---\ntask: Open\ndue: 2026-10-20\n---\nBuy paper\n"_s);
    write(u"Plain.md"_s, u"Just text\n"_s);
    open();

    QCOMPARE(m_index->noteCount(), 3);
    const NoteInfo mara = *m_index->note(u"Cast/Mara.md"_s);
    QCOMPARE(mara.title, u"Mara Vey"_s);
    QCOMPARE(mara.tags, (QStringList{u"ashfall"_s, u"cast/lead"_s, u"ferry"_s}));
    QCOMPARE(mara.aliases, QStringList{u"the Ferrywoman"_s});
    QCOMPARE(mara.type, u"character"_s);
    const NoteInfo errand = *m_index->note(u"Errand.md"_s);
    QCOMPARE(errand.task, u"open"_s);
    QCOMPARE(errand.due, u"2026-10-20"_s);
    QCOMPARE(m_index->note(u"Plain.md"_s)->title, u"Plain"_s);
    QVERIFY(m_index->note(u"Plain.md"_s)->modified > 0);
    QCOMPARE(m_index->notesIn(u"Cast"_s).size(), 1);
    QCOMPARE(m_index->notesIn(u"Ca"_s).size(), 0);
    QCOMPARE(m_index->notesIn({}).size(), 3);
    QCOMPARE(m_index->noteCounts().value(u"Cast"_s), 1);
    QVERIFY(!m_index->note(u"Missing.md"_s));
}

void TestVaultIndex::refreshReadsOnlyWhatChanged()
{
    write(u"A.md"_s, u"a\n"_s);
    write(u"B.md"_s, u"b\n"_s);
    open();
    QCOMPARE(m_index->refresh(), QStringList());

    write(u"B.md"_s, u"b, changed\n"_s);
    QCOMPARE(m_index->refresh(), QStringList{u"B.md"_s});
    QCOMPARE(m_index->search(u"changed"_s).size(), 1);

    // The same size and a new time is a change too, as a quick edit can be.
    write(u"A.md"_s, u"z\n"_s);
    struct utimbuf times{1000, 1000};
    QVERIFY(::utime(QFile::encodeName(m_vault->absolute(u"A.md"_s)).constData(), &times) == 0);
    QCOMPARE(m_index->refresh(), QStringList{u"A.md"_s});
}

void TestVaultIndex::dropsNotesThatAreGone()
{
    write(u"Gone.md"_s, u"#tag [[Kept]]\n- [ ] task\n"_s);
    write(u"Kept.md"_s, u"kept\n"_s);
    open();
    QVERIFY(QFile::remove(m_vault->absolute(u"Gone.md"_s)));
    QCOMPARE(m_index->refresh(), QStringList{u"Gone.md"_s});
    QCOMPARE(m_index->noteCount(), 1);
    QVERIFY(m_index->tags().isEmpty());
    QVERIFY(m_index->backlinks(u"Kept.md"_s).isEmpty());
    QVERIFY(m_index->openTasks().isEmpty());
    QVERIFY(m_index->search(u"task"_s).isEmpty());
}

void TestVaultIndex::searchesWordsAtTheStartOfWords()
{
    write(u"Ferry.md"_s, u"The ferrywoman crossed the río at dawn.\n"_s);
    write(u"Other.md"_s, u"Nothing about boats.\n"_s);
    open();

    QCOMPARE(m_index->search(u"ferryw"_s).size(), 1);
    QCOMPARE(m_index->search(u"rio dawn"_s).size(), 1); // diacritics ignored, every word needed
    QCOMPARE(m_index->search(u"rio boats"_s).size(), 0);
    QCOMPARE(m_index->search(u"oman"_s).size(), 0); // words match from their start
    const SearchHit hit = m_index->search(u"dawn"_s).first();
    QCOMPARE(hit.path, u"Ferry.md"_s);
    QVERIFY2(hit.snippet.contains(u"<mark>dawn</mark>"_s), qPrintable(hit.snippet));
}

void TestVaultIndex::searchFiltersByTagFolderTaskAndType()
{
    write(u"Ashfall/Mara.md"_s, u"---\ntype: Character\ntags: [cast/lead]\n---\nferry\n"_s);
    write(u"Ashfall/Soren.md"_s, u"---\ntype: character\n---\nferry #cast\n"_s);
    write(u"Notes/Errand.md"_s, u"---\ntask: open\n---\nferry ticket\n"_s);
    open();

    const auto paths = [this](const QString &query) {
        QStringList found;
        for (const SearchHit &hit : m_index->search(query))
            found.append(hit.path);
        found.sort();
        return found;
    };
    QCOMPARE(paths(u"ferry tag:cast"_s),
             (QStringList{u"Ashfall/Mara.md"_s, u"Ashfall/Soren.md"_s}));
    QCOMPARE(paths(u"tag:cast/lead"_s), QStringList{u"Ashfall/Mara.md"_s});
    QCOMPARE(paths(u"ferry in:Notes"_s), QStringList{u"Notes/Errand.md"_s});
    QCOMPARE(paths(u"task:open"_s), QStringList{u"Notes/Errand.md"_s});
    QCOMPARE(paths(u"type:character"_s),
             (QStringList{u"Ashfall/Mara.md"_s, u"Ashfall/Soren.md"_s}));
    QCOMPARE(paths(u"in:Ash"_s), QStringList());
}

void TestVaultIndex::searchRanksTitlesFirst()
{
    write(u"Notes about many things.md"_s, u"Ferry ferry ferry, and the ferry again.\n"_s);
    write(u"Ferry.md"_s, u"A crossing.\n"_s);
    open();
    QCOMPARE(m_index->search(u"ferry"_s).first().path, u"Ferry.md"_s);
}

void TestVaultIndex::searchTakesAwkwardInput()
{
    write(u"A.md"_s, u"Say \"hi\" AND NOT: (this) * near\n"_s);
    open();
    // Words that mean something to FTS5 are only words here, and nothing throws.
    for (const QString &query : {u"\"hi"_s, u"AND"_s, u"NOT:"_s, u"(this)"_s, u"*"_s, u"\""_s,
                                 u"NEAR(a b)"_s, u"tag:"_s, QString()})
        (void)m_index->search(query);
    QCOMPARE(m_index->search(u"AND NOT"_s).size(), 1);
    QCOMPARE(m_index->search(u"\"say hi\""_s).size(), 1);
}

void TestVaultIndex::resolvesLinksTheWayObsidianDoes()
{
    write(u"Cast/Mara.md"_s, u"---\naliases: [the Ferrywoman]\n---\n"_s);
    write(u"Cast/Soren.md"_s, u"brother\n"_s);
    write(u"Old/Soren.md"_s, u"an older Soren\n"_s);
    write(u"Places/The Ferry.md"_s, u"ferry\n"_s);
    write(u"Cast/Mara.assets/sketch.png"_s, u"png\n"_s);
    write(u"Cast/Notes.md"_s, u"notes\n"_s);
    open();

    const QString from = u"Cast/Notes.md"_s;
    QCOMPARE(m_index->resolve(u"Mara"_s, from), u"Cast/Mara.md"_s);
    QCOMPARE(m_index->resolve(u"mara"_s, from), u"Cast/Mara.md"_s);
    QCOMPARE(m_index->resolve(u"Mara#Voice"_s, from), u"Cast/Mara.md"_s);
    QCOMPARE(m_index->resolve(u"Mara.md"_s, from), u"Cast/Mara.md"_s);
    // Same folder wins, then the shortest path.
    QCOMPARE(m_index->resolve(u"Soren"_s, from), u"Cast/Soren.md"_s);
    QCOMPARE(m_index->resolve(u"Soren"_s, u"Places/x.md"_s), u"Old/Soren.md"_s);
    QCOMPARE(m_index->resolve(u"Old/Soren"_s, from), u"Old/Soren.md"_s);
    QCOMPARE(m_index->resolve(u"../Places/The%20Ferry.md"_s, from), u"Places/The Ferry.md"_s);
    QCOMPARE(m_index->resolve(u"The Ferry"_s, from), u"Places/The Ferry.md"_s);
    QCOMPARE(m_index->resolve(u"Mara.assets/sketch.png"_s, from), u"Cast/Mara.assets/sketch.png"_s);
    // A dotted note name is a note, even beside a file of that name.
    write(u"Cast/v1.2 plan.md"_s, u"plan\n"_s);
    write(u"Cast/v1.2 plan"_s, u"stray\n"_s);
    m_index->update({u"Cast/v1.2 plan.md"_s});
    QCOMPARE(m_index->resolve(u"v1.2 plan"_s, from), u"Cast/v1.2 plan.md"_s);
    // An attachment by name alone, from its own note.
    QCOMPARE(m_index->resolve(u"sketch.png"_s, u"Cast/Mara.md"_s),
             u"Cast/Mara.assets/sketch.png"_s);
    QCOMPARE(m_index->resolve(u"the ferrywoman"_s, from), u"Cast/Mara.md"_s);
    QCOMPARE(m_index->resolve(u"#Heading"_s, from), from);
    QCOMPARE(m_index->resolve(u"Nobody"_s, from), QString());
    QCOMPARE(m_index->resolve(u"Cast"_s, from), QString()); // a folder is not a note
    QCOMPARE(m_index->resolve(u"../../outside.md"_s, from), QString());
}

void TestVaultIndex::backlinksFollowRenames()
{
    write(u"Mara.md"_s, u"mara\n"_s);
    write(u"Scene.md"_s, u"line one\n[[Mara]] and ![[Mara]]\n"_s);
    write(u"Other.md"_s, u"[her](Mara.md)\n"_s);
    open();

    QList<NoteLink> links = m_index->backlinks(u"Mara.md"_s);
    QCOMPARE(links.size(), 3);
    QCOMPARE(links.first(), (NoteLink{u"Other.md"_s, u"Mara.md"_s, u"Mara.md"_s, u"link"_s, 1}));
    QCOMPARE(links.at(1).line, 2);

    // Moved into a folder, every link still finds it by name.
    QVERIFY(QDir().mkpath(m_vault->absolute(u"Cast"_s)));
    QVERIFY(QFile::rename(m_vault->absolute(u"Mara.md"_s), m_vault->absolute(u"Cast/Mara.md"_s)));
    m_index->update({u"Mara.md"_s, u"Cast/Mara.md"_s});
    QVERIFY(m_index->backlinks(u"Mara.md"_s).isEmpty());
    links = m_index->backlinks(u"Cast/Mara.md"_s);
    QCOMPARE(links.size(), 3);
    QCOMPARE(links.first().source, u"Other.md"_s);
}

void TestVaultIndex::listsUnresolvedLinks()
{
    write(u"A.md"_s, u"[[Someday]] and [[A]]\n"_s);
    open();
    const QList<NoteLink> missing = m_index->unresolved();
    QCOMPARE(missing.size(), 1);
    QCOMPARE(missing.first().raw, u"Someday"_s);

    write(u"Someday.md"_s, u"now it exists\n"_s);
    m_index->update({u"Someday.md"_s});
    QVERIFY(m_index->unresolved().isEmpty());
    QCOMPARE(m_index->backlinks(u"Someday.md"_s).size(), 1);
    // A note read after others keeps its own search text.
    QCOMPARE(m_index->search(u"exists"_s).size(), 1);
    QCOMPARE(m_index->search(u"exists"_s).first().path, u"Someday.md"_s);
    // A note's links to itself are not backlinks.
    QVERIFY(m_index->backlinks(u"A.md"_s).isEmpty());
}

void TestVaultIndex::nestedTagsCountForTheirParents()
{
    write(u"A.md"_s, u"#novel/ch1\n"_s);
    write(u"B.md"_s, u"#novel\n"_s);
    write(u"C.md"_s, u"#novelette\n"_s);
    open();
    QCOMPARE(m_index->tags(), (QStringList{u"novel"_s, u"novel/ch1"_s, u"novelette"_s}));
    QCOMPARE(m_index->tagged(u"novel"_s), (QStringList{u"A.md"_s, u"B.md"_s}));
    QCOMPARE(m_index->tagged(u"NOVEL/ch1"_s), QStringList{u"A.md"_s});
}

void TestVaultIndex::countsTagsWithTheParentsOfNestedOnes()
{
    write(u"A.md"_s, u"#novel/ch10 #novel/ch2\n"_s);
    write(u"B.md"_s, u"---\ntags: [Novel/ch2, draft/old/rejected]\n---\n"_s);
    write(u"C.md"_s, u"#novelette\n"_s);
    open();
    // Parents first, numbers in order, case aside; draft and draft/old are no note's own tags.
    QCOMPARE(m_index->tagCounts(), (QList<TagCount>{{u"draft"_s, 1},
                                                    {u"draft/old"_s, 1},
                                                    {u"draft/old/rejected"_s, 1},
                                                    {u"novel"_s, 2},
                                                    {u"novel/ch2"_s, 2},
                                                    {u"novel/ch10"_s, 1},
                                                    {u"novelette"_s, 1}}));
}

void TestVaultIndex::listsOpenTasksAndTaskNotes()
{
    write(u"Shop.md"_s, u"- [ ] paper\n- [x] pens\n- [ ] ink\n"_s);
    write(u"Later.md"_s, u"---\ntask: open\n---\n"_s);
    write(u"Soon.md"_s, u"---\ntask: open\ndue: 2026-10-15\n---\n"_s);
    write(u"Done.md"_s, u"---\ntask: done\n---\n"_s);
    open();

    QCOMPARE(m_index->openTasks(), (QList<NoteTask>{{u"Shop.md"_s, 1, false, u"paper"_s},
                                                    {u"Shop.md"_s, 3, false, u"ink"_s}}));
    QStringList tasks;
    for (const NoteInfo &note : m_index->openTaskNotes())
        tasks.append(note.path);
    QCOMPARE(tasks, (QStringList{u"Soon.md"_s, u"Later.md"_s}));
}

void TestVaultIndex::rebuildsAnIndexFromAnotherSchema()
{
    write(u"A.md"_s, u"a\n"_s);
    open();
    m_index.reset();
    {
        // Stand in for an index an older Jot left behind.
        QSqlDatabase db = QSqlDatabase::addDatabase(u"QSQLITE"_s, u"old"_s);
        db.setDatabaseName(m_dir->filePath(u"index.sqlite"_s));
        QVERIFY(db.open());
        QSqlQuery(u"PRAGMA user_version = 99"_s, db);
        db.close();
    }
    QSqlDatabase::removeDatabase(u"old"_s);
    open();
    QCOMPARE(m_index->noteCount(), 1);
}

void TestVaultIndex::sortsNotesWithThePinnedFirst()
{
    const qint64 day = 24LL * 3600 * 1000 * 1000000;
    const qint64 oct1 = 1790812800000LL * 1000000; // 2026-10-01
    const auto info = [](const QString &path, const QString &title, const QString &created,
                         qint64 modified) {
        NoteInfo note;
        note.path = path;
        note.title = title;
        note.created = created;
        note.modified = modified;
        return note;
    };
    QList<NoteInfo> notes = {
        info(u"b.md"_s, u"Ch 10"_s, u"2026-09-01"_s, oct1),
        info(u"a.md"_s, u"Ch 2"_s, u"2026-10-05T08:00:00Z"_s, oct1 - day),
        // Without a creation time, when it last changed stands in.
        info(u"c.md"_s, u"Alpha"_s, {}, oct1 + day),
    };
    const auto paths = [&] {
        QStringList found;
        for (const NoteInfo &note : std::as_const(notes))
            found << note.path;
        return found;
    };
    sortNotes(notes, u"updated"_s);
    QCOMPARE(paths(), (QStringList{u"c.md"_s, u"b.md"_s, u"a.md"_s}));
    sortNotes(notes, u"created"_s);
    QCOMPARE(paths(), (QStringList{u"a.md"_s, u"c.md"_s, u"b.md"_s}));
    sortNotes(notes, u"title"_s);
    QCOMPARE(paths(), (QStringList{u"c.md"_s, u"a.md"_s, u"b.md"_s}));
    sortNotes(notes, u"title"_s, {u"b.md"_s, u"a.md"_s});
    QCOMPARE(paths(), (QStringList{u"a.md"_s, u"b.md"_s, u"c.md"_s}));
}

QTEST_GUILESS_MAIN(TestVaultIndex)
#include "tst_vaultindex.moc"
