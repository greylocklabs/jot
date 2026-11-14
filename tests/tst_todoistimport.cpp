#include "jot/TodoistImport.h"
#include "jot/Vault.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTest>

using namespace jot;
using namespace Qt::StringLiterals;

namespace {

/// A full sync shaped as Todoist's: an Inbox, projects with sections, one inside another, one
/// archived and one deleted, and tasks with the things a task can carry.
QByteArray account()
{
    QFile file(QStringLiteral(JOT_TODOIST_SYNC));
    if (!file.open(QIODevice::ReadOnly))
        qFatal("cannot read %s", JOT_TODOIST_SYNC);
    return file.readAll();
}

TodoistData read(const QByteArray &json = account())
{
    QString error;
    std::optional<TodoistData> data = readTodoistSync(json, &error);
    if (!data)
        qFatal("%s", qPrintable(error));
    return *data;
}

TodoistData::Task &task(TodoistData &data, const QString &id)
{
    for (auto &t : data.tasks) {
        if (t.id == id)
            return t;
    }
    qFatal("no task %s", qPrintable(id));
}

} // namespace

class TestTodoistImport : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void init();

    void readsAFullSync();
    void refusesWhatIsNotOne();
    void writesEachProjectAsANote();
    void importingAgainKeepsJotsEdits();
    void aDryRunWritesNothing();
    void leavesAnUnreadableInboxAlone();
    void writesOnlyInsideTheVault();

private:
    QString readFile(const QString &path) const;
    void writeFile(const QString &path, const QString &text) const;
    TodoistImport import(const TodoistData &data, bool dryRun = false);

    std::unique_ptr<QTemporaryDir> m_dir;
    std::optional<Vault> m_vault;
};

void TestTodoistImport::init()
{
    m_dir = std::make_unique<QTemporaryDir>();
    QString error;
    m_vault = Vault::create(m_dir->filePath(u"vault"_s), &error);
    QVERIFY2(m_vault, qPrintable(error));
}

QString TestTodoistImport::readFile(const QString &path) const
{
    QString error;
    return notefile::read(m_vault->absolute(path), &error).value_or(QString());
}

void TestTodoistImport::writeFile(const QString &path, const QString &text) const
{
    QString error;
    QDir().mkpath(QFileInfo(m_vault->absolute(path)).path());
    QVERIFY2(notefile::write(m_vault->absolute(path), text, &error), qPrintable(error));
}

TodoistImport TestTodoistImport::import(const TodoistData &data, bool dryRun)
{
    QString error;
    std::optional<TodoistImport> result = importTodoist(*m_vault, data, dryRun, &error);
    if (!result)
        qFatal("%s", qPrintable(error));
    return *result;
}

void TestTodoistImport::readsAFullSync()
{
    const TodoistData data = read();
    // What was deleted is left out; what was archived or done is there, for the import to judge.
    QCOMPARE(data.projects.size(), 5);
    QCOMPARE(data.sections.size(), 3);
    QCOMPARE(data.tasks.size(), 10);
    QCOMPARE(data.comments.size(), 2);
    const TodoistData::Task &perdido = data.tasks.at(1);
    QCOMPARE(perdido.priority, 4);
    QCOMPARE(perdido.labels, QStringList{u"hours"_s});
    QCOMPARE(perdido.due, u"2026-11-20"_s);
    QCOMPARE(data.tasks.at(2).parentId, u"T2"_s);
    QVERIFY(data.projects.first().inbox);
    QVERIFY(data.projects.at(4).archived);
}

void TestTodoistImport::refusesWhatIsNotOne()
{
    QString error;
    QVERIFY(!readTodoistSync("not json", &error));
    QVERIFY(!error.isEmpty());
    QVERIFY(!readTodoistSync(R"({"error": "Unauthorized"})", &error));
}

void TestTodoistImport::writesEachProjectAsANote()
{
    const TodoistImport result = import(read());
    QCOMPARE(result.notes.size(), 3);
    QCOMPARE(result.count(TodoistImport::Outcome::Created), 3);
    QCOMPARE(result.notebooks, (QStringList{u"Task Lists"_s, u"Task Lists/Money"_s}));
    QCOMPARE(result.skipped, QStringList{u"1 archived project"_s});

    // Loose tasks first, then each section in Todoist's order; done, deleted and shelved tasks
    // are left out. A repeat Jot cannot follow, a deadline, a duration, a description and
    // comments go under their task, and a description's heading is bold words.
    QCOMPARE(readFile(u"Task Lists/Arrangements.md"_s).split(u'\n'),
             QString(u"- [ ] Loose task @p4\n"
                     u"\n"
                     u"## Compositions\n"
                     u"\n"
                     u"- [ ] Practice @p2 @due 2026-11-14 @daily #day-long\n"
                     u"  Use the straw\n"
                     u"  [sheet.pdf](<https://files.example/sheet.pdf>)\n"
                     u"- [ ] Voice @p4 @due 2026-11-14\n"
                     u"  Repeats every day ending 2026-11-24\n"
                     u"  Deadline 2026-11-30\n"
                     u"  Takes 30 minutes\n"
                     u"\n"
                     u"## Brass in a Box\n"
                     u"\n"
                     u"- [ ] Arrange Perdido @p1 @due 2026-11-20 #hours\n"
                     u"  **Feedback**\n"
                     u"\n"
                     u"  1. Horns\n"
                     u"  - [ ] sub-list\n"
                     u"  - [ ] Bass line @p3 @due 2026-11-21 14:30\n")
                 .split(u'\n'));
    QCOMPARE(result.notes.first().tasks, 5);
    // A project inside another is a note in a notebook named for it, and a time fixed in a time
    // zone is said as it is there.
    QCOMPARE(readFile(u"Task Lists/Money.md"_s), u"- [ ] Budget @p4\n"_s);
    QCOMPARE(readFile(u"Task Lists/Money/Clients.md"_s),
             u"- [ ] Invoice @p4 @due 2026-11-20 14:00\n"_s);
    // The Inbox's tasks go to the vault's inbox note.
    QCOMPARE(result.inboxPath, u"Inbox.md"_s);
    QCOMPARE(result.inboxTasks, 1);
    QCOMPARE(readFile(u"Inbox.md"_s), u"- [ ] Call the vet @p4\n"_s);
    QVERIFY(QFileInfo::exists(m_vault->absolute(u".jot/todoist.yaml"_s)));
}

void TestTodoistImport::importingAgainKeepsJotsEdits()
{
    TodoistData data = read();
    import(data);
    writeFile(u"Inbox.md"_s, u"- [ ] Call the vet @p4\n- [ ] Typed in Jot\n"_s);

    // Nothing new: nothing written, and the Inbox's task is not added twice.
    TodoistImport again = import(data);
    QCOMPARE(again.count(TodoistImport::Outcome::Unchanged), 3);
    QCOMPARE(again.inboxTasks, 0);

    // Changed in Todoist, a note untouched in Jot is brought up to date; one edited in Jot, or
    // removed, is left as it is.
    task(data, u"T1"_s).content = u"Loose task, renamed"_s;
    task(data, u"T11"_s).content = u"Budget for December"_s;
    task(data, u"T10"_s).content = u"Invoice twice"_s;
    writeFile(u"Task Lists/Money.md"_s, u"- [x] Budget @p4\n"_s);
    QVERIFY(QFile::remove(m_vault->absolute(u"Task Lists/Money/Clients.md"_s)));
    TodoistData::Task later;
    later.id = u"T12"_s;
    later.projectId = u"P0"_s;
    later.content = u"Book the hall"_s;
    later.order = 2;
    data.tasks.append(later);

    again = import(data);
    QCOMPARE(again.notes.at(0).outcome, TodoistImport::Outcome::Updated);
    QVERIFY(
        readFile(u"Task Lists/Arrangements.md"_s).startsWith(u"- [ ] Loose task, renamed @p4\n"_s));
    QCOMPARE(again.notes.at(1).outcome, TodoistImport::Outcome::ChangedInJot);
    QCOMPARE(readFile(u"Task Lists/Money.md"_s), u"- [x] Budget @p4\n"_s);
    QCOMPARE(again.notes.at(2).outcome, TodoistImport::Outcome::Removed);
    QVERIFY(!QFileInfo::exists(m_vault->absolute(u"Task Lists/Money/Clients.md"_s)));
    // Only the new Inbox task joins the inbox note, after what was typed there.
    QCOMPARE(again.inboxTasks, 1);
    QCOMPARE(readFile(u"Inbox.md"_s),
             u"- [ ] Call the vet @p4\n- [ ] Typed in Jot\n- [ ] Book the hall @p4\n"_s);
}

void TestTodoistImport::aDryRunWritesNothing()
{
    const TodoistImport planned = import(read(), true);
    QCOMPARE(planned.count(TodoistImport::Outcome::Created), 3);
    QCOMPARE(planned.inboxTasks, 1);
    QCOMPARE(planned.notes.at(2).path, u"Task Lists/Money/Clients.md"_s);
    QVERIFY(!QFileInfo::exists(m_vault->absolute(u"Task Lists"_s)));
    QVERIFY(!QFileInfo::exists(m_vault->absolute(u"Inbox.md"_s)));
    QVERIFY(!QFileInfo::exists(m_vault->absolute(u".jot/todoist.yaml"_s)));
    // A note already at a planned name keeps it, and the project takes the next.
    writeFile(u"Task Lists/Money.md"_s, u"mine\n"_s);
    QCOMPARE(import(read(), true).notes.at(1).path, u"Task Lists/Money 2.md"_s);
}

void TestTodoistImport::leavesAnUnreadableInboxAlone()
{
    // Not UTF-8, so Jot will not read it, and must not write over it either.
    QFile inbox(m_vault->absolute(u"Inbox.md"_s));
    QVERIFY(inbox.open(QIODevice::WriteOnly));
    inbox.write("\xff\xfe not text\n");
    inbox.close();
    TodoistData data = read();
    TodoistData::Task later;
    later.id = u"T12"_s;
    later.projectId = u"P0"_s;
    later.content = u"Book the hall"_s;
    data.tasks.append(later);
    QString error;
    QVERIFY(!importTodoist(*m_vault, data, false, &error));
    QVERIFY2(error.contains(u"Inbox.md"_s), qPrintable(error));
    QVERIFY(inbox.open(QIODevice::ReadOnly));
    QCOMPARE(inbox.readAll(), QByteArray("\xff\xfe not text\n"));
}

void TestTodoistImport::writesOnlyInsideTheVault()
{
    // A ledger edited to send a project's note outside the vault, or into Jot's own folder, is
    // not followed: the project comes in as new, inside Task Lists.
    writeFile(u".jot/todoist.yaml"_s, u"projects:\n  P1:\n    path: ../outside.md\n    sha: x\n"
                                      u"  P2:\n    path: .jot/vault.yaml\n    sha: x\ninbox:\n"_s);
    const TodoistImport result = import(read());
    QCOMPARE(result.count(TodoistImport::Outcome::Created), 3);
    QCOMPARE(result.notes.at(0).path, u"Task Lists/Arrangements.md"_s);
    QCOMPARE(result.notes.at(1).path, u"Task Lists/Money.md"_s);
    QVERIFY(!QFileInfo::exists(m_dir->filePath(u"outside.md"_s)));
    QVERIFY(readFile(u".jot/vault.yaml"_s).contains(u"format"_s));
}

QTEST_GUILESS_MAIN(TestTodoistImport)
#include "tst_todoistimport.moc"
