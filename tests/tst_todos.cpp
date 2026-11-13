#include "jot/ToDos.h"

#include <QDir>
#include <QTemporaryDir>
#include <QTest>
#include <QTimeZone>

using namespace jot;
using namespace Qt::StringLiterals;

class TestToDos : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void init();
    void cleanup();
    void showsEachView();
    void listsDoneOnesMostRecentFirst();
    void filtersByTag();
    void filtersByPriority();
    void sortsAsAsked();
    void remindsAtTheTimeOrInTheMorning();

private:
    QStringList titles(ToDoView view, const QString &tag = {}) const;

    std::unique_ptr<QTemporaryDir> m_dir;
    std::unique_ptr<Vault> m_vault;
    std::unique_ptr<VaultIndex> m_index;
    const QDate m_today{2026, 10, 28};
};

void TestToDos::init()
{
    m_dir = std::make_unique<QTemporaryDir>();
    m_vault = std::make_unique<Vault>(m_dir->filePath(u"vault"_s));
    QVERIFY(QDir().mkpath(m_vault->root()));
    const auto write = [this](const QString &path, const QString &text) {
        QString error;
        QVERIFY2(notefile::write(m_vault->absolute(path), text, &error), qPrintable(error));
    };
    write(u"Inbox.md"_s, u"- [ ] Call Soren #calls @due 2026-10-28 14:00 @p1\n"
                         "- [ ] Buy rope @due 2026-10-28\n"
                         "- [ ] Late fee #home/money @due 2026-10-20\n"
                         "- [ ] Plan trip @due 2026-11-02\n"
                         "- [ ] Someday read Proust\n"
                         "- [x] Done already @due 2026-10-28\n"_s);
    write(u"Taxes.md"_s, u"- [ ] Taxes #home @due 2026-10-30 @p1\n"_s);
    write(u"Fence.md"_s, u"- [ ] Fence\n"_s);
    write(u"Dentist.md"_s, u"- [ ] Dentist @due 2026-10-28 09:00\n"_s);
    // What was once a task note is a note like any other.
    write(u"Retired.md"_s, u"---\ntask: open\ndue: 2026-10-28\n---\nJust words.\n"_s);
    write(u"Done.md"_s, u"- [x] Pay rent #home @done 2026-10-26\n"
                        "- [x] Water plants @done 2026-10-27\n"
                        "- [X] Old one, no day\n"_s);
    write(u"Garage.md"_s, u"- [x] Garage @done 2026-10-28\n"_s);
    m_index = std::make_unique<VaultIndex>(*m_vault, m_dir->filePath(u"index.sqlite"_s));
    QString error;
    QVERIFY2(m_index->open(&error), qPrintable(error));
    m_index->refresh();
}

void TestToDos::cleanup()
{
    m_index.reset();
}

QStringList TestToDos::titles(ToDoView view, const QString &tag) const
{
    QStringList found;
    for (const ToDo &each : toDos(*m_index, view, m_today, tag))
        found << each.title;
    return found;
}

void TestToDos::showsEachView()
{
    // Overdue and today's, a day's untimed before its timed.
    QCOMPARE(titles(ToDoView::Today), (QStringList{u"Late fee #home/money"_s, u"Buy rope"_s,
                                                   u"Dentist"_s, u"Call Soren #calls"_s}));
    QCOMPARE(titles(ToDoView::Upcoming), (QStringList{u"Taxes #home"_s, u"Plan trip"_s}));
    // By note.
    QCOMPARE(titles(ToDoView::Anytime), (QStringList{u"Fence"_s, u"Someday read Proust"_s}));
    QCOMPARE(titles(ToDoView::All).size(), 8);
    QVERIFY(!titles(ToDoView::All).contains(u"Retired"_s));

    const ToDo call = toDos(*m_index, ToDoView::Today, m_today).last();
    QCOMPARE(call.note, u"Inbox"_s);
    QCOMPARE(call.line, 1);
    QCOMPARE(call.due, u"2026-10-28T14:00"_s);
    QCOMPARE(call.dueDate(), m_today);
    QCOMPARE(call.tags, QStringList{u"calls"_s});
    QCOMPARE(toDoView(u"upcoming"_s), ToDoView::Upcoming);
    QVERIFY(!toDoView(u"later"_s));
}

void TestToDos::listsDoneOnesMostRecentFirst()
{
    // By when they were done; those with no day written last.
    QCOMPARE(titles(ToDoView::Done),
             (QStringList{u"Garage"_s, u"Water plants"_s, u"Pay rent #home"_s, u"Old one, no day"_s,
                          u"Done already"_s}));
    QCOMPARE(titles(ToDoView::Done, u"home"_s), QStringList{u"Pay rent #home"_s});
    const ToDo garage = toDos(*m_index, ToDoView::Done, m_today).first();
    QCOMPARE(garage.doneAt(), QDateTime(QDate(2026, 10, 28), QTime(0, 0)));
    QCOMPARE(toDos(*m_index, ToDoView::Done, m_today).at(1).doneAt(),
             QDateTime(QDate(2026, 10, 27), QTime(0, 0)));
    // Done ones stay out of the other views.
    QVERIFY(!titles(ToDoView::All).contains(u"Garage"_s));
    QVERIFY(!titles(ToDoView::All).contains(u"Pay rent"_s));
    QCOMPARE(toDoView(u"done"_s), std::optional(ToDoView::Done));
}

void TestToDos::filtersByTag()
{
    // A tag takes in the tags nested in it, and a task note's own.
    QCOMPARE(titles(ToDoView::All, u"home"_s),
             (QStringList{u"Late fee #home/money"_s, u"Taxes #home"_s}));
    QCOMPARE(titles(ToDoView::Today, u"home"_s), QStringList{u"Late fee #home/money"_s});
    QCOMPARE(titles(ToDoView::Today, u"calls"_s), QStringList{u"Call Soren #calls"_s});
}

void TestToDos::sortsAsAsked()
{
    QList<ToDo> list;
    const auto make = [](const QString &title, const QString &due, int priority, int day) {
        ToDo each;
        each.title = title;
        each.due = due;
        each.priority = priority;
        each.created = QDateTime(QDate(2026, 10, day), QTime(9, 0));
        return each;
    };
    list = {make(u"ch 10"_s, u"2026-10-30"_s, 0, 3), make(u"Ch 2"_s, {}, 2, 1),
            make(u"apple"_s, u"2026-10-29T14:00"_s, 1, 2), make(u"Dusk"_s, u"2026-10-29"_s, 4, 4)};
    const auto titles = [&](ToDoSort sort, bool descending) {
        QList<ToDo> sorted = list;
        sortToDos(sorted, sort, descending);
        QStringList out;
        for (const ToDo &each : sorted)
            out << each.title;
        return out;
    };
    // By day and time, the untimed first in a day; those with no day last either way.
    QCOMPARE(titles(ToDoSort::Due, false),
             (QStringList{u"Dusk"_s, u"apple"_s, u"ch 10"_s, u"Ch 2"_s}));
    QCOMPARE(titles(ToDoSort::Due, true),
             (QStringList{u"ch 10"_s, u"apple"_s, u"Dusk"_s, u"Ch 2"_s}));
    QCOMPARE(titles(ToDoSort::Created, false),
             (QStringList{u"Ch 2"_s, u"apple"_s, u"ch 10"_s, u"Dusk"_s}));
    QCOMPARE(titles(ToDoSort::Created, true),
             (QStringList{u"Dusk"_s, u"ch 10"_s, u"apple"_s, u"Ch 2"_s}));
    // Case aside, and numbers in order.
    QCOMPARE(titles(ToDoSort::Title, false),
             (QStringList{u"apple"_s, u"Ch 2"_s, u"ch 10"_s, u"Dusk"_s}));
    QCOMPARE(titles(ToDoSort::Title, true),
             (QStringList{u"Dusk"_s, u"ch 10"_s, u"Ch 2"_s, u"apple"_s}));
    // Urgent first, or low first; none last either way.
    QCOMPARE(titles(ToDoSort::Priority, false),
             (QStringList{u"apple"_s, u"Ch 2"_s, u"Dusk"_s, u"ch 10"_s}));
    QCOMPARE(titles(ToDoSort::Priority, true),
             (QStringList{u"Dusk"_s, u"Ch 2"_s, u"apple"_s, u"ch 10"_s}));
    QCOMPARE(toDoSort(u"title"_s), ToDoSort::Title);
    QVERIFY(!toDoSort(u"mood"_s));
}

void TestToDos::filtersByPriority()
{
    // A checkbox's @p1 and a task note's priority: p1, and those with none.
    const auto of = [this](int priority) {
        QStringList found;
        for (const ToDo &each : toDos(*m_index, ToDoView::All, m_today, {}, priority))
            found << each.title;
        return found;
    };
    QCOMPARE(of(1), (QStringList{u"Call Soren #calls"_s, u"Taxes #home"_s}));
    QVERIFY(of(2).isEmpty());
    QVERIFY(of(0).contains(u"Buy rope"_s));
    QVERIFY(!of(0).contains(u"Taxes"_s));
    QCOMPARE(toDos(*m_index, ToDoView::Today, m_today).last().priority, 1);
}

void TestToDos::remindsAtTheTimeOrInTheMorning()
{
    const QTime nine(9, 0);
    ToDo todo;
    QVERIFY(!reminderAt(todo, nine).isValid());
    todo.due = u"2026-10-29"_s;
    QCOMPARE(reminderAt(todo, nine), QDateTime(QDate(2026, 10, 29), nine));
    todo.due = u"2026-10-29T14:30"_s;
    QCOMPARE(reminderAt(todo, nine), QDateTime(QDate(2026, 10, 29), QTime(14, 30)));
    todo.due = u"2026-10-29 07:15"_s;
    QCOMPARE(reminderAt(todo, nine), QDateTime(QDate(2026, 10, 29), QTime(7, 15)));
    todo.due = u"2026-10-29T14:30:00Z"_s;
    QCOMPARE(reminderAt(todo, nine),
             QDateTime(QDate(2026, 10, 29), QTime(14, 30), QTimeZone::UTC).toLocalTime());

    // Ticking finds a checkbox by its text as written.
    const ToDo call = toDos(*m_index, ToDoView::Today, m_today).last();
    QCOMPARE(call.text, u"Call Soren #calls @due 2026-10-28 14:00 @p1"_s);
}

QTEST_GUILESS_MAIN(TestToDos)
#include "tst_todos.moc"
