#include "jot/ToDos.h"

#include <QDir>
#include <QTemporaryDir>
#include <QTest>

using namespace jot;
using namespace Qt::StringLiterals;

class TestToDos : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void init();
    void cleanup();
    void showsEachView();
    void filtersByLabelAndTag();

private:
    QStringList titles(ToDoView view, const QString &label = {}, const QString &tag = {}) const;

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
    write(u"Inbox.md"_s, u"- [ ] Call Soren @minutes #calls 📅 2026-10-28 14:00\n"
                         "- [ ] Buy rope @minutes 📅 2026-10-28\n"
                         "- [ ] Late fee @hours #home/money 📅 2026-10-20\n"
                         "- [ ] Plan trip @days 📅 2026-11-02\n"
                         "- [ ] Someday read Proust\n"
                         "- [x] Done already 📅 2026-10-28\n"_s);
    write(u"Taxes.md"_s,
          u"---\ntask: open\ndue: 2026-10-30\nlabels: [days]\ntags: [home]\n---\n"_s);
    write(u"Fence.md"_s, u"---\ntask: open\n---\n"_s);
    m_index = std::make_unique<VaultIndex>(*m_vault, m_dir->filePath(u"index.sqlite"_s));
    QString error;
    QVERIFY2(m_index->open(&error), qPrintable(error));
    m_index->refresh();
}

void TestToDos::cleanup()
{
    m_index.reset();
}

QStringList TestToDos::titles(ToDoView view, const QString &label, const QString &tag) const
{
    QStringList found;
    for (const ToDo &each : toDos(*m_index, view, m_today, label, tag))
        found << each.title;
    return found;
}

void TestToDos::showsEachView()
{
    // Overdue and today's, a day's untimed before its timed.
    QCOMPARE(titles(ToDoView::Today),
             (QStringList{u"Late fee #home/money"_s, u"Buy rope"_s, u"Call Soren #calls"_s}));
    QCOMPARE(titles(ToDoView::Upcoming), (QStringList{u"Taxes"_s, u"Plan trip"_s}));
    // Task notes first, then each note's boxes.
    QCOMPARE(titles(ToDoView::Anytime), (QStringList{u"Fence"_s, u"Someday read Proust"_s}));
    QCOMPARE(titles(ToDoView::All).size(), 7);

    const ToDo call = toDos(*m_index, ToDoView::Today, m_today).last();
    QCOMPARE(call.kind, u"task"_s);
    QCOMPARE(call.note, u"Inbox"_s);
    QCOMPARE(call.line, 1);
    QCOMPARE(call.due, u"2026-10-28T14:00"_s);
    QCOMPARE(call.dueDate(), m_today);
    QCOMPARE(call.labels, QStringList{u"minutes"_s});
    QCOMPARE(call.tags, QStringList{u"calls"_s});
    QCOMPARE(toDoView(u"upcoming"_s), ToDoView::Upcoming);
    QVERIFY(!toDoView(u"later"_s));
}

void TestToDos::filtersByLabelAndTag()
{
    QCOMPARE(titles(ToDoView::All, u"Minutes"_s),
             (QStringList{u"Buy rope"_s, u"Call Soren #calls"_s}));
    QCOMPARE(titles(ToDoView::Upcoming, u"days"_s), (QStringList{u"Taxes"_s, u"Plan trip"_s}));
    // A tag takes in the tags nested in it, and a task note's own.
    QCOMPARE(titles(ToDoView::All, {}, u"home"_s),
             (QStringList{u"Late fee #home/money"_s, u"Taxes"_s}));
    QCOMPARE(titles(ToDoView::Today, u"hours"_s, u"home"_s),
             QStringList{u"Late fee #home/money"_s});
}

QTEST_GUILESS_MAIN(TestToDos)
#include "tst_todos.moc"
