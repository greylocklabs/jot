#include "jot/Settings.h"

#include "greylock/Identity.h"

#include <QStandardPaths>
#include <QTime>

#include <algorithm>

using namespace Qt::StringLiterals;

namespace jot {

namespace {

// Never a key without a group: QSettings keeps those in [General], so a group of that
// name is written as [%General] and read back as another.
const QString kVaultPath = u"vault/path"_s;
const QString kTheme = u"appearance/theme"_s;
const QString kNoteSort = u"notes/sort"_s;
const QString kCollapsedNotebooks = u"notes/collapsedNotebooks"_s;
const QString kCollapsedTags = u"notes/collapsedTags"_s;
const QString kTasksView = u"tasks/view"_s;
const QString kCheckForUpdates = u"updates/check"_s;
const QString kLastSeenVersion = u"app/lastSeenVersion"_s;
const QString kKeepRunning = u"app/keepRunning"_s;
const QString kRemindersOn = u"reminders/on"_s;
const QString kReminderTime = u"reminders/time"_s;
const QString kLastReminderCheck = u"reminders/lastCheck"_s;
const QString kSourceMode = u"editor/sourceMode"_s;
const QString kReadMode = u"editor/readMode"_s;
const QString kNoteFont = u"editor/font"_s;
const QString kSidebarWidth = u"layout/sidebarWidth"_s;
const QString kNoteListWidth = u"layout/noteListWidth"_s;
const QString kViMode = u"keyboard/viMode"_s;
const QString kLeaderKey = u"keyboard/leaderKey"_s;
const QString kLeaderTimeout = u"keyboard/leaderTimeout"_s;

/// A saved width within its bounds; 0 stays 0, the theme's own.
int bounded(int width, int lowest, int highest)
{
    return width <= 0 ? 0 : std::clamp(width, lowest, highest);
}

constexpr int kMinSidebarWidth = 180;
constexpr int kMaxSidebarWidth = 480;
constexpr int kMaxNoteListWidth = 640;

} // namespace

Settings::Settings(const QString &path, QObject *parent)
    : QObject(parent), m_store(path, QSettings::IniFormat)
{
    m_vaultPath = m_store.value(kVaultPath).toString();
    m_theme = m_store.value(kTheme).toString();
    m_noteSort = m_store.value(kNoteSort).toString();
    if (!noteSorts().contains(m_noteSort))
        m_noteSort = noteSorts().first();
    m_collapsedNotebooks = m_store.value(kCollapsedNotebooks).toStringList();
    m_collapsedTags = m_store.value(kCollapsedTags).toStringList();
    m_tasksView = m_store.value(kTasksView, u"today"_s).toString();
    if (!toDoViews().contains(m_tasksView))
        m_tasksView = u"today"_s;
    m_checkForUpdates = m_store.value(kCheckForUpdates, true).toBool();
    m_lastSeenVersion = m_store.value(kLastSeenVersion).toString();
    m_keepRunning = m_store.value(kKeepRunning, true).toBool();
    m_remindersOn = m_store.value(kRemindersOn, true).toBool();
    if (const QTime time = QTime::fromString(m_store.value(kReminderTime).toString(), u"HH:mm"_s);
        time.isValid())
        m_reminderTime = time.toString(u"HH:mm"_s);
    m_lastReminderCheck = m_store.value(kLastReminderCheck).toDateTime();
    m_sourceMode = m_store.value(kSourceMode, false).toBool();
    m_readMode = m_store.value(kReadMode, false).toBool();
    m_noteFont = m_store.value(kNoteFont).toString();
    m_sidebarWidth =
        bounded(m_store.value(kSidebarWidth, 0).toInt(), kMinSidebarWidth, kMaxSidebarWidth);
    m_noteListWidth =
        bounded(m_store.value(kNoteListWidth, 0).toInt(), kMinNoteListWidth, kMaxNoteListWidth);
    if (!noteFonts().contains(m_noteFont))
        m_noteFont.clear();
    m_viMode = m_store.value(kViMode, false).toBool();
    const QString leader = m_store.value(kLeaderKey).toString();
    m_leaderKey = kLeaderKeys.contains(leader) ? leader : u","_s;
    m_leaderTimeout = std::max(0, m_store.value(kLeaderTimeout, 5000).toInt());
}

QString Settings::defaultPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + u'/' +
           greylock::identity::dirName() + u"/settings.ini"_s;
}

QStringList Settings::noteSorts()
{
    return {u"updated"_s, u"created"_s, u"title"_s};
}

void Settings::setVaultPath(const QString &path)
{
    if (m_vaultPath == path)
        return;
    m_vaultPath = path;
    m_store.setValue(kVaultPath, path);
    Q_EMIT vaultPathChanged();
}

void Settings::setTheme(const QString &idOrPath)
{
    if (m_theme == idOrPath)
        return;
    m_theme = idOrPath;
    m_store.setValue(kTheme, idOrPath);
    Q_EMIT themeChanged();
}

void Settings::setNoteSort(const QString &sort)
{
    if (m_noteSort == sort || !noteSorts().contains(sort))
        return;
    m_noteSort = sort;
    m_store.setValue(kNoteSort, sort);
    Q_EMIT noteSortChanged();
}

QStringList Settings::toDoViews()
{
    return {u"today"_s, u"upcoming"_s, u"anytime"_s, u"all"_s};
}

void Settings::setTasksView(const QString &view)
{
    if (m_tasksView == view || !toDoViews().contains(view))
        return;
    m_tasksView = view;
    m_store.setValue(kTasksView, view);
    Q_EMIT tasksViewChanged();
}

void Settings::setCollapsedTags(const QStringList &tags)
{
    if (m_collapsedTags == tags)
        return;
    m_collapsedTags = tags;
    m_store.setValue(kCollapsedTags, tags);
    Q_EMIT collapsedTagsChanged();
}

void Settings::setCollapsedNotebooks(const QStringList &paths)
{
    if (m_collapsedNotebooks == paths)
        return;
    m_collapsedNotebooks = paths;
    m_store.setValue(kCollapsedNotebooks, paths);
    Q_EMIT collapsedNotebooksChanged();
}

void Settings::setCheckForUpdates(bool check)
{
    if (m_checkForUpdates == check)
        return;
    m_checkForUpdates = check;
    m_store.setValue(kCheckForUpdates, check);
    Q_EMIT checkForUpdatesChanged();
}

void Settings::setLastSeenVersion(const QString &version)
{
    if (m_lastSeenVersion == version)
        return;
    m_lastSeenVersion = version;
    m_store.setValue(kLastSeenVersion, version);
    Q_EMIT lastSeenVersionChanged();
}

void Settings::setKeepRunning(bool keep)
{
    if (m_keepRunning == keep)
        return;
    m_keepRunning = keep;
    m_store.setValue(kKeepRunning, keep);
    Q_EMIT remindersChanged();
}

void Settings::setRemindersOn(bool on)
{
    if (m_remindersOn == on)
        return;
    m_remindersOn = on;
    m_store.setValue(kRemindersOn, on);
    Q_EMIT remindersChanged();
}

void Settings::setReminderTime(const QString &time)
{
    QTime read = QTime::fromString(time.trimmed(), u"H:mm"_s);
    if (!read.isValid())
        read = QTime::fromString(time.trimmed(), u"HH:mm"_s);
    if (!read.isValid() || read.toString(u"HH:mm"_s) == m_reminderTime)
        return;
    m_reminderTime = read.toString(u"HH:mm"_s);
    m_store.setValue(kReminderTime, m_reminderTime);
    Q_EMIT remindersChanged();
}

void Settings::setLastReminderCheck(const QDateTime &at)
{
    m_lastReminderCheck = at;
    m_store.setValue(kLastReminderCheck, at);
}

void Settings::setSidebarWidth(int width)
{
    // A drag past the edge stops at the narrowest; only a reset means the theme's width.
    width = std::clamp(width, kMinSidebarWidth, kMaxSidebarWidth);
    if (m_sidebarWidth == width)
        return;
    m_sidebarWidth = width;
    m_store.setValue(kSidebarWidth, width);
    Q_EMIT sidebarWidthChanged();
}

void Settings::setNoteListWidth(int width)
{
    width = std::clamp(width, kMinNoteListWidth, kMaxNoteListWidth);
    if (m_noteListWidth == width)
        return;
    m_noteListWidth = width;
    m_store.setValue(kNoteListWidth, width);
    Q_EMIT noteListWidthChanged();
}

void Settings::resetSidebarWidth()
{
    if (m_sidebarWidth == 0)
        return;
    m_sidebarWidth = 0;
    m_store.setValue(kSidebarWidth, 0);
    Q_EMIT sidebarWidthChanged();
}

void Settings::resetNoteListWidth()
{
    if (m_noteListWidth == 0)
        return;
    m_noteListWidth = 0;
    m_store.setValue(kNoteListWidth, 0);
    Q_EMIT noteListWidthChanged();
}

QStringList Settings::noteFonts()
{
    return {QString(), u"serif"_s, u"mono"_s};
}

void Settings::setNoteFont(const QString &font)
{
    if (m_noteFont == font || !noteFonts().contains(font))
        return;
    m_noteFont = font;
    m_store.setValue(kNoteFont, font);
    Q_EMIT noteFontChanged();
}

void Settings::setReadMode(bool read)
{
    if (m_readMode == read)
        return;
    m_readMode = read;
    m_store.setValue(kReadMode, read);
    Q_EMIT readModeChanged();
}

void Settings::setSourceMode(bool source)
{
    if (m_sourceMode == source)
        return;
    m_sourceMode = source;
    m_store.setValue(kSourceMode, source);
    Q_EMIT sourceModeChanged();
}

void Settings::setViMode(bool on)
{
    if (m_viMode == on)
        return;
    m_viMode = on;
    m_store.setValue(kViMode, on);
    Q_EMIT keyboardChanged();
}

void Settings::setLeaderKey(const QString &key)
{
    if (m_leaderKey == key || !kLeaderKeys.contains(key))
        return;
    m_leaderKey = key;
    m_store.setValue(kLeaderKey, key);
    Q_EMIT keyboardChanged();
}

void Settings::setLeaderTimeout(int ms)
{
    ms = std::max(0, ms);
    if (m_leaderTimeout == ms)
        return;
    m_leaderTimeout = ms;
    m_store.setValue(kLeaderTimeout, ms);
    Q_EMIT keyboardChanged();
}

void Settings::reset()
{
    setTheme({});
    setNoteSort(noteSorts().first());
    setCheckForUpdates(true);
    setSourceMode(false);
    setReadMode(false);
    setTasksView(u"today"_s);
    setKeepRunning(true);
    setRemindersOn(true);
    setReminderTime(u"09:00"_s);
    setNoteFont({});
    resetSidebarWidth();
    resetNoteListWidth();
    setViMode(false);
    setLeaderKey(u","_s);
    setLeaderTimeout(5000);
}

} // namespace jot
