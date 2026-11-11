#include "jot/QuickAdd.h"

#include <QTest>

using namespace jot;
using namespace Qt::StringLiterals;

class TestQuickAdd : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void reads_data();
    void reads();
    void schedulesACheckboxFromItsWords();
    void setsAndClearsTheDayAndRepeat();
    void keepsWordsAfterARuleWhenRescheduled();
    void keepsARepeatRuleAsWritten();
};

void TestQuickAdd::reads_data()
{
    QTest::addColumn<QString>("typed");
    QTest::addColumn<QString>("line");

    // As of Wednesday, October 28, 2026, at 10:00.
    QTest::newRow("nothing") << u"Buy rope"_s << u"Buy rope"_s;
    QTest::newRow("tomorrow, a time and tags")
        << u"call Soren tomorrow 3pm #calls"_s << u"call Soren #calls @due 2026-10-29 15:00"_s;
    QTest::newRow("markers typed as written")
        << u"water plants @weekly"_s << u"water plants @weekly @due 2026-10-28"_s;
    QTest::newRow("a priority, as Todoist writes it")
        << u"call Soren p1 tomorrow"_s << u"call Soren @due 2026-10-29 @p1"_s;
    QTest::newRow("a priority marker") << u"call Soren @p2"_s << u"call Soren @p2"_s;
    QTest::newRow("a day typed as written")
        << u"pay rent @due 2026-11-01 9:00"_s << u"pay rent @due 2026-11-01 09:00"_s;
    QTest::newRow("today") << u"pay rent today"_s << u"pay rent @due 2026-10-28"_s;
    QTest::newRow("month first in numbers")
        << u"pay rent 11-14-2026"_s << u"pay rent @due 2026-11-14"_s;
    QTest::newRow("a month and day gone is next year's")
        << u"pay rent 10/1"_s << u"pay rent @due 2027-10-01"_s;
    QTest::newRow("no such day") << u"pay rent 13/40"_s << u"pay rent 13/40"_s;
    QTest::newRow("a time alone is today")
        << u"stand-up at 9:30 am"_s << u"stand-up @due 2026-10-28 09:30"_s;
    QTest::newRow("24 hours") << u"meet Mara tomorrow at 15:45"_s
                              << u"meet Mara @due 2026-10-29 15:45"_s;
    QTest::newRow("noon") << u"lunch friday at noon"_s << u"lunch @due 2026-10-30 12:00"_s;
    QTest::newRow("a weekday is the next one") << u"plan wednesday"_s << u"plan @due 2026-11-04"_s;
    QTest::newRow("short weekday after on")
        << u"mow the lawn on sat"_s << u"mow the lawn @due 2026-10-31"_s;
    QTest::newRow("a short day name alone stays") << u"sat by the fire"_s << u"sat by the fire"_s;
    QTest::newRow("next week") << u"taxes next week"_s << u"taxes @due 2026-11-02"_s;
    QTest::newRow("next month") << u"renew next month"_s << u"renew @due 2026-11-01"_s;
    QTest::newRow("in days") << u"follow up in 3 days"_s << u"follow up @due 2026-10-31"_s;
    QTest::newRow("in a week") << u"follow up in a week"_s << u"follow up @due 2026-11-04"_s;
    QTest::newRow("month and day")
        << u"dentist Nov 3rd 2pm"_s << u"dentist @due 2026-11-03 14:00"_s;
    QTest::newRow("a day gone by is next year")
        << u"birthday march 2"_s << u"birthday @due 2027-03-02"_s;
    QTest::newRow("day and month") << u"trip 12 december"_s << u"trip @due 2026-12-12"_s;
    QTest::newRow("iso") << u"file by 2026-12-01"_s << u"file @due 2026-12-01"_s;
    QTest::newRow("weekend") << u"clean the garage this weekend"_s
                             << u"clean the garage @due 2026-10-31"_s;
    QTest::newRow("every week starts today")
        << u"water plants every week @day"_s << u"water plants @day @weekly @due 2026-10-28"_s;
    QTest::newRow("every other week")
        << u"payroll every other week"_s << u"payroll @every 2 weeks @due 2026-10-28"_s;
    QTest::newRow("every monday") << u"standup every monday 9am"_s
                                  << u"standup @every monday @due 2026-11-02 09:00"_s;
    QTest::newRow("daily, from tomorrow")
        << u"stretch daily tomorrow"_s << u"stretch @daily @due 2026-10-29"_s;
    QTest::newRow("a number is not a time") << u"buy 3 apples"_s << u"buy 3 apples"_s;
    QTest::newRow("marbles are no month") << u"buy 2 marbles"_s << u"buy 2 marbles"_s;
    QTest::newRow("nor decorations") << u"pick up 3 decorations"_s << u"pick up 3 decorations"_s;
    QTest::newRow("nor a market") << u"read market 5 times"_s << u"read market 5 times"_s;
    QTest::newRow("a month in full, gone by this year")
        << u"trip september 12"_s << u"trip @due 2027-09-12"_s;
    QTest::newRow("no such day stays in the words") << u"pay rent feb 30"_s << u"pay rent feb 30"_s;
    QTest::newRow("no such time stays") << u"meet 25pm"_s << u"meet 25pm"_s;
    QTest::newRow("nor minutes") << u"call 3:75pm"_s << u"call 3:75pm"_s;
    QTest::newRow("nor an hour") << u"gate at 30"_s << u"gate at 30"_s;
    QTest::newRow("every other monday")
        << u"1:1 every other monday"_s << u"1:1 @every 2 weeks on monday @due 2026-11-02"_s;
    QTest::newRow("every weekday")
        << u"stand-up every weekday at 9am"_s << u"stand-up @weekdays @due 2026-10-28 09:00"_s;
}

void TestQuickAdd::reads()
{
    QFETCH(QString, typed);
    QFETCH(QString, line);
    const QuickTask task = readQuickTask(typed, QDateTime(QDate(2026, 10, 28), QTime(10, 0)));
    QCOMPARE(task.line, line);
}

void TestQuickAdd::schedulesACheckboxFromItsWords()
{
    const QDateTime now(QDate(2026, 10, 28), QTime(10, 0));
    QCOMPARE(scheduledFromWords(u"water the plants every week @day"_s, now),
             std::optional(u"water the plants @day @weekly @due 2026-10-28"_s));
    QCOMPARE(scheduledFromWords(u"call Soren tomorrow 3pm"_s, now),
             std::optional(u"call Soren @due 2026-10-29 15:00"_s));
    // Words said after the markers are the to-do's, not read; a new day replaces the old, and
    // the repeat already there stays.
    QCOMPARE(scheduledFromWords(u"pay rent friday @monthly @due 2026-10-01 #home"_s, now),
             std::optional(u"pay rent #home @monthly @due 2026-10-30"_s));
    // A repeat or a time said alone keeps the day written already.
    QCOMPARE(scheduledFromWords(u"water every week @due 2026-11-10"_s, now),
             std::optional(u"water @weekly @due 2026-11-10"_s));
    QCOMPARE(scheduledFromWords(u"call Soren 3pm @due 2026-11-10"_s, now),
             std::optional(u"call Soren @due 2026-11-10 15:00"_s));
    // Nothing said, nothing to do.
    QCOMPARE(scheduledFromWords(u"buy rope @due 2026-11-02"_s, now), std::nullopt);
    QCOMPARE(scheduledFromWords(u"buy rope"_s, now), std::nullopt);
}

void TestQuickAdd::setsAndClearsTheDayAndRepeat()
{
    QCOMPARE(withTaskSchedule(u"buy rope"_s, u"2026-11-02"_s, {}), u"buy rope @due 2026-11-02"_s);
    QCOMPARE(withTaskSchedule(u"buy rope @due 2026-11-02 #home"_s, u"2026-11-05T09:00"_s,
                              u"every week"_s),
             u"buy rope #home @weekly @due 2026-11-05 09:00"_s);
    // Cleared, tags stay with the to-do, and a done day stays at the end.
    QCOMPARE(withTaskSchedule(u"water @daily #home @due 2026-11-02 @done 2026-11-01"_s, {}, {}),
             u"water #home @done 2026-11-01"_s);
}

void TestQuickAdd::keepsWordsAfterARuleWhenRescheduled()
{
    // Rescheduling takes the rule away, but not the words typed after it.
    QCOMPARE(withTaskSchedule(u"@every monday Call Soren"_s, u"2026-11-02"_s, u"every week"_s),
             u"Call Soren @weekly @due 2026-11-02"_s);
    // Done with no day said stays done.
    QCOMPARE(withTaskSchedule(u"Pay @done #home"_s, u"2026-11-02"_s, {}),
             u"Pay #home @due 2026-11-02 @done"_s);
}

void TestQuickAdd::keepsARepeatRuleAsWritten()
{
    const QDateTime now(QDate(2026, 10, 28), QTime(10, 0));
    const QString water = u"water @every 2 weeks on Monday @due 2026-10-28"_s;
    // Its rule kept in the words, only the day changes, the rule as it was written.
    QCOMPARE(scheduledAs(water, u"2026-11-04 every 2 weeks on Monday"_s, now),
             std::optional(u"water @every 2 weeks on Monday @due 2026-11-04"_s));
    // A new rule replaces it; no words at all clear both; words of neither do nothing.
    QCOMPARE(scheduledAs(water, u"friday every month"_s, now),
             std::optional(u"water @monthly @due 2026-10-30"_s));
    QCOMPARE(scheduledAs(water, {}, now), std::optional(u"water"_s));
    QCOMPARE(scheduledAs(water, u"soon"_s, now), std::nullopt);
    // A rule that only starts with the one written is a new rule.
    const std::optional<QString> weekdays =
        scheduledAs(u"sweep @weekly"_s, u"every weekday"_s, now);
    QVERIFY(weekdays && weekdays->contains(u"@weekdays"_s));
}

QTEST_GUILESS_MAIN(TestQuickAdd)
#include "tst_quickadd.moc"
