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
    QTest::newRow("an address is no label")
        << u"Mail hello@example.org"_s << TaskLine{u"Mail hello@example.org"_s, {}, {}, {}, {}};
    QTest::newRow("a date that is not one")
        << u"Pay 📅 soon"_s << TaskLine{u"Pay"_s, {}, {}, {}, {}};
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

QTEST_GUILESS_MAIN(TestTaskLine)
#include "tst_taskline.moc"
