#include "jot/FrontMatter.h"
#include "jot/JoplinImport.h"
#include "jot/Layout.h"
#include "jot/Vault.h"

#include <QCryptographicHash>
#include <QFileInfo>
#include <QScopeGuard>
#include <QTemporaryDir>
#include <QTest>
#include <QTimeZone>

using namespace jot;
using namespace Qt::StringLiterals;

namespace {

/// A Joplin id, 32 hex characters, made from `name` so tests can say which item is which.
QString jid(const QString &name)
{
    return QString::fromLatin1(
        QCryptographicHash::hash(name.toUtf8(), QCryptographicHash::Md5).toHex());
}

QDateTime at(int day, int hour = 9)
{
    return QDateTime(QDate(2026, 10, day), QTime(hour, 0), QTimeZone::UTC);
}

JoplinExport::Note note(const QString &id, const QString &notebook, const QString &title,
                        const QString &body, int updated = 2)
{
    JoplinExport::Note n;
    n.id = jid(id);
    n.notebookId = notebook.isEmpty() ? QString() : jid(notebook);
    n.title = title;
    n.body = body;
    n.created = at(1);
    n.updated = at(updated);
    return n;
}

/// Stories, with Places inside it; two notes that link to each other and to a map.
JoplinExport stories()
{
    JoplinExport ex;
    ex.notebooks = {{jid(u"a1"_s), {}, u"Stories"_s, u"🦊"_s, false},
                    {jid(u"a2"_s), jid(u"a1"_s), u"Places"_s, {}, false}};
    ex.notes = {
        note(u"n1"_s, u"a2"_s, u"The ferry"_s,
             u"Crossing at dawn, as in [Mara](:/%1).\n\n![map](:/%2)"_s.arg(jid(u"n2"_s),
                                                                            jid(u"r1"_s))),
        note(u"n2"_s, u"a1"_s, u"Mara"_s, u"She takes [the ferry](:/%1)."_s.arg(jid(u"n1"_s)))};
    ex.tags = {{jid(u"t1"_s), u"travel"_s}};
    ex.noteTags.insert(jid(u"n1"_s), {jid(u"t1"_s)});
    ex.resources = {{jid(u"r1"_s), u"map.png"_s, u"image/png"_s, u"map.png"_s, u"png"_s,
                     QByteArray("\x89PNG", 4)}};
    return ex;
}

} // namespace

class TestJoplinImport : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void init();
    void importsNotebooksNotesTagsAndAttachments();
    void aDryRunWritesNothingAndPlansTheSame();
    void importingAgainSkipsUpdatesAndKeepsJotEdits();
    void findsANoteMovedInJot();
    void aSubNotebookAloneMovesUnderItsParentLater();
    void mergesWithFoldersAndNumbersClashingNames();
    void writesTasksAndLeavesTrashOut();
    void aNotebookThatIsItsOwnAncestorStopsAtTheTop();
    void aFailedWriteKeepsTrackOfWhatWasWritten();
    void aNotebookKnownFromBeforeKeepsItsName();
    void aRemovedNoteIsNotFoundInAnotherWithTheSameText();
    void notebooksKeepTheirEmojiUnlessChangedInJot();

private:
    QString read(const QString &path) const;

    std::unique_ptr<QTemporaryDir> m_dir;
    std::optional<Vault> m_vault;
};

void TestJoplinImport::init()
{
    m_dir = std::make_unique<QTemporaryDir>();
    QString error;
    m_vault = Vault::create(m_dir->filePath(u"vault"_s), &error);
    QVERIFY2(m_vault, qPrintable(error));
}

QString TestJoplinImport::read(const QString &path) const
{
    QString error;
    return notefile::read(m_vault->absolute(path), &error).value_or(QString());
}

void TestJoplinImport::importsNotebooksNotesTagsAndAttachments()
{
    QString error;
    const auto done = importJoplin(*m_vault, stories(), {}, false, &error);
    QVERIFY2(done, qPrintable(error));
    QCOMPARE(done->count(JoplinImport::Outcome::Created), 2);
    QCOMPARE(done->notebooks, (QStringList{u"Stories"_s, u"Stories/Places"_s}));
    QCOMPARE(done->attachments, 1);

    const QString ferry = read(u"Stories/Places/The ferry.md"_s);
    const auto front = readFrontMatter(splitFrontMatter(ferry).frontMatter);
    QCOMPARE(front.value(u"created"_s).toString(), u"2026-10-01T09:00:00Z"_s);
    QCOMPARE(front.value(u"tags"_s).toStringList(), QStringList{u"travel"_s});
    QVERIFY(!front.contains(u"title"_s));
    // Joplin's :/id links point at the files now: the other note, and the map beside this one.
    QVERIFY2(ferry.contains(u"[Mara](../Mara.md)"_s), qPrintable(ferry));
    QVERIFY2(ferry.contains(u"![map](The%20ferry.assets/map.png)"_s), qPrintable(ferry));
    QCOMPARE(read(u"Stories/Mara.md"_s).contains(u"[the ferry](Places/The%20ferry.md)"_s), true);
    QFile map(m_vault->absolute(u"Stories/Places/The ferry.assets/map.png"_s));
    QVERIFY(map.open(QIODevice::ReadOnly));
    QCOMPARE(map.readAll(), QByteArray("\x89PNG", 4));
    // The file's modification time is when Joplin last changed the note.
    QCOMPARE(QFileInfo(m_vault->absolute(u"Stories/Mara.md"_s)).lastModified().toUTC(), at(2));
    QVERIFY(QFileInfo::exists(m_vault->absolute(u".jot/joplin.yaml"_s)));
}

void TestJoplinImport::aDryRunWritesNothingAndPlansTheSame()
{
    QString error;
    const auto planned = importJoplin(*m_vault, stories(), {}, true, &error);
    QVERIFY(planned);
    QVERIFY(m_vault->notes().isEmpty());
    QVERIFY(!QFileInfo::exists(m_vault->absolute(u"Stories"_s)));
    QVERIFY(!QFileInfo::exists(m_vault->absolute(u".jot/joplin.yaml"_s)));

    const auto done = importJoplin(*m_vault, stories(), {}, false, &error);
    QVERIFY(done);
    QCOMPARE(planned->notebooks, done->notebooks);
    QCOMPARE(planned->attachments, done->attachments);
    for (int i = 0; i < done->notes.size(); ++i)
        QCOMPARE(planned->notes[i].path, done->notes[i].path);
}

void TestJoplinImport::importingAgainSkipsUpdatesAndKeepsJotEdits()
{
    QString error;
    QVERIFY(importJoplin(*m_vault, stories(), {}, false, &error));

    // The same export again changes nothing.
    auto again = importJoplin(*m_vault, stories(), {}, false, &error);
    QVERIFY(again);
    QCOMPARE(again->count(JoplinImport::Outcome::Unchanged), 2);
    QCOMPARE(again->attachments, 0);
    QCOMPARE(m_vault->notes().size(), 2);

    // Both changed in Joplin; Mara was also edited in Jot, so only the ferry is updated.
    const QString edited = read(u"Stories/Mara.md"_s) + u"\nA line written in Jot.\n"_s;
    QVERIFY(notefile::write(m_vault->absolute(u"Stories/Mara.md"_s), edited, &error));
    JoplinExport changed = stories();
    changed.notes[0].body = u"Crossing at noon."_s;
    changed.notes[0].updated = at(5);
    changed.notes[1].body = u"Joplin's newer Mara."_s;
    changed.notes[1].updated = at(5);
    again = importJoplin(*m_vault, changed, {}, false, &error);
    QVERIFY(again);
    QCOMPARE(again->count(JoplinImport::Outcome::Updated), 1);
    QCOMPARE(again->count(JoplinImport::Outcome::ChangedInJot), 1);
    QVERIFY(read(u"Stories/Places/The ferry.md"_s).contains(u"Crossing at noon."_s));
    QCOMPARE(read(u"Stories/Mara.md"_s), edited);

    // A note deleted in Jot is not brought back.
    QVERIFY(QFile::remove(m_vault->absolute(u"Stories/Places/The ferry.md"_s)));
    again = importJoplin(*m_vault, changed, {}, false, &error);
    QVERIFY(again);
    QCOMPARE(again->count(JoplinImport::Outcome::Removed), 1);
    QVERIFY(!QFileInfo::exists(m_vault->absolute(u"Stories/Places/The ferry.md"_s)));
}

void TestJoplinImport::findsANoteMovedInJot()
{
    QString error;
    QVERIFY(importJoplin(*m_vault, stories(), {}, false, &error));
    QVERIFY(m_vault->moveNote(u"Stories/Mara.md"_s, u"Mara.md"_s, &error));

    const auto again = importJoplin(*m_vault, stories(), {}, false, &error);
    QVERIFY(again);
    QCOMPARE(again->count(JoplinImport::Outcome::Unchanged), 2);
    QCOMPARE(m_vault->notes().size(), 2);
    QVERIFY(QFileInfo::exists(m_vault->absolute(u"Mara.md"_s)));
}

void TestJoplinImport::aSubNotebookAloneMovesUnderItsParentLater()
{
    // Places exported on its own lands at the top, then moves into Stories when that comes.
    JoplinExport places;
    places.notebooks = {{jid(u"a2"_s), jid(u"a1"_s), u"Places"_s, {}, false}};
    places.notes = {note(u"n1"_s, u"a2"_s, u"The ferry"_s, u"Crossing."_s)};
    QString error;
    QVERIFY(importJoplin(*m_vault, places, {}, false, &error));
    QVERIFY(QFileInfo::exists(m_vault->absolute(u"Places/The ferry.md"_s)));

    JoplinExport parent;
    parent.notebooks = {{jid(u"a1"_s), {}, u"Stories"_s, {}, false}};
    parent.notes = {
        note(u"n2"_s, u"a1"_s, u"Mara"_s, u"About [the ferry](:/%1)."_s.arg(jid(u"n1"_s)))};
    const auto done = importJoplin(*m_vault, parent, {}, false, &error);
    QVERIFY2(done, qPrintable(error));
    QVERIFY(QFileInfo::exists(m_vault->absolute(u"Stories/Places/The ferry.md"_s)));
    QVERIFY(!QFileInfo::exists(m_vault->absolute(u"Places"_s)));
    // The link to a note from the earlier import follows it to where it is now.
    QVERIFY(read(u"Stories/Mara.md"_s).contains(u"[the ferry](Places/The%20ferry.md)"_s));
}

void TestJoplinImport::mergesWithFoldersAndNumbersClashingNames()
{
    QString error;
    QVERIFY(m_vault->createFolder({}, u"Stories"_s, &error));
    QVERIFY(m_vault->createNote(u"Stories"_s, u"Mara"_s, at(1), &error));

    const auto done = importJoplin(*m_vault, stories(), u"Imported"_s, false, &error);
    QVERIFY(done);
    // Under the folder asked for, so the existing Stories is not touched.
    QVERIFY(QFileInfo::exists(m_vault->absolute(u"Imported/Stories/Mara.md"_s)));

    // Another export, with notebooks and notes this vault has not seen, at the top.
    JoplinExport ex = stories();
    for (auto &b : ex.notebooks) {
        b.id = jid(b.id.right(2) + u"x"_s);
        if (!b.parentId.isEmpty())
            b.parentId = jid(b.parentId.right(2) + u"x"_s);
    }
    for (auto &n : ex.notes) {
        n.id = jid(n.id.right(2) + u"x"_s);
        n.notebookId = jid(n.notebookId.right(2) + u"x"_s);
    }
    QVERIFY(importJoplin(*m_vault, ex, {}, false, &error));
    // Joplin's Stories merges with the vault's, and a second Mara gets a number.
    QVERIFY(QFileInfo::exists(m_vault->absolute(u"Stories/Mara 2.md"_s)));
    QVERIFY(!QFileInfo::exists(m_vault->absolute(u"Stories 2"_s)));
}

void TestJoplinImport::writesTasksAndLeavesTrashOut()
{
    JoplinExport ex;
    auto task = note(u"n1"_s, {}, u"Q&A / call: ferry office?"_s, u"Ask about times."_s);
    task.isTodo = true;
    task.due = at(10, 17);
    auto finished = note(u"n2"_s, {}, u"Book tickets"_s, {});
    finished.isTodo = true;
    finished.completed = at(3);
    auto binned = note(u"n3"_s, {}, u"Old draft"_s, u"gone"_s);
    binned.deleted = true;
    auto clash = note(u"n4"_s, {}, u"Book tickets"_s, u"the other edit"_s);
    clash.conflict = true;
    ex.notes = {task, finished, binned, clash};

    QString error;
    const auto done = importJoplin(*m_vault, ex, {}, false, &error);
    QVERIFY(done);
    QCOMPARE(done->notes.size(), 3);
    QCOMPARE(done->skipped, QStringList{u"1 note in Joplin's trash"_s});

    const auto front = [&](const QString &path) {
        const QString text = read(path);
        return readFrontMatter(splitFrontMatter(text).frontMatter);
    };
    const auto open = front(u"Q&A - call ferry office.md"_s);
    QCOMPARE(open.value(u"title"_s).toString(), u"Q&A / call: ferry office?"_s);
    QCOMPARE(open.value(u"task"_s).toString(), u"open"_s);
    QCOMPARE(open.value(u"due"_s).toString(), u"2026-10-10T17:00:00Z"_s);
    const auto closed = front(u"Book tickets.md"_s);
    QCOMPARE(closed.value(u"task"_s).toString(), u"done"_s);
    QCOMPARE(closed.value(u"done"_s).toString(), u"2026-10-03T09:00:00Z"_s);
    QVERIFY(QFileInfo::exists(m_vault->absolute(u"Book tickets (conflict).md"_s)));
}

void TestJoplinImport::aNotebookThatIsItsOwnAncestorStopsAtTheTop()
{
    JoplinExport ex;
    ex.notebooks = {{jid(u"a1"_s), jid(u"a2"_s), u"One"_s, {}, false},
                    {jid(u"a2"_s), jid(u"a1"_s), u"Two"_s, {}, false},
                    {jid(u"a3"_s), jid(u"a3"_s), u"Self"_s, {}, false}};
    ex.notes = {note(u"n1"_s, u"a1"_s, u"In one"_s, u"x"_s),
                note(u"n2"_s, u"a3"_s, u"In self"_s, u"y"_s)};
    QString error;
    const auto done = importJoplin(*m_vault, ex, {}, false, &error);
    QVERIFY2(done, qPrintable(error));
    QCOMPARE(done->count(JoplinImport::Outcome::Created), 2);
    QCOMPARE(m_vault->notes().size(), 2);
}

void TestJoplinImport::aFailedWriteKeepsTrackOfWhatWasWritten()
{
    // A folder the import merges into, but cannot write in, fails the second note.
    QString error;
    QVERIFY(m_vault->createFolder({}, u"Locked"_s, &error));
    QFile locked(m_vault->absolute(u"Locked"_s));
    QVERIFY(locked.setPermissions(QFile::ReadOwner | QFile::ExeOwner));
    const auto unlock = qScopeGuard(
        [&] { locked.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner); });
    QFile probe(m_vault->absolute(u"Locked/probe"_s));
    if (probe.open(QIODevice::WriteOnly))
        QSKIP("permissions are not enforced here, as when tests run as root");

    JoplinExport ex;
    ex.notebooks = {{jid(u"a1"_s), {}, u"Open"_s, {}, false},
                    {jid(u"a2"_s), {}, u"Locked"_s, {}, false}};
    ex.notes = {note(u"n1"_s, u"a1"_s, u"First"_s, u"x"_s),
                note(u"n2"_s, u"a2"_s, u"Second"_s, u"y"_s)};
    QVERIFY(!importJoplin(*m_vault, ex, {}, false, &error));
    QVERIFY(QFileInfo::exists(m_vault->absolute(u"Open/First.md"_s)));

    // Once it can be written, the next import brings the rest and no second copy of the first.
    locked.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
    const auto again = importJoplin(*m_vault, ex, {}, false, &error);
    QVERIFY2(again, qPrintable(error));
    QCOMPARE(again->count(JoplinImport::Outcome::Unchanged), 1);
    QCOMPARE(again->count(JoplinImport::Outcome::Created), 1);
    QVERIFY(!QFileInfo::exists(m_vault->absolute(u"Open/First 2.md"_s)));
}

void TestJoplinImport::aNotebookKnownFromBeforeKeepsItsName()
{
    QString error;
    QVERIFY(importJoplin(*m_vault, stories(), {}, false, &error));
    QVERIFY(m_vault->moveFolder(u"Stories"_s, u"Tales"_s, &error));

    // Places again, without Stories: Stories is known only from the first import, and its
    // folder has gone, so it comes back by the name it had.
    JoplinExport places;
    places.notebooks = {{jid(u"a2"_s), jid(u"a1"_s), u"Places"_s, {}, false}};
    places.notes = {note(u"n9"_s, u"a2"_s, u"The harbour"_s, u"z"_s)};
    QVERIFY(importJoplin(*m_vault, places, {}, false, &error));
    QVERIFY(QFileInfo::exists(m_vault->absolute(u"Stories/Places/The harbour.md"_s)));
    QVERIFY(!QFileInfo::exists(m_vault->absolute(u"Untitled"_s)));
}

void TestJoplinImport::aRemovedNoteIsNotFoundInAnotherWithTheSameText()
{
    // Two notes with the same text; one is deleted in Jot, then changed in Joplin.
    JoplinExport ex;
    ex.notes = {note(u"n1"_s, {}, u"First"_s, u"same"_s),
                note(u"n2"_s, {}, u"Second"_s, u"same"_s)};
    for (auto &n : ex.notes)
        n.created = {};
    QString error;
    QVERIFY(importJoplin(*m_vault, ex, {}, false, &error));
    const QString second = read(u"Second.md"_s);
    QCOMPARE(read(u"First.md"_s), second);
    QVERIFY(QFile::remove(m_vault->absolute(u"First.md"_s)));

    ex.notes[0].body = u"changed in Joplin"_s;
    ex.notes[0].updated = at(6);
    const auto again = importJoplin(*m_vault, ex, {}, false, &error);
    QVERIFY(again);
    QCOMPARE(again->notes[0].outcome, JoplinImport::Outcome::Removed);
    QCOMPARE(read(u"Second.md"_s), second);
    QVERIFY(!QFileInfo::exists(m_vault->absolute(u"First.md"_s)));
}

void TestJoplinImport::notebooksKeepTheirEmojiUnlessChangedInJot()
{
    QString error;
    QVERIFY(importJoplin(*m_vault, stories(), {}, true, &error));
    QVERIFY(!QFileInfo::exists(m_vault->absolute(Layout::kPath)));

    QVERIFY2(importJoplin(*m_vault, stories(), {}, false, &error), qPrintable(error));
    Layout layout = Layout::read(*m_vault);
    QCOMPARE(layout.emojis(), (QHash<QString, QString>{{u"Stories"_s, u"🦊"_s}}));

    // One chosen in Jot since stays.
    layout.setEmoji(u"Stories"_s, u"📚"_s);
    QVERIFY(layout.write(*m_vault, &error));
    QVERIFY2(importJoplin(*m_vault, stories(), {}, false, &error), qPrintable(error));
    QCOMPARE(Layout::read(*m_vault).emoji(u"Stories"_s), u"📚"_s);
}

QTEST_GUILESS_MAIN(TestJoplinImport)
#include "tst_joplinimport.moc"
