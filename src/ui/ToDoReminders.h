#pragma once

#include "jot/ToDos.h"

#include <QDateTime>
#include <QHash>
#include <QObject>
#include <QPointer>
#include <QTimer>

#include <functional>

namespace greylock {
class NotificationServer;
}

namespace jot {

class Settings;
class VaultController;

/// Desktop notifications for to-dos as they come due: at their time, or at the morning time in
/// Settings for those due on a day without one. Those that came due while Jot was closed are
/// told when it starts. Each has buttons to open the to-do, tick it done, or snooze it.
class ToDoReminders : public QObject
{
    Q_OBJECT

public:
    using Now = std::function<QDateTime()>;

    explicit ToDoReminders(QObject *parent = nullptr);

    /// Starts reminding. Until then nothing is shown, as in tests and screenshots.
    void setup(VaultController *vault, greylock::NotificationServer *server, Settings *settings);
    /// Replaces the wall clock, for tests.
    void setNow(Now now) { m_now = std::move(now); }

    /// Tells what came due since the last look, then waits for the next. Runs by itself;
    /// public so tests can step the clock.
    void check();

    /// More than this many coming due at once are told in one notification.
    static constexpr int kMostAtOnce = 3;
    static constexpr int kSnoozeMinutes = 10;

Q_SIGNALS:
    /// Asks the window to show the to-do on `line` of the note at `path`.
    void openRequested(const QString &path, int line);
    /// Asks the window to show what is due today.
    void todayRequested();

private:
    struct Due
    {
        ToDo todo;
        QDateTime at;
    };

    void notify(const ToDo &todo);
    void notifyMany(const QList<ToDo> &todos);
    void onAction(uint id, const QString &key);
    void schedule(const QDateTime &next);

    QPointer<VaultController> m_vault;
    QPointer<greylock::NotificationServer> m_server;
    QPointer<Settings> m_settings;
    Now m_now = [] { return QDateTime::currentDateTime(); };
    QDateTime m_lastCheck;
    QList<Due> m_snoozed;
    /// What each notification shown is about; an empty path for the summary.
    QHash<uint, ToDo> m_shown;
    QTimer m_timer;
    QTimer m_soon;
};

} // namespace jot
