#include "jot/Settings.h"

#include "greylock/Identity.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTime>
#include <QUuid>

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
const QString kTasksSort = u"tasks/sort"_s;
const QString kTasksSortDescending = u"tasks/sortDescending"_s;
const QStringList kTaskSorts = {u"due"_s, u"created"_s, u"title"_s, u"priority"_s};
const QString kCheckForUpdates = u"updates/check"_s;
const QString kLastSeenVersion = u"app/lastSeenVersion"_s;
const QString kKeepRunning = u"app/keepRunning"_s;
const QString kRemindersOn = u"reminders/on"_s;
const QString kQuickCaptureKey = u"app/quickCaptureKey"_s;
const QString kReminderTime = u"reminders/time"_s;
const QString kLastReminderCheck = u"reminders/lastCheck"_s;
const QString kSourceMode = u"editor/sourceMode"_s;
const QString kReadMode = u"editor/readMode"_s;
const QString kOutlineShown = u"editor/outline"_s;
const QString kColumnsHidden = u"layout/columnsHidden"_s;
const QString kFocusMode = u"editor/focusMode"_s;
const QString kTypewriter = u"editor/typewriter"_s;
const QString kNoteFont = u"editor/font"_s;
const QString kSidebarWidth = u"layout/sidebarWidth"_s;
const QString kNoteListWidth = u"layout/noteListWidth"_s;
const QString kViMode = u"keyboard/viMode"_s;
const QString kDeveloperMode = u"developer/mode"_s;
const QString kVerboseLogging = u"developer/verboseLogging"_s;
const QString kLeaderKey = u"keyboard/leaderKey"_s;
const QString kLeaderTimeout = u"keyboard/leaderTimeout"_s;
const QString kTrashDays = u"trash/days"_s;
const QString kSearchTrash = u"trash/search"_s;
const QString kAutosave = u"editor/autosave"_s;
const QString kSpellcheck = u"editor/spellcheck"_s;
const QString kDailyWordGoal = u"writing/dailyGoal"_s;
const QString kComputerId = u"app/computerId"_s;

// What names this computer's folders, sizes its window, or records what it last did goes in
// local.ini, so settings.ini can be copied to another computer as it is. Each vault's sync
// settings, under sync/, go there too.
const QStringList kLocalKeys{kVaultPath,       kCollapsedNotebooks, kCollapsedTags,     kTasksView,
                             kLastSeenVersion, kQuickCaptureKey,    kLastReminderCheck, kSourceMode,
                             kReadMode,        kOutlineShown,       kColumnsHidden,     kFocusMode,
                             kSidebarWidth,    kNoteListWidth,      kComputerId};

bool isLocal(const QString &key)
{
    return kLocalKeys.contains(key) || key.startsWith(u"sync/"_s);
}

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
    : QObject(parent), m_store(path, QSettings::IniFormat),
      m_local(QFileInfo(path).dir().filePath(u"local.ini"_s), QSettings::IniFormat)
{
    m_vaultPath = get(kVaultPath).toString();
    // A theme file in Jot's own folder is kept relative to it, so it travels with this file.
    const QString theme = get(kTheme).toString();
    m_theme = theme.endsWith(u".toml"_s) && QDir::isRelativePath(theme)
                  ? QDir::cleanPath(QFileInfo(m_store.fileName()).dir().filePath(theme))
                  : theme;
    m_noteSort = get(kNoteSort).toString();
    if (!noteSorts().contains(m_noteSort))
        m_noteSort = noteSorts().first();
    m_collapsedNotebooks = get(kCollapsedNotebooks).toStringList();
    m_collapsedTags = get(kCollapsedTags).toStringList();
    m_tasksView = get(kTasksView, u"today"_s).toString();
    if (!toDoViews().contains(m_tasksView))
        m_tasksView = u"today"_s;
    m_tasksSort = get(kTasksSort, u"due"_s).toString();
    if (!kTaskSorts.contains(m_tasksSort))
        m_tasksSort = u"due"_s;
    m_tasksSortDescending = get(kTasksSortDescending, false).toBool();
    m_checkForUpdates = get(kCheckForUpdates, true).toBool();
    m_lastSeenVersion = get(kLastSeenVersion).toString();
    m_keepRunning = get(kKeepRunning, true).toBool();
    m_remindersOn = get(kRemindersOn, true).toBool();
    m_quickCaptureKey = get(kQuickCaptureKey, false).toBool();
    if (const QTime time = QTime::fromString(get(kReminderTime).toString(), u"HH:mm"_s);
        time.isValid())
        m_reminderTime = time.toString(u"HH:mm"_s);
    m_lastReminderCheck = get(kLastReminderCheck).toDateTime();
    m_sourceMode = get(kSourceMode, false).toBool();
    m_readMode = get(kReadMode, false).toBool();
    m_outlineShown = get(kOutlineShown, false).toBool();
    m_columnsHidden = get(kColumnsHidden, false).toBool();
    m_focusMode = get(kFocusMode, false).toBool();
    m_typewriter = get(kTypewriter, false).toBool();
    m_noteFont = get(kNoteFont).toString();
    m_sidebarWidth = bounded(get(kSidebarWidth, 0).toInt(), kMinSidebarWidth, kMaxSidebarWidth);
    m_noteListWidth = bounded(get(kNoteListWidth, 0).toInt(), kMinNoteListWidth, kMaxNoteListWidth);
    if (!noteFonts().contains(m_noteFont))
        m_noteFont.clear();
    m_viMode = get(kViMode, false).toBool();
    m_developerMode = get(kDeveloperMode, false).toBool();
    m_verboseLogging = get(kVerboseLogging, false).toBool();
    const QString leader = get(kLeaderKey).toString();
    m_leaderKey = kLeaderKeys.contains(leader) ? leader : u","_s;
    m_leaderTimeout = std::max(0, get(kLeaderTimeout, 5000).toInt());
    m_trashDays = std::max(0, get(kTrashDays, 0).toInt());
    m_searchTrash = get(kSearchTrash, true).toBool();
    m_autosave = get(kAutosave, true).toBool();
    m_spellcheck = get(kSpellcheck, true).toBool();
    m_dailyWordGoal = std::max(0, get(kDailyWordGoal, 0).toInt());
}

QSettings &Settings::storeFor(const QString &key)
{
    return isLocal(key) ? m_local : m_store;
}

const QSettings &Settings::storeFor(const QString &key) const
{
    return isLocal(key) ? m_local : m_store;
}

void Settings::put(const QString &key, const QVariant &value)
{
    storeFor(key).setValue(key, value);
}

QVariant Settings::get(const QString &key, const QVariant &fallback) const
{
    return storeFor(key).value(key, fallback);
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
    put(kVaultPath, path);
    Q_EMIT vaultPathChanged();
}

void Settings::setTheme(const QString &idOrPath)
{
    if (m_theme == idOrPath)
        return;
    m_theme = idOrPath;
    const QString relative = QFileInfo(m_store.fileName()).dir().relativeFilePath(idOrPath);
    put(kTheme,
        QDir::isAbsolutePath(idOrPath) && !relative.startsWith(u"../"_s) ? relative : idOrPath);
    Q_EMIT themeChanged();
}

void Settings::setNoteSort(const QString &sort)
{
    if (m_noteSort == sort || !noteSorts().contains(sort))
        return;
    m_noteSort = sort;
    put(kNoteSort, sort);
    Q_EMIT noteSortChanged();
}

QStringList Settings::toDoViews()
{
    return {u"today"_s, u"upcoming"_s, u"anytime"_s, u"all"_s, u"done"_s};
}

void Settings::setTasksSort(const QString &sort)
{
    if (m_tasksSort == sort || !kTaskSorts.contains(sort))
        return;
    m_tasksSort = sort;
    put(kTasksSort, sort);
    Q_EMIT tasksSortChanged();
}

void Settings::setTasksSortDescending(bool descending)
{
    if (m_tasksSortDescending == descending)
        return;
    m_tasksSortDescending = descending;
    put(kTasksSortDescending, descending);
    Q_EMIT tasksSortChanged();
}

void Settings::setTasksView(const QString &view)
{
    if (m_tasksView == view || !toDoViews().contains(view))
        return;
    m_tasksView = view;
    put(kTasksView, view);
    Q_EMIT tasksViewChanged();
}

void Settings::setCollapsedTags(const QStringList &tags)
{
    if (m_collapsedTags == tags)
        return;
    m_collapsedTags = tags;
    put(kCollapsedTags, tags);
    Q_EMIT collapsedTagsChanged();
}

void Settings::setCollapsedNotebooks(const QStringList &paths)
{
    if (m_collapsedNotebooks == paths)
        return;
    m_collapsedNotebooks = paths;
    put(kCollapsedNotebooks, paths);
    Q_EMIT collapsedNotebooksChanged();
}

void Settings::setCheckForUpdates(bool check)
{
    if (m_checkForUpdates == check)
        return;
    m_checkForUpdates = check;
    put(kCheckForUpdates, check);
    Q_EMIT checkForUpdatesChanged();
}

void Settings::setLastSeenVersion(const QString &version)
{
    if (m_lastSeenVersion == version)
        return;
    m_lastSeenVersion = version;
    put(kLastSeenVersion, version);
    Q_EMIT lastSeenVersionChanged();
}

namespace {

/// The settings group for a vault: its path hashed, as paths cannot be keys.
QString syncGroup(const QString &vaultRoot)
{
    return u"sync/"_s +
           QString::fromLatin1(QCryptographicHash::hash(QDir(vaultRoot).absolutePath().toUtf8(),
                                                        QCryptographicHash::Sha256)
                                   .toHex()
                                   .left(16));
}

} // namespace

QString Settings::syncRemote(const QString &vaultRoot) const
{
    return get(syncGroup(vaultRoot) + u"/remote"_s).toString();
}

int Settings::syncRemoteUses(const QString &remote) const
{
    int uses = 0;
    for (const QString &key : m_local.allKeys()) {
        if (key.startsWith(u"sync/"_s) && key.endsWith(u"/remote"_s) &&
            get(key).toString() == remote)
            ++uses;
    }
    return uses;
}

void Settings::setSyncRemote(const QString &vaultRoot, const QString &remote)
{
    if (syncRemote(vaultRoot) == remote)
        return;
    const QString group = syncGroup(vaultRoot);
    if (remote.isEmpty()) {
        m_local.remove(group);
    } else {
        // The vault's own path too, for whoever reads the file.
        put(group + u"/vault"_s, QDir(vaultRoot).absolutePath());
        put(group + u"/remote"_s, remote);
    }
    Q_EMIT syncRemoteChanged();
}

void Settings::setKeepRunning(bool keep)
{
    if (m_keepRunning == keep)
        return;
    m_keepRunning = keep;
    put(kKeepRunning, keep);
    Q_EMIT remindersChanged();
}

void Settings::setRemindersOn(bool on)
{
    if (m_remindersOn == on)
        return;
    m_remindersOn = on;
    put(kRemindersOn, on);
    Q_EMIT remindersChanged();
}

void Settings::setQuickCaptureKey(bool on)
{
    if (m_quickCaptureKey == on)
        return;
    m_quickCaptureKey = on;
    put(kQuickCaptureKey, on);
    Q_EMIT quickCaptureKeyChanged();
}

void Settings::setReminderTime(const QString &time)
{
    QTime read = QTime::fromString(time.trimmed(), u"H:mm"_s);
    if (!read.isValid())
        read = QTime::fromString(time.trimmed(), u"HH:mm"_s);
    if (!read.isValid() || read.toString(u"HH:mm"_s) == m_reminderTime)
        return;
    m_reminderTime = read.toString(u"HH:mm"_s);
    put(kReminderTime, m_reminderTime);
    Q_EMIT remindersChanged();
}

void Settings::setLastReminderCheck(const QDateTime &at)
{
    m_lastReminderCheck = at;
    put(kLastReminderCheck, at);
}

void Settings::setSidebarWidth(int width)
{
    // A drag past the edge stops at the narrowest; only a reset means the theme's width.
    width = std::clamp(width, kMinSidebarWidth, kMaxSidebarWidth);
    if (m_sidebarWidth == width)
        return;
    m_sidebarWidth = width;
    put(kSidebarWidth, width);
    Q_EMIT sidebarWidthChanged();
}

void Settings::setNoteListWidth(int width)
{
    width = std::clamp(width, kMinNoteListWidth, kMaxNoteListWidth);
    if (m_noteListWidth == width)
        return;
    m_noteListWidth = width;
    put(kNoteListWidth, width);
    Q_EMIT noteListWidthChanged();
}

void Settings::resetSidebarWidth()
{
    if (m_sidebarWidth == 0)
        return;
    m_sidebarWidth = 0;
    put(kSidebarWidth, 0);
    Q_EMIT sidebarWidthChanged();
}

void Settings::resetNoteListWidth()
{
    if (m_noteListWidth == 0)
        return;
    m_noteListWidth = 0;
    put(kNoteListWidth, 0);
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
    put(kNoteFont, font);
    Q_EMIT noteFontChanged();
}

void Settings::setColumnsHidden(bool hidden)
{
    if (m_columnsHidden == hidden)
        return;
    m_columnsHidden = hidden;
    put(kColumnsHidden, hidden);
    Q_EMIT columnsHiddenChanged();
}

void Settings::setOutlineShown(bool shown)
{
    if (m_outlineShown == shown)
        return;
    m_outlineShown = shown;
    put(kOutlineShown, shown);
    Q_EMIT outlineShownChanged();
}

void Settings::setFocusMode(bool on)
{
    if (m_focusMode == on)
        return;
    m_focusMode = on;
    put(kFocusMode, on);
    Q_EMIT focusChanged();
}

void Settings::setTypewriter(bool on)
{
    if (m_typewriter == on)
        return;
    m_typewriter = on;
    put(kTypewriter, on);
    Q_EMIT focusChanged();
}

void Settings::setReadMode(bool read)
{
    if (m_readMode == read)
        return;
    m_readMode = read;
    put(kReadMode, read);
    Q_EMIT readModeChanged();
}

void Settings::setSourceMode(bool source)
{
    if (m_sourceMode == source)
        return;
    m_sourceMode = source;
    put(kSourceMode, source);
    Q_EMIT sourceModeChanged();
}

void Settings::setDeveloperMode(bool on)
{
    if (m_developerMode == on)
        return;
    m_developerMode = on;
    put(kDeveloperMode, on);
    Q_EMIT developerChanged();
}

void Settings::setVerboseLogging(bool on)
{
    if (m_verboseLogging == on)
        return;
    m_verboseLogging = on;
    put(kVerboseLogging, on);
    Q_EMIT developerChanged();
}

void Settings::setViMode(bool on)
{
    if (m_viMode == on)
        return;
    m_viMode = on;
    put(kViMode, on);
    Q_EMIT keyboardChanged();
}

void Settings::setLeaderKey(const QString &key)
{
    if (m_leaderKey == key || !kLeaderKeys.contains(key))
        return;
    m_leaderKey = key;
    put(kLeaderKey, key);
    Q_EMIT keyboardChanged();
}

void Settings::setLeaderTimeout(int ms)
{
    ms = std::max(0, ms);
    if (m_leaderTimeout == ms)
        return;
    m_leaderTimeout = ms;
    put(kLeaderTimeout, ms);
    Q_EMIT keyboardChanged();
}

void Settings::setTrashDays(int days)
{
    days = std::max(0, days);
    if (m_trashDays == days)
        return;
    m_trashDays = days;
    put(kTrashDays, days);
    Q_EMIT trashChanged();
}

void Settings::setSearchTrash(bool on)
{
    if (m_searchTrash == on)
        return;
    m_searchTrash = on;
    put(kSearchTrash, on);
    Q_EMIT trashChanged();
}

void Settings::setAutosave(bool on)
{
    if (m_autosave == on)
        return;
    m_autosave = on;
    put(kAutosave, on);
    Q_EMIT autosaveChanged();
}

QString Settings::computerId()
{
    QString id = get(kComputerId).toString();
    if (id.isEmpty()) {
        id = QUuid::createUuid().toString(QUuid::Id128).left(8);
        put(kComputerId, id);
    }
    return id;
}

void Settings::setSpellcheck(bool on)
{
    if (m_spellcheck == on)
        return;
    m_spellcheck = on;
    put(kSpellcheck, on);
    Q_EMIT spellcheckChanged();
}

void Settings::setDailyWordGoal(int words)
{
    words = std::max(0, words);
    if (m_dailyWordGoal == words)
        return;
    m_dailyWordGoal = words;
    put(kDailyWordGoal, words);
    Q_EMIT writingChanged();
}

void Settings::reset()
{
    setTheme({});
    setNoteSort(noteSorts().first());
    setCheckForUpdates(true);
    setSourceMode(false);
    setReadMode(false);
    setOutlineShown(false);
    setColumnsHidden(false);
    setFocusMode(false);
    setTypewriter(false);
    setTasksView(u"today"_s);
    setTasksSort(u"due"_s);
    setTasksSortDescending(false);
    setKeepRunning(true);
    setRemindersOn(true);
    setQuickCaptureKey(false);
    setReminderTime(u"09:00"_s);
    setNoteFont({});
    resetSidebarWidth();
    resetNoteListWidth();
    setViMode(false);
    setLeaderKey(u","_s);
    setLeaderTimeout(5000);
    setTrashDays(0);
    setSearchTrash(true);
    setAutosave(true);
    setSpellcheck(true);
    setDailyWordGoal(0);
}

} // namespace jot
