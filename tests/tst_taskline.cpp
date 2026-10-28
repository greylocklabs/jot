#include "jot/TaskLine.h"

#include <QTest>

using namespace jot;
using namespace Qt::StringLiterals;

class TestTaskLine : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void reads_data();
    void reads();
    void findsTheNextDay();
    void writesTheNextOneWhenDone();
};

void TestTaskLine::reads_data()
{
    QTest::addColumn<QString>("text");
    QTest::addColumn<TaskLine>("line");

    QTest::newRow("plain") << u"Buy rope"_s << TaskLine{u"Buy rope"_s, {}, {}, {}, {}};
    QTest::newRow("due") << u"Call Soren 📅 2026-10-29"_s
                         << TaskLine{u"Call Soren"_s, u"2026-10-29"_s, {}, {}, {}};
    QTest::newRow("due at a time") << u"Call Soren 📅 2026-10-29 9:30"_s
                                   << TaskLine{u"Call Soren"_s, u"2026-10-29T09:30"_s, {}, {}, {}};
    QTest::newRow("labels and tags")
        << u"Call Soren @minutes #calls @Phone 📅 2026-10-29 14:00"_s
        << TaskLine{
               u"Call Soren #calls"_s, u"2026-10-29T14:00"_s, {}, {}, {u"minutes"_s, u"Phone"_s}};
    QTest::newRow("repeats and done")
        << u"Water plants 🔁 every week 📅 2026-10-27 ✅ 2026-10-27"_s
        << TaskLine{u"Water plants"_s, u"2026-10-27"_s, u"every week"_s, u"2026-10-27"_s, {}};
    QTest::newRow("words after the date")
        << u"Pay rent 📅 2026-11-01 #home"_s
        << TaskLine{u"Pay rent #home"_s, u"2026-11-01"_s, {}, {}, {}};
    QTest::newRow("other markers left out")
        << u"Plan ⏫ ⏳ 2026-10-30 🛫 2026-10-28"_s << TaskLine{u"Plan"_s, {}, {}, {}, {}};
    QTest::newRow("labels and tags after the rule")
        << u"Water plants 🔁 every week @day #home"_s
        << TaskLine{u"Water plants #home"_s, {}, u"every week"_s, {}, {u"day"_s}};
    QTest::newRow("words after a priority")
        << u"Plan ⏫ the trip @days"_s << TaskLine{u"Plan the trip"_s, {}, {}, {}, {u"days"_s}};
    QTest::newRow("an address is no label")
        << u"Mail hello@example.org"_s << TaskLine{u"Mail hello@example.org"_s, {}, {}, {}, {}};
    QTest::newRow("a date that is not one")
        << u"Pay 📅 soon @bills"_s << TaskLine{u"Pay soon"_s, {}, {}, {}, {u"bills"_s}};
}

void TestTaskLine::reads()
{
    QFETCH(QString, text);
    QFETCH(TaskLine, line);
    const TaskLine read = readTaskLine(text);
    QCOMPARE(read.title, line.title);
    QCOMPARE(read.due, line.due);
    QCOMPARE(read.repeat, line.repeat);
    QCOMPARE(read.done, line.done);
    QCOMPARE(read.labels, line.labels);
}

void TestTaskLine::findsTheNextDay()
{
    const QDate wednesday(2026, 10, 28);
    QCOMPARE(nextOccurrence(u"every day"_s, wednesday), QDate(2026, 10, 29));
    QCOMPARE(nextOccurrence(u"every 3 days"_s, wednesday), QDate(2026, 10, 31));
    QCOMPARE(nextOccurrence(u"every week"_s, wednesday), QDate(2026, 11, 4));
    QCOMPARE(nextOccurrence(u"every other week"_s, wednesday), QDate(2026, 11, 11));
    QCOMPARE(nextOccurrence(u"every 2 weeks"_s, wednesday), QDate(2026, 11, 11));
    QCOMPARE(nextOccurrence(u"every month"_s, QDate(2026, 1, 31)), QDate(2026, 2, 28));
    QCOMPARE(nextOccurrence(u"every year"_s, wednesday), QDate(2027, 10, 28));
    QCOMPARE(nextOccurrence(u"Every Monday"_s, wednesday), QDate(2026, 11, 2));
    QCOMPARE(nextOccurrence(u"every wednesday"_s, wednesday), QDate(2026, 11, 4));
    QCOMPARE(nextOccurrence(u"every weekday"_s, QDate(2026, 10, 30)), QDate(2026, 11, 2));
    QCOMPARE(nextOccurrence(u"every 2 weeks on monday"_s, QDate(2026, 11, 2)), QDate(2026, 11, 16));
    QCOMPARE(nextOccurrence(u"every 2 weeks on monday"_s, wednesday), QDate(2026, 11, 9));
    QVERIFY(!nextOccurrence(u"every full moon"_s, wednesday).isValid());
    // A count past any sensible repeat is no rule Jot reads.
    QVERIFY(!nextOccurrence(u"every 400000000 weeks"_s, wednesday).isValid());
    QVERIFY(!nextOccurrence(u"every 99999999999999999999 days"_s, wednesday).isValid());
    QVERIFY(!nextOccurrence(u"every week"_s, {}).isValid());
}

void TestTaskLine::writesTheNextOneWhenDone()
{
    const QDate today(2026, 10, 28);
    // One that does not repeat is only ticked.
    QCOMPARE(completedTaskLine(u"- [ ] Buy rope 📅 2026-10-28"_s, today),
             u"- [x] Buy rope 📅 2026-10-28"_s);
    // One that does gets its next one above it, at the same time of day.
    QCOMPARE(completedTaskLine(u"  - [ ] Water @day 🔁 every week 📅 2026-10-28 09:00"_s, today),
             u"  - [ ] Water @day 🔁 every week 📅 2026-11-04 09:00\n"
             "  - [x] Water @day 🔁 every week 📅 2026-10-28 09:00 ✅ 2026-10-28"_s);
    // Done late, it comes round after today, not on a day gone by.
    QCOMPARE(completedTaskLine(u"- [ ] Stretch 🔁 every day 📅 2026-10-20"_s, today),
             u"- [ ] Stretch 🔁 every day 📅 2026-10-29\n"
             "- [x] Stretch 🔁 every day 📅 2026-10-20 ✅ 2026-10-28"_s);
    // Windows line breaks stay, and one with no day, or a rule Jot cannot read, is only ticked.
    QCOMPARE(
        completedTaskLine(u"- [ ] Pay 🔁 every month 📅 2026-10-28\r"_s, today),
        u"- [ ] Pay 🔁 every month 📅 2026-11-28\r\n- [x] Pay 🔁 every month 📅 2026-10-28 ✅ 2026-10-28\r"_s);
    QCOMPARE(completedTaskLine(u"- [ ] Pay 🔁 every week"_s, today), u"- [x] Pay 🔁 every week"_s);
    QCOMPARE(completedTaskLine(u"- [ ] Huge 🔁 every 400000000 weeks 📅 2000-01-01"_s, today),
             u"- [x] Huge 🔁 every 400000000 weeks 📅 2000-01-01"_s);
    QCOMPARE(completedTaskLine(u"- [ ] Moon 🔁 every full moon 📅 2026-10-28"_s, today),
             u"- [x] Moon 🔁 every full moon 📅 2026-10-28"_s);
}

QTEST_GUILESS_MAIN(TestTaskLine)
#include "tst_taskline.moc"
