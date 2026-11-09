#include "jot/Vault.h"
#include "jot/WritingStats.h"

#include <QDir>
#include <QTemporaryDir>
#include <QTest>

using namespace jot;
using namespace Qt::StringLiterals;

class TestWritingStats : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void countsWordsAndCharacters();
    void addsUpTheDaysOfEveryComputer();
    void countsTheDaysInARow();
};

void TestWritingStats::countsWordsAndCharacters()
{
    const TextCount count = countText(u"# Mara's well-kept log\n\n- [ ] 3 ferries, *maybe*\n"_s);
    // Markup is no word; an apostrophe or hyphen keeps one whole.
    QCOMPARE(count.words, 6);
    QCOMPARE(count.characters, 46);
    QCOMPARE(countText(u""_s).words, 0);
    QCOMPARE(countText(u"café über naïve"_s).words, 3);
    // An emoji is one character, though two UTF-16 units.
    QCOMPARE(countText(u"\U0001F98A fox"_s).characters, 5);
}

void TestWritingStats::addsUpTheDaysOfEveryComputer()
{
    QTemporaryDir dir;
    const QString folder = dir.filePath(u"stats"_s);
    const QDate day(2026, 11, 8);
    QString error;
    WritingStats laptop(folder, u"laptop"_s);
    QVERIFY(laptop.add(day, 300, &error));
    QVERIFY(laptop.add(day, -20, &error));
    QCOMPARE(laptop.wordsOn(day), 280);

    // Each computer writes its own file, and reads the others'.
    WritingStats desktop(folder, u"desktop"_s);
    QVERIFY(desktop.add(day, 100, &error));
    QCOMPARE(desktop.wordsOn(day), 380);
    QVERIFY(laptop.add(day.addDays(1), 5, &error));
    QCOMPARE(laptop.wordsOn(day), 380);
    QCOMPARE(QDir(folder).entryList(QDir::Files),
             (QStringList{u"desktop.yaml"_s, u"laptop.yaml"_s}));
    QCOMPARE(*notefile::read(QDir(folder).filePath(u"laptop.yaml"_s), &error),
             u"2026-11-08: 280\n2026-11-09: 5\n"_s);

    // A day that took more away than it added counts as none.
    QVERIFY(desktop.add(day.addDays(2), -50, &error));
    QCOMPARE(desktop.wordsOn(day.addDays(2)), 0);
}

void TestWritingStats::countsTheDaysInARow()
{
    QTemporaryDir dir;
    WritingStats stats(dir.filePath(u"stats"_s), u"here"_s);
    const QDate today(2026, 11, 8);
    QString error;
    for (int back : {1, 2, 3, 5})
        QVERIFY(stats.add(today.addDays(-back), 600, &error));
    QVERIFY(stats.add(today.addDays(-2), -300, &error));
    // Today has none yet, so the run up to yesterday stands.
    QCOMPARE(stats.streak(today, 0), 3);
    QCOMPARE(stats.streak(today, 500), 1);
    QVERIFY(stats.add(today, 500, &error));
    QCOMPARE(stats.streak(today, 500), 2);
    QCOMPARE(stats.streak(today, 0), 4);
}

QTEST_GUILESS_MAIN(TestWritingStats)
#include "tst_writingstats.moc"
