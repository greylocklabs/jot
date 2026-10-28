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
};

void TestQuickAdd::reads_data()
{
    QTest::addColumn<QString>("typed");
    QTest::addColumn<QString>("line");

    // As of Wednesday, October 28, 2026, at 10:00.
    QTest::newRow("nothing") << u"Buy rope"_s << u"Buy rope"_s;
    QTest::newRow("tomorrow, a time, labels and tags")
        << u"call Soren tomorrow 3pm @minutes #calls"_s
        << u"call Soren @minutes #calls 📅 2026-10-29 15:00"_s;
    QTest::newRow("today") << u"pay rent today"_s << u"pay rent 📅 2026-10-28"_s;
    QTest::newRow("a time alone is today")
        << u"stand-up at 9:30 am"_s << u"stand-up 📅 2026-10-28 09:30"_s;
    QTest::newRow("24 hours") << u"meet Mara tomorrow at 15:45"_s
                              << u"meet Mara 📅 2026-10-29 15:45"_s;
    QTest::newRow("noon") << u"lunch friday at noon"_s << u"lunch 📅 2026-10-30 12:00"_s;
    QTest::newRow("a weekday is the next one") << u"plan wednesday"_s << u"plan 📅 2026-11-04"_s;
    QTest::newRow("short weekday after on")
        << u"mow the lawn on sat"_s << u"mow the lawn 📅 2026-10-31"_s;
    QTest::newRow("a short day name alone stays") << u"sat by the fire"_s << u"sat by the fire"_s;
    QTest::newRow("next week") << u"taxes next week"_s << u"taxes 📅 2026-11-02"_s;
    QTest::newRow("next month") << u"renew next month"_s << u"renew 📅 2026-11-01"_s;
    QTest::newRow("in days") << u"follow up in 3 days"_s << u"follow up 📅 2026-10-31"_s;
    QTest::newRow("in a week") << u"follow up in a week"_s << u"follow up 📅 2026-11-04"_s;
    QTest::newRow("month and day") << u"dentist Nov 3rd 2pm"_s << u"dentist 📅 2026-11-03 14:00"_s;
    QTest::newRow("a day gone by is next year")
        << u"birthday march 2"_s << u"birthday 📅 2027-03-02"_s;
    QTest::newRow("day and month") << u"trip 12 december"_s << u"trip 📅 2026-12-12"_s;
    QTest::newRow("iso") << u"file by 2026-12-01"_s << u"file 📅 2026-12-01"_s;
    QTest::newRow("weekend") << u"clean the garage this weekend"_s
                             << u"clean the garage 📅 2026-10-31"_s;
    QTest::newRow("every week starts today")
        << u"water plants every week @day"_s << u"water plants @day 🔁 every week 📅 2026-10-28"_s;
    QTest::newRow("every other week")
        << u"payroll every other week"_s << u"payroll 🔁 every 2 weeks 📅 2026-10-28"_s;
    QTest::newRow("every monday") << u"standup every monday 9am"_s
                                  << u"standup 🔁 every monday 📅 2026-11-02 09:00"_s;
    QTest::newRow("daily, from tomorrow")
        << u"stretch daily tomorrow"_s << u"stretch 🔁 every day 📅 2026-10-29"_s;
    QTest::newRow("a number is not a time") << u"buy 3 apples"_s << u"buy 3 apples"_s;
    QTest::newRow("no such day stays in the words") << u"pay rent feb 30"_s << u"pay rent feb 30"_s;
    QTest::newRow("no such time stays") << u"meet 25pm"_s << u"meet 25pm"_s;
    QTest::newRow("nor minutes") << u"call 3:75pm"_s << u"call 3:75pm"_s;
    QTest::newRow("nor an hour") << u"gate at 30"_s << u"gate at 30"_s;
    QTest::newRow("every other monday")
        << u"1:1 every other monday"_s << u"1:1 🔁 every 2 weeks on monday 📅 2026-11-02"_s;
    QTest::newRow("every weekday")
        << u"stand-up every weekday at 9am"_s << u"stand-up 🔁 every weekday 📅 2026-10-28 09:00"_s;
}

void TestQuickAdd::reads()
{
    QFETCH(QString, typed);
    QFETCH(QString, line);
    const QuickTask task = readQuickTask(typed, QDateTime(QDate(2026, 10, 28), QTime(10, 0)));
    QCOMPARE(task.line, line);
}

QTEST_GUILESS_MAIN(TestQuickAdd)
#include "tst_quickadd.moc"
