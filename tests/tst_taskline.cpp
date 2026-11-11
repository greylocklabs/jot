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
    void findsWhereMarkersAre();
    void writesMarkers();
    void offersMarkersAsTheyAreTyped();
    void findsTheNextDay();
    void writesTheNextOneWhenDone();
};

void TestTaskLine::reads_data()
{
    QTest::addColumn<QString>("text");
    QTest::addColumn<TaskLine>("line");

    QTest::newRow("plain") << u"Buy rope"_s << TaskLine{u"Buy rope"_s, {}, {}, {}};
    QTest::newRow("due") << u"Call Soren @due 2026-10-29"_s
                         << TaskLine{u"Call Soren"_s, u"2026-10-29"_s, {}, {}};
    QTest::newRow("due at a time") << u"Call Soren @due 2026-10-29 9:30"_s
                                   << TaskLine{u"Call Soren"_s, u"2026-10-29T09:30"_s, {}, {}};
    QTest::newRow("tags, and words with an @")
        << u"Call Soren @minutes #calls @due 2026-10-29 14:00"_s
        << TaskLine{u"Call Soren @minutes #calls"_s, u"2026-10-29T14:00"_s, {}, {}};
    QTest::newRow("repeats and done")
        << u"Water plants @weekly @due 2026-10-27 @done 2026-10-27"_s
        << TaskLine{u"Water plants"_s, u"2026-10-27"_s, u"every week"_s, u"2026-10-27"_s};
    QTest::newRow("words after the date") << u"Pay rent @due 2026-11-01 #home"_s
                                          << TaskLine{u"Pay rent #home"_s, u"2026-11-01"_s, {}, {}};
    QTest::newRow("a rule up to a tag")
        << u"Water @every 2 weeks on monday #home @due 2026-11-02"_s
        << TaskLine{u"Water #home"_s, u"2026-11-02"_s, u"every 2 weeks on monday"_s, {}};
    QTest::newRow("shortcuts, case aside")
        << u"Stretch @Weekdays"_s << TaskLine{u"Stretch"_s, {}, u"every weekday"_s, {}};
    QTest::newRow("a time after a T")
        << u"Call @due 2026-11-02T9:30"_s << TaskLine{u"Call"_s, u"2026-11-02T09:30"_s, {}, {}};
    QTest::newRow("@ words that are no markers")
        << u"Plan @trip and @every"_s << TaskLine{u"Plan @trip and @every"_s, {}, {}, {}};
    QTest::newRow("an address") << u"Mail hello@example.org"_s
                                << TaskLine{u"Mail hello@example.org"_s, {}, {}, {}};
    QTest::newRow("a day that is not one")
        << u"Pay @due soon @done 2026-02-30"_s
        << TaskLine{u"Pay @due soon @done 2026-02-30"_s, {}, {}, {}};
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
}

void TestTaskLine::findsWhereMarkersAre()
{
    const QString text = u"Water @weekly #home @due 2026-11-02 14:00 @done 2026-11-01"_s;
    const QList<TaskMarker> found = taskMarkers(text);
    QCOMPARE(found.size(), 3);
    QCOMPARE(text.sliced(found.at(0).start, found.at(0).end - found.at(0).start), u"@weekly"_s);
    QCOMPARE(text.sliced(found.at(1).start, found.at(1).end - found.at(1).start),
             u"@due 2026-11-02 14:00"_s);
    QCOMPARE(found.at(1).value, u"2026-11-02T14:00"_s);
    QCOMPARE(found.at(2).kind, TaskMarker::Done);
    // Taken out with the space before them, the rest as it was.
    QCOMPARE(withoutMarkers(text, {TaskMarker::Due, TaskMarker::Done}), u"Water @weekly #home"_s);
    QCOMPARE(withoutMarkers(u"@daily  water"_s, {TaskMarker::Repeat}), u"water"_s);
    QCOMPARE(withoutMarkers(u"  water @daily"_s, {TaskMarker::Repeat}), u"  water"_s);
}

void TestTaskLine::writesMarkers()
{
    QCOMPARE(repeatMarker(u"every week"_s), u"@weekly"_s);
    QCOMPARE(repeatMarker(u"Every Weekday"_s), u"@weekdays"_s);
    QCOMPARE(repeatMarker(u"every 2 weeks on monday"_s), u"@every 2 weeks on monday"_s);
    QCOMPARE(dueMarker(u"2026-11-02T14:00"_s), u"@due 2026-11-02 14:00"_s);
    QCOMPARE(doneMarker(QDate(2026, 11, 1)), u"@done 2026-11-01"_s);
}

void TestTaskLine::offersMarkersAsTheyAreTyped()
{
    const QDate wednesday(2026, 10, 28);
    const auto shown = [&](const QString &typed) {
        QStringList out;
        for (const MarkerSuggestion &each : markerSuggestions(typed, wednesday))
            out << each.shown;
        return out;
    };
    QCOMPARE(shown(u"we"_s), (QStringList{u"@weekly"_s, u"@weekdays"_s}));
    QCOMPARE(shown(u"DUE"_s).size(), 4);
    QVERIFY(shown(u""_s).size() > 8);
    QVERIFY(shown(u"zz"_s).isEmpty());
    // A day is offered as the day itself; next week is next Monday.
    const QList<MarkerSuggestion> due = markerSuggestions(u"due"_s, wednesday);
    QCOMPARE(due.at(0).text, u"@due 2026-10-28"_s);
    QCOMPARE(due.at(1).text, u"@due 2026-10-29"_s);
    QCOMPARE(due.at(2).text, u"@due 2026-11-02"_s);
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
    // One that does not repeat is ticked and says when.
    QCOMPARE(completedTaskLine(u"- [ ] Buy rope @due 2026-10-28"_s, today),
             u"- [x] Buy rope @due 2026-10-28 @done 2026-10-28"_s);
    // Ticked again, as when typed back, it says only the latest day.
    QCOMPARE(completedTaskLine(u"- [ ] Buy rope @done 2026-10-01 "_s, today),
             u"- [x] Buy rope @done 2026-10-28"_s);
    // One that does gets its next one above it, at the same time of day.
    QCOMPARE(completedTaskLine(u"  - [ ] Water @weekly #home @due 2026-10-28 09:00"_s, today),
             u"  - [ ] Water @weekly #home @due 2026-11-04 09:00\n"
             "  - [x] Water @weekly #home @due 2026-10-28 09:00 @done 2026-10-28"_s);
    // Done late, it comes round after today, not on a day gone by.
    QCOMPARE(completedTaskLine(u"- [ ] Stretch @daily @due 2026-10-20"_s, today),
             u"- [ ] Stretch @daily @due 2026-10-29\n"
             "- [x] Stretch @daily @due 2026-10-20 @done 2026-10-28"_s);
    // Windows line breaks stay, and one with no day, or a rule Jot cannot read, is only ticked
    // with its day.
    QCOMPARE(
        completedTaskLine(u"- [ ] Pay @monthly @due 2026-10-28\r"_s, today),
        u"- [ ] Pay @monthly @due 2026-11-28\r\n- [x] Pay @monthly @due 2026-10-28 @done 2026-10-28\r"_s);
    QCOMPARE(completedTaskLine(u"- [ ] Pay @weekly"_s, today),
             u"- [x] Pay @weekly @done 2026-10-28"_s);
    QCOMPARE(completedTaskLine(u"- [ ] Huge @every 400000000 weeks @due 2000-01-01"_s, today),
             u"- [x] Huge @every 400000000 weeks @due 2000-01-01 @done 2026-10-28"_s);
    QCOMPARE(completedTaskLine(u"- [ ] Moon @every full moon @due 2026-10-28"_s, today),
             u"- [x] Moon @every full moon @due 2026-10-28 @done 2026-10-28"_s);
}

QTEST_GUILESS_MAIN(TestTaskLine)
#include "tst_taskline.moc"
