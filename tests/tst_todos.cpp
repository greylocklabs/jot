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
    write(u"Inbox.md"_s, u"- [ ] Call Soren #calls @due 2026-10-28 14:00\n"
                         "- [ ] Buy rope @due 2026-10-28\n"
                         "- [ ] Late fee #home/money @due 2026-10-20\n"
                         "- [ ] Plan trip @due 2026-11-02\n"
                         "- [ ] Someday read Proust\n"
                         "- [x] Done already @due 2026-10-28\n"_s);
    write(u"Taxes.md"_s, u"---\ntask: open\ndue: 2026-10-30\ntags: [home]\n---\n"_s);
    write(u"Fence.md"_s, u"---\ntask: open\n---\n"_s);
    // A time written with a space, as YAML allows, sorts by the clock too.
    write(u"Dentist.md"_s, u"---\ntask: open\ndue: 2026-10-28 09:00\n---\n"_s);
    write(u"Done.md"_s, u"- [x] Pay rent #home @done 2026-10-26\n"
                        "- [x] Water plants @done 2026-10-27\n"
                        "- [X] Old one, no day\n"_s);
    write(u"Garage.md"_s, u"---\ntask: done\ndone: 2026-10-27T15:30:00Z\n---\n"_s);
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
    QCOMPARE(titles(ToDoView::Upcoming), (QStringList{u"Taxes"_s, u"Plan trip"_s}));
    // Task notes first, then each note's boxes.
    QCOMPARE(titles(ToDoView::Anytime), (QStringList{u"Fence"_s, u"Someday read Proust"_s}));
    QCOMPARE(titles(ToDoView::All).size(), 8);

    const ToDo call = toDos(*m_index, ToDoView::Today, m_today).last();
    QCOMPARE(call.kind, u"task"_s);
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
    // By when they were done, a moment before a bare day's midnight on the same day; those with
    // no day written last.
    QCOMPARE(titles(ToDoView::Done),
             (QStringList{u"Garage"_s, u"Water plants"_s, u"Pay rent #home"_s, u"Old one, no day"_s,
                          u"Done already"_s}));
    QCOMPARE(titles(ToDoView::Done, u"home"_s), QStringList{u"Pay rent #home"_s});
    const ToDo garage = toDos(*m_index, ToDoView::Done, m_today).first();
    QCOMPARE(garage.kind, u"note"_s);
    QCOMPARE(garage.doneAt(),
             QDateTime(QDate(2026, 10, 27), QTime(15, 30), QTimeZone::UTC).toLocalTime());
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
             (QStringList{u"Late fee #home/money"_s, u"Taxes"_s}));
    QCOMPARE(titles(ToDoView::Today, u"home"_s), QStringList{u"Late fee #home/money"_s});
    QCOMPARE(titles(ToDoView::Today, u"calls"_s), QStringList{u"Call Soren #calls"_s});
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
    QCOMPARE(call.text, u"Call Soren #calls @due 2026-10-28 14:00"_s);
}

QTEST_GUILESS_MAIN(TestToDos)
#include "tst_todos.moc"
