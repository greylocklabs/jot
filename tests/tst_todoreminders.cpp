#include "ToDoReminders.h"
#include "VaultController.h"

#include "greylock/Notifications.h"
#include "jot/Settings.h"

#include <QDir>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace jot;
using namespace Qt::StringLiterals;

namespace {

/// Keeps what would be shown, and hands out ids at once.
class FakeServer : public greylock::NotificationServer
{
public:
    struct Shown
    {
        uint id;
        QString title;
        QString body;
        QStringList actions;
    };
    QList<Shown> shown;
    QList<uint> closedIds;

    void show(const QString &title, const QString &body, const QStringList &actions,
              std::function<void(uint)> done) override
    {
        const uint id = uint(shown.size()) + 1;
        shown.append({id, title, body, actions});
        done(id);
    }
    void close(uint id) override { closedIds.append(id); }
    void click(uint id, const QString &key) { Q_EMIT actionInvoked(id, key); }
};

} // namespace

class TestToDoReminders : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void init();
    void cleanup();
    void remindsAtTheTimeAndInTheMorning();
    void tellsWhatCameDueWhileClosed();
    void manyAtOnceComeAsOne();
    void buttonsOpenTickAndSnooze();
    void quietWhenTurnedOff();

private:
    void write(const QString &path, const QString &text);

    std::unique_ptr<QTemporaryDir> m_dir;
    std::unique_ptr<Settings> m_settings;
    std::unique_ptr<VaultController> m_vault;
    FakeServer m_server;
    ToDoReminders m_reminders;
    QDateTime m_now;
};

void TestToDoReminders::init()
{
    m_dir = std::make_unique<QTemporaryDir>();
    QVERIFY(QDir().mkpath(m_dir->filePath(u"Notes"_s)));
    m_settings = std::make_unique<Settings>(m_dir->filePath(u"settings.ini"_s));
    m_vault = std::make_unique<VaultController>(*m_settings);
    m_vault->setIndexPath(m_dir->filePath(u"index.sqlite"_s));
    m_server.shown.clear();
    m_server.closedIds.clear();
    // Wednesday, October 28, 2026, at 8:00.
    m_now = QDateTime(QDate(2026, 10, 28), QTime(8, 0));
    m_reminders.setNow([this] { return m_now; });
}

void TestToDoReminders::cleanup()
{
    m_vault.reset();
}

void TestToDoReminders::write(const QString &path, const QString &text)
{
    QString error;
    QVERIFY2(notefile::write(m_dir->filePath(u"Notes/"_s + path), text, &error), qPrintable(error));
}

void TestToDoReminders::remindsAtTheTimeAndInTheMorning()
{
    write(u"Today.md"_s, u"- [ ] Call Soren @minutes 📅 2026-10-28 14:00\n"
                         "- [ ] Buy rope 📅 2026-10-28\n"
                         "- [ ] Later 📅 2026-10-30\n"
                         "- [x] Done already 📅 2026-10-28 10:00\n"_s);
    QVERIFY(m_vault->openPath(m_dir->filePath(u"Notes"_s)));
    m_reminders.setup(m_vault.get(), &m_server, m_settings.get());
    QVERIFY(m_server.shown.isEmpty());

    // The morning time brings what is due today with no time of its own.
    m_now = QDateTime(QDate(2026, 10, 28), QTime(9, 0));
    m_reminders.check();
    QCOMPARE(m_server.shown.size(), 1);
    QCOMPARE(m_server.shown.last().title, u"Buy rope"_s);
    QCOMPARE(m_server.shown.last().body, u"Today\nToday"_s);
    QVERIFY(m_server.shown.last().actions.contains(u"done"_s));

    // The timed one at its time, and once only.
    m_now = QDateTime(QDate(2026, 10, 28), QTime(14, 0, 30));
    m_reminders.check();
    m_reminders.check();
    QCOMPARE(m_server.shown.size(), 2);
    QCOMPARE(m_server.shown.last().title, u"Call Soren"_s);
    QVERIFY(m_server.shown.last().body.endsWith(u"\nToday"_s));

    // The morning time is the user's to choose.
    m_settings->setReminderTime(u"7:00"_s);
    m_now = QDateTime(QDate(2026, 10, 30), QTime(7, 1));
    m_reminders.check();
    QCOMPARE(m_server.shown.last().title, u"Later"_s);
}

void TestToDoReminders::tellsWhatCameDueWhileClosed()
{
    write(u"Today.md"_s, u"- [ ] Call Soren 📅 2026-10-27 14:00\n- [ ] Old 📅 2026-10-01\n"_s);
    // Last open yesterday at noon: Call Soren came due since, the old one long before.
    m_settings->setLastReminderCheck(QDateTime(QDate(2026, 10, 27), QTime(12, 0)));
    QVERIFY(m_vault->openPath(m_dir->filePath(u"Notes"_s)));
    m_reminders.setup(m_vault.get(), &m_server, m_settings.get());
    QCOMPARE(m_server.shown.size(), 1);
    QCOMPARE(m_server.shown.first().title, u"Call Soren"_s);
    QVERIFY(m_server.shown.first().body.startsWith(u"Due October 27"_s));
    QCOMPARE(m_settings->lastReminderCheck(), m_now);
}

void TestToDoReminders::manyAtOnceComeAsOne()
{
    write(u"Today.md"_s, u"- [ ] One 📅 2026-10-28\n- [ ] Two 📅 2026-10-28\n"
                         "- [ ] Three 📅 2026-10-28\n- [ ] Four 📅 2026-10-28\n"
                         "- [ ] Five 📅 2026-10-28\n"_s);
    QVERIFY(m_vault->openPath(m_dir->filePath(u"Notes"_s)));
    m_reminders.setup(m_vault.get(), &m_server, m_settings.get());
    QSignalSpy today(&m_reminders, &ToDoReminders::todayRequested);
    m_now = QDateTime(QDate(2026, 10, 28), QTime(9, 0));
    m_reminders.check();
    QCOMPARE(m_server.shown.size(), 1);
    QCOMPARE(m_server.shown.first().title, u"5 to-dos are due"_s);
    QCOMPARE(m_server.shown.first().body, u"One\nTwo\nThree\nand 2 more"_s);
    m_server.click(1, u"default"_s);
    QCOMPARE(today.size(), 1);
}

void TestToDoReminders::buttonsOpenTickAndSnooze()
{
    write(u"Today.md"_s, u"# Today\n\n- [ ] Call Soren 📅 2026-10-28 09:00\n"_s);
    write(u"Taxes.md"_s, u"---\ntask: open\ndue: 2026-10-28T09:00\n---\n"_s);
    QVERIFY(m_vault->openPath(m_dir->filePath(u"Notes"_s)));
    m_reminders.setup(m_vault.get(), &m_server, m_settings.get());
    QSignalSpy opened(&m_reminders, &ToDoReminders::openRequested);
    m_now = QDateTime(QDate(2026, 10, 28), QTime(9, 0));
    m_reminders.check();
    QCOMPARE(m_server.shown.size(), 2);
    const uint taxes = m_server.shown.at(0).title == u"Taxes"_s ? 1 : 2;
    const uint call = 3 - taxes;

    m_server.click(call, u"default"_s);
    QCOMPARE(opened.size(), 1);
    QCOMPARE(opened.first().at(0).toString(), u"Today.md"_s);
    QCOMPARE(opened.first().at(1).toInt(), 3);

    // Snoozed, it comes again ten minutes on.
    m_server.click(taxes, u"snooze"_s);
    QCOMPARE(m_server.closedIds.last(), taxes);
    m_now = m_now.addSecs(10 * 60);
    m_reminders.check();
    QCOMPARE(m_server.shown.size(), 3);
    QCOMPARE(m_server.shown.last().title, u"Taxes"_s);

    // Done ticks the box, or the task note.
    m_server.click(3, u"done"_s);
    QString error;
    QVERIFY(
        notefile::read(m_dir->filePath(u"Notes/Taxes.md"_s), &error)->contains(u"task: done"_s));
    // A notification already closed is not answered.
    m_server.click(call, u"default"_s);
    QCOMPARE(opened.size(), 1);
}

void TestToDoReminders::quietWhenTurnedOff()
{
    write(u"Today.md"_s, u"- [ ] Buy rope 📅 2026-10-28\n"_s);
    m_settings->setRemindersOn(false);
    QVERIFY(m_vault->openPath(m_dir->filePath(u"Notes"_s)));
    m_reminders.setup(m_vault.get(), &m_server, m_settings.get());
    m_now = QDateTime(QDate(2026, 10, 28), QTime(9, 0));
    m_reminders.check();
    QVERIFY(m_server.shown.isEmpty());
}

QTEST_MAIN(TestToDoReminders)
#include "tst_todoreminders.moc"
