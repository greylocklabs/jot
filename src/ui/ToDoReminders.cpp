#include "ToDoReminders.h"

#include "VaultController.h"

#include "greylock/Notifications.h"
#include "jot/Settings.h"

#include <QLocale>

#include <algorithm>

using namespace Qt::StringLiterals;

namespace jot {

namespace {

/// The longest the timer sleeps, so a clock changed or a computer woken is noticed.
constexpr qint64 kLongestWait = 60LL * 60 * 1000;

} // namespace

ToDoReminders::ToDoReminders(QObject *parent) : QObject(parent)
{
    m_timer.setSingleShot(true);
    connect(&m_timer, &QTimer::timeout, this, &ToDoReminders::check);
    // A burst of changes to the notes is looked at once.
    m_soon.setSingleShot(true);
    m_soon.setInterval(0);
    connect(&m_soon, &QTimer::timeout, this, &ToDoReminders::check);
}

void ToDoReminders::setup(VaultController *vault, greylock::NotificationServer *server,
                          Settings *settings)
{
    for (QObject *old : {static_cast<QObject *>(m_vault), static_cast<QObject *>(m_server),
                         static_cast<QObject *>(m_settings)}) {
        if (old)
            disconnect(old, nullptr, this, nullptr);
    }
    m_vault = vault;
    m_server = server;
    m_settings = settings;
    m_shown.clear();
    m_snoozed.clear();
    // What came due while Jot was closed is told now; on the very first run there is none.
    m_lastCheck =
        m_settings->lastReminderCheck().isValid() ? m_settings->lastReminderCheck() : m_now();
    connect(m_vault, &VaultController::notesChanged, &m_soon, qOverload<>(&QTimer::start));
    connect(m_settings, &Settings::remindersChanged, &m_soon, qOverload<>(&QTimer::start));
    connect(m_server, &greylock::NotificationServer::actionInvoked, this, &ToDoReminders::onAction);
    connect(m_server, &greylock::NotificationServer::closed, this,
            [this](uint id) { m_shown.remove(id); });
    check();
}

void ToDoReminders::check()
{
    if (!m_vault || !m_server || !m_settings)
        return;
    const QDateTime now = m_now();
    const QDateTime since = m_lastCheck;
    m_lastCheck = now;
    m_settings->setLastReminderCheck(now);
    const VaultIndex *index = m_vault->index();
    if (!m_settings->remindersOn() || !index) {
        m_timer.stop();
        return;
    }

    const QTime allDay = QTime::fromString(m_settings->reminderTime(), u"HH:mm"_s);
    QList<ToDo> due;
    QDateTime next;
    for (const ToDo &todo : toDos(*index, ToDoView::All, now.date())) {
        const QDateTime at = reminderAt(todo, allDay);
        if (!at.isValid())
            continue;
        if (at > since && at <= now)
            due.append(todo);
        else if (at > now && (!next.isValid() || at < next))
            next = at;
    }
    for (auto it = m_snoozed.begin(); it != m_snoozed.end();) {
        if (it->at <= now) {
            due.append(it->todo);
            it = m_snoozed.erase(it);
        } else {
            if (!next.isValid() || it->at < next)
                next = it->at;
            ++it;
        }
    }

    if (due.size() > kMostAtOnce) {
        notifyMany(due);
    } else {
        for (const ToDo &todo : std::as_const(due))
            notify(todo);
    }
    schedule(next);
}

void ToDoReminders::schedule(const QDateTime &next)
{
    const qint64 wait = next.isValid() ? m_now().msecsTo(next) : kLongestWait;
    m_timer.start(int(std::clamp<qint64>(wait, 1000, kLongestWait)));
}

void ToDoReminders::notify(const ToDo &todo)
{
    const QTime allDay = QTime::fromString(m_settings->reminderTime(), u"HH:mm"_s);
    const QDateTime at = reminderAt(todo, allDay);
    const QDate today = m_now().date();
    const bool timed = todo.due.size() > 10;
    QString when = timed ? QLocale().toString(at.time(), QLocale::ShortFormat) : tr("Today");
    if (at.date() < today)
        when = tr("Due %1").arg(QLocale().toString(at.date(), u"MMMM d"_s));
    // Where it is written, unless it is a note of its own.
    const QString body = todo.note.isEmpty() ? when : when + u'\n' + todo.note;
    // "default" is a click on the notification itself, which daemons do not show as a button.
    const QStringList actions{u"default"_s, tr("Open in Jot"),
                              u"done"_s,    tr("Done"),
                              u"snooze"_s,  tr("Snooze %n min", nullptr, kSnoozeMinutes)};
    // Daemons may read the body as markup, and a note's name is the user's own text.
    m_server->show(todo.title, body.toHtmlEscaped(), actions,
                   [guard = QPointer(this), todo](uint id) {
                       if (guard && id != 0)
                           guard->m_shown.insert(id, todo);
                   });
}

void ToDoReminders::notifyMany(const QList<ToDo> &todos)
{
    QStringList titles;
    for (qsizetype i = 0; i < std::min<qsizetype>(todos.size(), kMostAtOnce); ++i)
        titles << todos.at(i).title;
    if (todos.size() > kMostAtOnce)
        titles << tr("and %n more", nullptr, int(todos.size() - kMostAtOnce));
    m_server->show(tr("%n to-dos are due", nullptr, int(todos.size())),
                   titles.join(u'\n').toHtmlEscaped(), {u"default"_s, tr("Open in Jot")},
                   [guard = QPointer(this)](uint id) {
                       if (guard && id != 0)
                           guard->m_shown.insert(id, ToDo{});
                   });
}

void ToDoReminders::onAction(uint id, const QString &key)
{
    const auto found = m_shown.constFind(id);
    if (found == m_shown.cend())
        return;
    const ToDo todo = *found;
    if (todo.path.isEmpty()) {
        if (key != u"default"_s)
            return;
        Q_EMIT todayRequested();
    } else if (key == u"default"_s) {
        Q_EMIT openRequested(todo.path, todo.line);
    } else if (key == u"done"_s) {
        if (todo.kind == u"note"_s)
            m_vault->completeTask(todo.path);
        else
            m_vault->setTaskDone(todo.path, todo.line, true, todo.text);
    } else if (key == u"snooze"_s) {
        m_snoozed.append({todo, m_now().addSecs(60LL * kSnoozeMinutes)});
        m_soon.start();
    } else {
        return;
    }
    m_server->close(id);
    m_shown.remove(id);
}

} // namespace jot
