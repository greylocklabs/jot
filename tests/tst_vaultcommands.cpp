#include "VaultCommands.h"

#include "JexBuilder.h"
#include "jot/Layout.h"

#include <QDir>
#include <QDirIterator>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTest>

using namespace jot;
using namespace Qt::StringLiterals;

class TestVaultCommands : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void init();
    void cleanup();

    void statusSaysWhatTheVaultHolds();
    void listsNotesNewestOrByTitle();
    void listsPinnedNotesFirst();
    void searchesWithFilters();
    void makesNotesWithAPipedBody();
    void showsANoteByTitleAliasOrPath();
    void listsWhatIsLeftToDo();
    void opensANoteInTheApp();
    void listsTagsWithTheirNotes();
    void importsAJoplinExportAfterADryRun();

private:
    int run(const std::function<int(cli::VaultCommands &)> &command);
    QJsonDocument json() const { return QJsonDocument::fromJson(m_out.toUtf8()); }

    std::unique_ptr<QTemporaryDir> m_dir;
    std::unique_ptr<Vault> m_vault;
    std::unique_ptr<VaultIndex> m_index;
    QString m_out;
    QString m_err;
};

void TestVaultCommands::init()
{
    m_dir = std::make_unique<QTemporaryDir>();
    const QString root = m_dir->filePath(u"Notes"_s);
    const QString sample = QStringLiteral(JOT_SAMPLE_VAULT);
    QDirIterator files(sample, QDir::Files, QDirIterator::Subdirectories);
    while (files.hasNext()) {
        const QString from = files.next();
        const QString to = root + from.mid(sample.size());
        QVERIFY(QDir().mkpath(QFileInfo(to).path()));
        QVERIFY(QFile::copy(from, to));
    }
    m_vault = std::make_unique<Vault>(root);
    m_index = std::make_unique<VaultIndex>(*m_vault, m_dir->filePath(u"index.sqlite"_s));
    QString error;
    QVERIFY(m_index->open(&error));
    m_index->refresh();
}

void TestVaultCommands::cleanup()
{
    m_index.reset();
}

int TestVaultCommands::run(const std::function<int(cli::VaultCommands &)> &command)
{
    m_out.clear();
    m_err.clear();
    QTextStream out(&m_out);
    QTextStream err(&m_err);
    cli::VaultCommands commands(*m_vault, *m_index, out, err);
    const int code = command(commands);
    out.flush();
    err.flush();
    return code;
}

void TestVaultCommands::statusSaysWhatTheVaultHolds()
{
    QCOMPARE(run([](auto &c) { return c.status(u"/cache/index.sqlite"_s, true); }), 0);
    const QJsonObject status = json().object();
    QCOMPARE(status.value(u"notes"_s).toInt(), 8);
    QCOMPARE(status.value(u"notebooks"_s).toInt(), 5);
    QCOMPARE(status.value(u"openTasks"_s).toInt(), 3);
    QCOMPARE(status.value(u"openTaskNotes"_s).toInt(), 1);
    QVERIFY(status.value(u"writable"_s).toBool());

    QCOMPARE(run([](auto &c) { return c.status(u"/cache/index.sqlite"_s, false); }), 0);
    QVERIFY2(m_out.contains(u"8 notes in 5 notebooks"_s), qPrintable(m_out));
    QVERIFY(m_out.contains(u"1 task note, 3 tasks"_s));
}

void TestVaultCommands::listsNotesNewestOrByTitle()
{
    QCOMPARE(run([](auto &c) { return c.list(u"Recipes"_s, u"title"_s, true); }), 0);
    const QJsonArray notes = json().array();
    QCOMPARE(notes.size(), 2);
    QCOMPARE(notes.at(0).toObject().value(u"title"_s).toString(), u"Pancakes"_s);
    QCOMPARE(notes.at(0).toObject().value(u"tags"_s).toArray().first().toString(), u"food"_s);

    QCOMPARE(run([](auto &c) { return c.list({}, {}, false); }), 0);
    QCOMPARE(m_out.count(u'\n'), 8);
    QCOMPARE(run([](auto &c) { return c.list(u"Nowhere"_s, {}, false); }), 1);
    QVERIFY(m_err.contains(u"no notebook called Nowhere"_s));
    QCOMPARE(run([](auto &c) { return c.list({}, u"titel"_s, false); }), 2);
    QCOMPARE(run([](auto &c) { return c.list({}, u"created"_s, false); }), 0);
    QCOMPARE(run([](auto &c) { return c.list(u"../.."_s, {}, false); }), 2);
}

void TestVaultCommands::listsPinnedNotesFirst()
{
    Layout layout;
    layout.setPinned(u"Recipes/Sourdough.md"_s, true);
    QString error;
    QVERIFY(layout.write(*m_vault, &error));
    QCOMPARE(run([](auto &c) { return c.list(u"Recipes"_s, u"title"_s, true); }), 0);
    const QJsonArray notes = json().array();
    QCOMPARE(notes.at(0).toObject().value(u"title"_s).toString(), u"Sourdough"_s);
    QVERIFY(notes.at(0).toObject().value(u"pinned"_s).toBool());
    QVERIFY(!notes.at(1).toObject().value(u"pinned"_s).toBool());
}

void TestVaultCommands::searchesWithFilters()
{
    QCOMPARE(run([](auto &c) { return c.search(u"ferry in:Ashfall/Characters"_s, true); }), 0);
    const QJsonArray hits = json().array();
    QCOMPARE(hits.size(), 1);
    QCOMPARE(hits.first().toObject().value(u"path"_s).toString(),
             u"Ashfall/Characters/Mara Vey.md"_s);
    QVERIFY(!hits.first().toObject().value(u"snippet"_s).toString().contains(u"<mark>"_s));

    QCOMPARE(run([](auto &c) { return c.search(u"zzzz"_s, false); }), 0);
    QVERIFY(m_err.contains(u"Nothing matches."_s));
    QCOMPARE(run([](auto &c) { return c.search(QString(), false); }), 2);
}

void TestVaultCommands::makesNotesWithAPipedBody()
{
    QCOMPARE(run([](auto &c) {
                 return c.create(u"Ferry checklist"_s, u"Ashfall"_s, u"Bring a lantern."_s, true);
             }),
             0);
    QCOMPARE(json().object().value(u"path"_s).toString(), u"Ashfall/Ferry checklist.md"_s);
    QString error;
    const QString text =
        *notefile::read(m_vault->absolute(u"Ashfall/Ferry checklist.md"_s), &error);
    QVERIFY(text.startsWith(u"---\ncreated: "_s));
    QVERIFY(text.endsWith(u"---\nBring a lantern.\n"_s));
    QCOMPARE(m_index->search(u"lantern"_s).size(), 1);
    QCOMPARE(run([](auto &c) { return c.create(u"  "_s, {}, {}, false); }), 2);
    // A notebook outside the vault is refused before anything is written.
    QCOMPARE(run([](auto &c) { return c.create(u"Escape"_s, u"../../elsewhere"_s, {}, false); }),
             2);
    QVERIFY(!QFileInfo::exists(m_dir->filePath(u"elsewhere"_s)));
}

void TestVaultCommands::showsANoteByTitleAliasOrPath()
{
    for (const QString &name :
         {u"Mara Vey"_s, u"the Ferrywoman"_s, u"mara v"_s, u"Ashfall/Characters/Mara Vey.md"_s}) {
        QCOMPARE(run([&name](auto &c) { return c.show(name); }), 0);
        QVERIFY2(m_out.contains(u"waxed ledger"_s), qPrintable(name));
    }
    QCOMPARE(run([](auto &c) { return c.show(u"qqqq"_s); }), 1);
}

void TestVaultCommands::listsWhatIsLeftToDo()
{
    QCOMPARE(run([](auto &c) { return c.listTasks(true); }), 0);
    const QJsonArray tasks = json().array();
    QCOMPARE(tasks.size(), 4);
    QCOMPARE(tasks.first().toObject().value(u"kind"_s).toString(), u"note"_s);
    QCOMPARE(tasks.last().toObject().value(u"text"_s).toString(), u"oat milk"_s);
    QCOMPARE(tasks.last().toObject().value(u"line"_s).toInt(), 6);

    // A due that is not a date is never overdue.
    QString error;
    QVERIFY(notefile::write(m_vault->absolute(u"Someday.md"_s),
                            u"---\ntask: open\ndue: someday\n---\n"_s, &error));
    m_index->update({u"Someday.md"_s});
    m_index->update({u"Inbox/Groceries.md"_s});
    QCOMPARE(run([](auto &c) { return c.listTasks(false); }), 0);
    QVERIFY(m_out.contains(u"[ ] Someday  someday"_s));
    QVERIFY(!isOverdue(u"someday"_s, QDate(2026, 10, 15)));
    QVERIFY(isOverdue(u"2026-10-14T09:00:00Z"_s, QDate(2026, 10, 15)));
    QVERIFY(!isOverdue(u"2026-10-15"_s, QDate(2026, 10, 15)));
}

void TestVaultCommands::opensANoteInTheApp()
{
    QString program;
    QStringList arguments;
    const auto launch = [&](const QString &p, const QStringList &a) {
        program = p;
        arguments = a;
        return true;
    };
    QCOMPARE(run([&](auto &c) { return c.open(u"ferrywoman"_s, u"/usr/bin/jot-gui"_s, launch); }),
             0);
    QCOMPARE(program, u"/usr/bin/jot-gui"_s);
    QCOMPARE(arguments, (QStringList{u"--vault"_s, m_vault->root(), u"--note"_s,
                                     u"Ashfall/Characters/Mara Vey.md"_s}));
    QCOMPARE(run([&](auto &c) { return c.open(u"qqqq"_s, u"/usr/bin/jot-gui"_s, launch); }), 1);
    QCOMPARE(run([&](auto &c) { return c.open(u"mara"_s, {}, launch); }), 1);
    QVERIFY(m_err.contains(u"not installed"_s));
    QCOMPARE(run([](auto &c) {
                 return c.open(u"mara"_s, u"/x"_s,
                               [](const QString &, const QStringList &) { return false; });
             }),
             1);
    QVERIFY(m_err.contains(u"could not start /x"_s));
}

void TestVaultCommands::listsTagsWithTheirNotes()
{
    QCOMPARE(run([](auto &c) { return c.listTags(true); }), 0);
    QHash<QString, int> notes;
    for (const QJsonValue &tag : json().array())
        notes.insert(tag[u"tag"_s].toString(), tag[u"notes"_s].toInt());
    QCOMPARE(notes.value(u"food"_s), 2);
    // A tag counts the notes with tags nested in it too.
    QVERIFY(notes.value(u"cast"_s) >= notes.value(u"cast/lead"_s) + 1);

    QCOMPARE(run([](auto &c) { return c.listTags(false); }), 0);
    QVERIFY2(m_out.contains(u"food  2"_s), qPrintable(m_out));

    // A tag only others are nested in is listed too.
    QString error;
    QVERIFY(notefile::write(m_vault->absolute(u"Idea.md"_s), u"#draft/old\n"_s, &error));
    m_index->refresh();
    QCOMPARE(run([](auto &c) { return c.listTags(false); }), 0);
    QVERIFY2(m_out.contains(u"draft  1\n"_s), qPrintable(m_out));
    QVERIFY2(m_out.contains(u"draft/old  1\n"_s), qPrintable(m_out));
}

void TestVaultCommands::importsAJoplinExportAfterADryRun()
{
    const QString times = u"user_created_time: 2026-10-01T09:00:00.000Z\n"
                          u"user_updated_time: 2026-10-02T10:00:00.000Z\nencryption_applied: 0\n"_s;
    const QString notebook = u"0123456789abcdef0123456789abcdef"_s;
    const QString jex = m_dir->filePath(u"travel.jex"_s);
    QFile file(jex);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(tar({
        {notebook + u".md"_s,
         item(u"Travel"_s, {},
              u"id: "_s + notebook + u"\n"_s + times + u"parent_id: \ntype_: 2"_s)},
        {u"n1.md"_s, item(u"Packing list"_s, u"- [ ] passport"_s,
                          u"id: fedcba9876543210fedcba9876543210\nparent_id: "_s + notebook +
                              u"\n"_s + times + u"latitude: 40.6\nlongitude: -73.9\ntype_: 1"_s)},
    }));
    file.close();

    QCOMPARE(run([&](auto &c) { return c.importJoplin(jex, u"From Joplin"_s, true, true); }), 0);
    const QJsonObject planned = json().object();
    QVERIFY(planned[u"dryRun"_s].toBool());
    QCOMPARE(planned[u"notes"_s].toArray().first()[u"outcome"_s].toString(), u"created"_s);
    QCOMPARE(planned[u"notes"_s].toArray().first()[u"path"_s].toString(),
             u"From Joplin/Travel/Packing list.md"_s);
    QVERIFY(!QFileInfo::exists(m_vault->absolute(u"From Joplin"_s)));

    QCOMPARE(run([&](auto &c) { return c.importJoplin(jex, u"From Joplin"_s, false, false); }), 0);
    QVERIFY2(m_out.contains(u"1 new note"_s), qPrintable(m_out));
    QVERIFY(QFileInfo::exists(m_vault->absolute(u"From Joplin/Travel/Packing list.md"_s)));
    QString error;
    const QString text =
        notefile::read(m_vault->absolute(u"From Joplin/Travel/Packing list.md"_s), &error)
            .value_or(QString());
    QVERIFY(!text.contains(u"40.6"_s));
    // The index has it straight away, so ls and search find it.
    QCOMPARE(run([](auto &c) { return c.list(u"From Joplin"_s, {}, true); }), 0);
    QCOMPARE(json().array().size(), 1);

    QCOMPARE(run([&](auto &c) { return c.importJoplin(jex, u"From Joplin"_s, false, false); }), 0);
    QVERIFY2(m_out.contains(u"1 note unchanged since the last import"_s), qPrintable(m_out));

    QCOMPARE(run([](auto &c) { return c.importJoplin({}, {}, false, false); }), 2);
    QCOMPARE(run([](auto &c) { return c.importJoplin(u"/no/such.jex"_s, {}, false, false); }), 1);
    QCOMPARE(run([&](auto &c) { return c.importJoplin(jex, u"../outside"_s, false, false); }), 2);

    // "." is the top of the vault, so its paths match an import without --notebook.
    const QString other = m_dir->filePath(u"other.jex"_s);
    QFile otherFile(other);
    QVERIFY(otherFile.open(QIODevice::WriteOnly));
    otherFile.write(tar(
        {{u"n2.md"_s, item(u"Loose note"_s, u"x"_s,
                           u"id: 00112233445566778899aabbccddeeff\n"_s + times + u"type_: 1"_s)}}));
    otherFile.close();
    QCOMPARE(run([&](auto &c) { return c.importJoplin(other, u"From Joplin/.."_s, true, true); }),
             0);
    QCOMPARE(json().object()[u"notes"_s].toArray().first()[u"path"_s].toString(),
             u"Loose note.md"_s);
}

QTEST_GUILESS_MAIN(TestVaultCommands)
#include "tst_vaultcommands.moc"
