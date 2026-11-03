#include "jot/NoteHistory.h"

#include "jot/Vault.h"

#include <QDir>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QTest>

using namespace jot;
using namespace Qt::StringLiterals;

class TestNoteHistory : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void init();

    void keepsVersionsNewestFirst();
    void keepsNothingNewOrEmpty();
    void keepsTwoFromTheSameSecond();
    void ignoresFilesThatAreNotVersions();
    void movesWithItsNote();
    void movesWithItsNotebook();
    void joinsHistoryLeftWhereANoteMoves();
    void thinsOutVersionsAsTheyAge();

private:
    void write(const QString &path, const QString &text);
    QString read(const QString &path);
    QStringList texts(const QString &note);

    std::unique_ptr<QTemporaryDir> m_dir;
    std::unique_ptr<Vault> m_vault;
};

void TestNoteHistory::init()
{
    m_dir = std::make_unique<QTemporaryDir>();
    m_vault = std::make_unique<Vault>(m_dir->filePath(u"vault"_s));
    QVERIFY(QDir().mkpath(m_vault->root()));
}

void TestNoteHistory::write(const QString &path, const QString &text)
{
    QVERIFY(QDir().mkpath(QFileInfo(m_vault->absolute(path)).path()));
    QString error;
    QVERIFY2(notefile::write(m_vault->absolute(path), text, &error), qPrintable(error));
}

QString TestNoteHistory::read(const QString &path)
{
    QString error;
    return notefile::read(m_vault->absolute(path), &error).value_or(u"(missing)"_s);
}

QStringList TestNoteHistory::texts(const QString &note)
{
    QStringList found;
    for (const NoteHistory::Version &version : NoteHistory(*m_vault).versions(note))
        found.append(read(version.path));
    return found;
}

void TestNoteHistory::keepsVersionsNewestFirst()
{
    const NoteHistory history(*m_vault);
    const QDateTime monday(QDate(2026, 10, 26), QTime(9, 15, 30));
    QString error;
    QVERIFY(history.keep(u"Ashfall/Mara.md"_s, u"first\n"_s, monday, &error));
    QVERIFY(history.keep(u"Ashfall/Mara.md"_s, u"third\n"_s, monday.addDays(2), &error));
    QVERIFY(history.keep(u"Ashfall/Mara.md"_s, u"second\n"_s, monday.addDays(1), &error));

    const QList<NoteHistory::Version> versions = history.versions(u"Ashfall/Mara.md"_s);
    QCOMPARE(versions.size(), 3);
    QCOMPARE(versions.first().path, u".jot/history/Ashfall/Mara.md/2026-10-28 091530.md"_s);
    QCOMPARE(versions.first().written, monday.addDays(2));
    QCOMPARE(texts(u"Ashfall/Mara.md"_s), (QStringList{u"third\n"_s, u"second\n"_s, u"first\n"_s}));
    QVERIFY(history.versions(u"Ashfall/Soren.md"_s).isEmpty());
}

void TestNoteHistory::keepsNothingNewOrEmpty()
{
    const NoteHistory history(*m_vault);
    const QDateTime now(QDate(2026, 10, 28), QTime(15, 30));
    QString error;
    QVERIFY(history.keep(u"Mara.md"_s, u"same\n"_s, now, &error));
    QVERIFY(history.keep(u"Mara.md"_s, u"same\n"_s, now.addSecs(600), &error));
    QVERIFY(history.keep(u"Mara.md"_s, QString(), now.addSecs(1200), &error));
    QCOMPARE(texts(u"Mara.md"_s), QStringList{u"same\n"_s});

    // Only the newest counts: going back to an older text is a change worth keeping.
    QVERIFY(history.keep(u"Mara.md"_s, u"other\n"_s, now.addSecs(1800), &error));
    QVERIFY(history.keep(u"Mara.md"_s, u"same\n"_s, now.addSecs(2400), &error));
    QCOMPARE(texts(u"Mara.md"_s), (QStringList{u"same\n"_s, u"other\n"_s, u"same\n"_s}));
}

void TestNoteHistory::keepsTwoFromTheSameSecond()
{
    const NoteHistory history(*m_vault);
    const QDateTime now(QDate(2026, 10, 28), QTime(15, 30, 12));
    QString error;
    QVERIFY(history.keep(u"Mara.md"_s, u"one\n"_s, now, &error));
    QVERIFY(history.keep(u"Mara.md"_s, u"two\n"_s, now.addMSecs(400), &error));
    QCOMPARE(read(u".jot/history/Mara.md/2026-10-28 153012 2.md"_s), u"two\n"_s);
    QCOMPARE(texts(u"Mara.md"_s), (QStringList{u"two\n"_s, u"one\n"_s}));
}

void TestNoteHistory::ignoresFilesThatAreNotVersions()
{
    write(u".jot/history/Mara.md/notes.md"_s, u"mine\n"_s);
    write(u".jot/history/Mara.md/2026-10-28 153012.txt"_s, u"mine\n"_s);
    write(u".jot/history/Mara.md/2026-10-28 153012.md"_s, u"kept\n"_s);
    QCOMPARE(texts(u"Mara.md"_s), QStringList{u"kept\n"_s});
}

void TestNoteHistory::movesWithItsNote()
{
    write(u"Mara.md"_s, u"now\n"_s);
    QString error;
    QVERIFY(NoteHistory(*m_vault).keep(u"Mara.md"_s, u"then\n"_s,
                                       QDateTime(QDate(2026, 10, 28), QTime(9, 0)), &error));
    QVERIFY2(m_vault->moveNote(u"Mara.md"_s, u"Ashfall/Mara Vell.md"_s, &error), qPrintable(error));
    QVERIFY(texts(u"Mara.md"_s).isEmpty());
    QCOMPARE(texts(u"Ashfall/Mara Vell.md"_s), QStringList{u"then\n"_s});

    // A note with no history moves as before.
    write(u"Soren.md"_s, u"now\n"_s);
    QVERIFY2(m_vault->moveNote(u"Soren.md"_s, u"Ashfall/Soren.md"_s, &error), qPrintable(error));

    // History that cannot follow does not undo or fail a move already made.
    write(u"Kit.md"_s, u"now\n"_s);
    QVERIFY(NoteHistory(*m_vault).keep(u"Kit.md"_s, u"then\n"_s,
                                       QDateTime(QDate(2026, 10, 28), QTime(9, 0)), &error));
    write(u".jot/history/Books"_s, u"a file where a folder would go\n"_s);
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(u"but not its history"_s));
    QVERIFY2(m_vault->moveNote(u"Kit.md"_s, u"Books/Kit.md"_s, &error), qPrintable(error));
    QVERIFY(QFileInfo::exists(m_vault->absolute(u"Books/Kit.md"_s)));
}

void TestNoteHistory::movesWithItsNotebook()
{
    write(u"Ashfall/Characters/Mara.md"_s, u"now\n"_s);
    QString error;
    QVERIFY(NoteHistory(*m_vault).keep(u"Ashfall/Characters/Mara.md"_s, u"then\n"_s,
                                       QDateTime(QDate(2026, 10, 28), QTime(9, 0)), &error));
    QVERIFY2(m_vault->moveFolder(u"Ashfall"_s, u"Books/Ashfall"_s, &error), qPrintable(error));
    QCOMPARE(texts(u"Books/Ashfall/Characters/Mara.md"_s), QStringList{u"then\n"_s});
    QVERIFY(!QFileInfo::exists(m_vault->absolute(u".jot/history/Ashfall"_s)));
}

void TestNoteHistory::joinsHistoryLeftWhereANoteMoves()
{
    // A note once at Soren.md was deleted; its history stays, and Mara's joins it there.
    const QDateTime nine(QDate(2026, 10, 28), QTime(9, 0));
    const NoteHistory history(*m_vault);
    QString error;
    QVERIFY(history.keep(u"Soren.md"_s, u"old Soren\n"_s, nine, &error));
    write(u"Mara.md"_s, u"now\n"_s);
    QVERIFY(history.keep(u"Mara.md"_s, u"Mara at nine\n"_s, nine, &error));
    QVERIFY(history.keep(u"Mara.md"_s, u"Mara at ten\n"_s, nine.addSecs(3600), &error));

    QVERIFY2(m_vault->moveNote(u"Mara.md"_s, u"Soren.md"_s, &error), qPrintable(error));
    QCOMPARE(texts(u"Soren.md"_s),
             (QStringList{u"Mara at ten\n"_s, u"Mara at nine\n"_s, u"old Soren\n"_s}));
    QVERIFY(!QFileInfo::exists(m_vault->absolute(u".jot/history/Mara.md"_s)));

    // Both hold two versions from the same second: Ines's take the next free numbers.
    QVERIFY(history.keep(u"Kit.md"_s, u"Kit one\n"_s, nine, &error));
    QVERIFY(history.keep(u"Kit.md"_s, u"Kit two\n"_s, nine, &error));
    write(u"Ines.md"_s, u"now\n"_s);
    QVERIFY(history.keep(u"Ines.md"_s, u"Ines one\n"_s, nine, &error));
    QVERIFY(history.keep(u"Ines.md"_s, u"Ines two\n"_s, nine, &error));
    QVERIFY2(m_vault->moveNote(u"Ines.md"_s, u"Kit.md"_s, &error), qPrintable(error));
    QCOMPARE(texts(u"Kit.md"_s).size(), 4);
    QVERIFY(QFileInfo::exists(m_vault->absolute(u".jot/history/Kit.md/2026-10-28 090000 4.md"_s)));
}

void TestNoteHistory::thinsOutVersionsAsTheyAge()
{
    const NoteHistory history(*m_vault);
    const QDateTime now(QDate(2026, 11, 3), QTime(12, 0));
    QString error;
    int n = 0;
    const auto at = [&](const QDateTime &when) {
        QVERIFY(history.add(u"Mara.md"_s, u"v%1\n"_s.arg(++n), when, &error));
    };
    // Today: every one stays.
    at(now.addSecs(-60));
    at(now.addSecs(-3600 * 2));
    at(now.addSecs(-3600 * 3));
    // Two days ago: two in one hour, one in the next.
    const QDateTime twoDays = now.addDays(-2);
    at(twoDays.addSecs(10 * 60));
    at(twoDays.addSecs(40 * 60));
    at(twoDays.addSecs(70 * 60));
    // Ten days ago: three in one day.
    const QDateTime tenDays = now.addDays(-10);
    for (int hour = 8; hour <= 10; ++hour)
        at(tenDays.addSecs(hour * 3600));
    // Two months ago: one each day of a week.
    const QDate monday(2026, 9, 7);
    for (int day = 0; day < 5; ++day)
        at(QDateTime(monday.addDays(day), QTime(9, 0)));

    QCOMPARE(history.prune(u"Mara.md"_s, now), 2 + 4 + 1);
    // The newest in each stretch is the one that stays.
    QCOMPARE(texts(u"Mara.md"_s), (QStringList{u"v1\n"_s, u"v2\n"_s, u"v3\n"_s, u"v6\n"_s,
                                               u"v5\n"_s, u"v9\n"_s, u"v14\n"_s}));
    QCOMPARE(history.prune(u"Mara.md"_s, now), 0);
}

QTEST_GUILESS_MAIN(TestNoteHistory)
#include "tst_notehistory.moc"
