#include "jot/Settings.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace jot;
using namespace Qt::StringLiterals;

class TestSettings : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void startsWithDefaults();
    void savesAtOnceAndReadsBack();
    void signalsOnlyOnAChange();
    void refusesAnUnknownSort();
    void resetKeepsTheVault();
    void refusesAnUnknownNoteFont();
    void keepsColumnWidthsInBounds();
    void keepsKeyboardChoices();
    void keepsTrashChoices();
    void keepsSearchingTheTrash();
    void keepsTheSavingChoice();
    void keepsTheOutlineShown();
    void keepsTheDailyWordGoal();
    void keepsFocusAndTypewriterModes();
    void keepsDeveloperChoices();
    void keepsTheTasksView();
    void keepsReminderChoices();
    void countsTheVaultsOnARemote();
    void machineSettingsStayOutOfSettingsIni();
    void themesInTheFolderTravelWithIt();
    void keepsChosenShortcutsWithThePreferences();
};

void TestSettings::startsWithDefaults()
{
    QTemporaryDir dir;
    const Settings settings(dir.filePath(u"settings.ini"_s));
    QCOMPARE(settings.vaultPath(), QString());
    QCOMPARE(settings.theme(), QString());
    QCOMPARE(settings.noteSort(), u"updated"_s);
    QVERIFY(settings.collapsedNotebooks().isEmpty());
    QVERIFY(settings.checkForUpdates());
}

void TestSettings::savesAtOnceAndReadsBack()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(u"settings.ini"_s);
    {
        Settings settings(path);
        settings.setVaultPath(u"/home/me/Notes"_s);
        settings.setTheme(u"paper"_s);
        settings.setNoteSort(u"title"_s);
        settings.setCollapsedNotebooks({u"Ashfall"_s, u"Ashfall/Cast"_s});
        settings.setCheckForUpdates(false);
    }
    const Settings again(path);
    QCOMPARE(again.vaultPath(), u"/home/me/Notes"_s);
    QCOMPARE(again.theme(), u"paper"_s);
    QCOMPARE(again.noteSort(), u"title"_s);
    QCOMPARE(again.collapsedNotebooks(), (QStringList{u"Ashfall"_s, u"Ashfall/Cast"_s}));
    QVERIFY(!again.checkForUpdates());
}

void TestSettings::signalsOnlyOnAChange()
{
    QTemporaryDir dir;
    Settings settings(dir.filePath(u"settings.ini"_s));
    QSignalSpy changed(&settings, &Settings::vaultPathChanged);
    settings.setVaultPath(u"/a"_s);
    settings.setVaultPath(u"/a"_s);
    QCOMPARE(changed.size(), 1);
}

void TestSettings::refusesAnUnknownSort()
{
    QTemporaryDir dir;
    Settings settings(dir.filePath(u"settings.ini"_s));
    settings.setNoteSort(u"sideways"_s);
    QCOMPARE(settings.noteSort(), u"updated"_s);
}

void TestSettings::resetKeepsTheVault()
{
    QTemporaryDir dir;
    Settings settings(dir.filePath(u"settings.ini"_s));
    settings.setVaultPath(u"/home/me/Notes"_s);
    settings.setTheme(u"paper"_s);
    settings.setNoteSort(u"title"_s);
    settings.setCheckForUpdates(false);
    settings.setSourceMode(true);
    settings.setNoteFont(u"serif"_s);
    settings.setCollapsedNotebooks({u"Work"_s});
    QSignalSpy themeChanged(&settings, &Settings::themeChanged);

    settings.reset();

    QCOMPARE(themeChanged.count(), 1);
    QCOMPARE(settings.theme(), QString());
    QCOMPARE(settings.noteSort(), u"updated"_s);
    QVERIFY(settings.checkForUpdates());
    QVERIFY(!settings.sourceMode());
    QCOMPARE(settings.noteFont(), QString());
    QCOMPARE(settings.vaultPath(), u"/home/me/Notes"_s);
    QCOMPARE(settings.collapsedNotebooks(), QStringList{u"Work"_s});
    QCOMPARE(Settings(dir.filePath(u"settings.ini"_s)).noteSort(), u"updated"_s);
}

void TestSettings::refusesAnUnknownNoteFont()
{
    QTemporaryDir dir;
    Settings settings(dir.filePath(u"settings.ini"_s));
    settings.setNoteFont(u"mono"_s);
    settings.setNoteFont(u"comic"_s);
    QCOMPARE(settings.noteFont(), u"mono"_s);
    QCOMPARE(Settings(dir.filePath(u"settings.ini"_s)).noteFont(), u"mono"_s);
}

void TestSettings::keepsColumnWidthsInBounds()
{
    QTemporaryDir dir;
    Settings settings(dir.filePath(u"settings.ini"_s));
    QCOMPARE(settings.sidebarWidth(), 0);
    settings.setSidebarWidth(300);
    QCOMPARE(settings.sidebarWidth(), 300);
    settings.setSidebarWidth(20);
    QCOMPARE(settings.sidebarWidth(), 180);
    settings.setNoteListWidth(5000);
    QCOMPARE(settings.noteListWidth(), 640);
    QCOMPARE(Settings(dir.filePath(u"settings.ini"_s)).noteListWidth(), 640);
    // A drag past the edge stops at the narrowest rather than meaning the theme's width.
    settings.setSidebarWidth(0);
    QCOMPARE(settings.sidebarWidth(), 180);
    settings.setNoteListWidth(-40);
    QCOMPARE(settings.noteListWidth(), Settings::kMinNoteListWidth);
    settings.resetSidebarWidth();
    QCOMPARE(settings.sidebarWidth(), 0);
    QCOMPARE(Settings(dir.filePath(u"settings.ini"_s)).sidebarWidth(), 0);
    // 0 is the theme's own width, which reset goes back to.
    settings.reset();
    QCOMPARE(settings.sidebarWidth(), 0);
    QCOMPARE(settings.noteListWidth(), 0);
}

void TestSettings::keepsKeyboardChoices()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(u"settings.ini"_s);
    Settings settings(path);
    QVERIFY(!settings.viMode());
    QCOMPARE(settings.leaderKey(), u","_s);
    QCOMPARE(settings.leaderTimeout(), 5000);
    settings.setViMode(true);
    settings.setLeaderKey(u" "_s);
    settings.setLeaderKey(u"x"_s);
    settings.setLeaderTimeout(-10);
    const Settings again(path);
    QVERIFY(again.viMode());
    QCOMPARE(again.leaderKey(), u" "_s);
    QCOMPARE(again.leaderTimeout(), 0);
    settings.reset();
    QVERIFY(!settings.viMode());
    QCOMPARE(settings.leaderKey(), u","_s);
    QCOMPARE(settings.leaderTimeout(), 5000);
}

void TestSettings::keepsTrashChoices()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(u"settings.ini"_s);
    Settings settings(path);
    QCOMPARE(settings.trashDays(), 0);
    QSignalSpy changed(&settings, &Settings::trashChanged);
    settings.setTrashDays(30);
    settings.setTrashDays(30);
    QCOMPARE(changed.size(), 1);
    QCOMPARE(Settings(path).trashDays(), 30);
    settings.setTrashDays(-5);
    QCOMPARE(settings.trashDays(), 0);
    settings.setTrashDays(30);
    settings.reset();
    QCOMPARE(settings.trashDays(), 0);
}

void TestSettings::keepsSearchingTheTrash()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(u"settings.ini"_s);
    Settings settings(path);
    QVERIFY(settings.searchTrash());
    QSignalSpy changed(&settings, &Settings::trashChanged);
    settings.setSearchTrash(false);
    settings.setSearchTrash(false);
    QCOMPARE(changed.size(), 1);
    QVERIFY(!Settings(path).searchTrash());
    settings.reset();
    QVERIFY(settings.searchTrash());
}

void TestSettings::keepsTheSavingChoice()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(u"settings.ini"_s);
    Settings settings(path);
    QVERIFY(settings.autosave());
    QSignalSpy changed(&settings, &Settings::autosaveChanged);
    settings.setAutosave(false);
    settings.setAutosave(false);
    QCOMPARE(changed.size(), 1);
    QVERIFY(!Settings(path).autosave());
    settings.reset();
    QVERIFY(settings.autosave());
}

void TestSettings::keepsTheOutlineShown()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(u"settings.ini"_s);
    Settings settings(path);
    QVERIFY(!settings.outlineShown());
    settings.setOutlineShown(true);
    QVERIFY(Settings(path).outlineShown());
    settings.reset();
    QVERIFY(!settings.outlineShown());
}

void TestSettings::keepsTheDailyWordGoal()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(u"settings.ini"_s);
    Settings settings(path);
    QCOMPARE(settings.dailyWordGoal(), 0);
    settings.setDailyWordGoal(750);
    QCOMPARE(Settings(path).dailyWordGoal(), 750);
    settings.setDailyWordGoal(-5);
    QCOMPARE(settings.dailyWordGoal(), 0);
    settings.setDailyWordGoal(500);
    settings.reset();
    QCOMPARE(settings.dailyWordGoal(), 0);

    // The computer's id is made once and kept.
    const QString id = settings.computerId();
    QCOMPARE(id.size(), 8);
    QCOMPARE(Settings(path).computerId(), id);
}

void TestSettings::keepsFocusAndTypewriterModes()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(u"settings.ini"_s);
    Settings settings(path);
    QSignalSpy changed(&settings, &Settings::focusChanged);
    settings.setFocusMode(true);
    settings.setTypewriter(true);
    QCOMPARE(changed.size(), 2);
    QVERIFY(Settings(path).focusMode());
    QVERIFY(Settings(path).typewriter());
    settings.reset();
    QVERIFY(!settings.focusMode());
    QVERIFY(!settings.typewriter());
}

void TestSettings::keepsDeveloperChoices()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(u"settings.ini"_s);
    Settings settings(path);
    QVERIFY(!settings.developerMode());
    QVERIFY(!settings.verboseLogging());
    QSignalSpy changed(&settings, &Settings::developerChanged);
    settings.setDeveloperMode(true);
    settings.setVerboseLogging(true);
    settings.setVerboseLogging(true);
    QCOMPARE(changed.count(), 2);
    const Settings again(path);
    QVERIFY(again.developerMode());
    QVERIFY(again.verboseLogging());
    // Not a preference to reset: they are turned off as they were turned on, by hand.
    settings.reset();
    QVERIFY(settings.developerMode());
}

void TestSettings::keepsTheTasksView()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(u"settings.ini"_s);
    {
        Settings settings(path);
        QCOMPARE(settings.tasksView(), u"today"_s);
        QSignalSpy changed(&settings, &Settings::tasksViewChanged);
        settings.setTasksView(u"upcoming"_s);
        settings.setTasksView(u"someday"_s);
        QCOMPARE(settings.tasksView(), u"upcoming"_s);
        QCOMPARE(changed.count(), 1);
    }
    QCOMPARE(Settings(path).tasksView(), u"upcoming"_s);
    // A view this Jot does not know, as from a newer one, falls back to Today.
    QSettings(QFileInfo(path).dir().filePath(u"local.ini"_s), QSettings::IniFormat)
        .setValue(u"tasks/view"_s, u"someday"_s);
    Settings settings(path);
    QCOMPARE(settings.tasksView(), u"today"_s);
    settings.setTasksView(u"anytime"_s);
    settings.reset();
    QCOMPARE(settings.tasksView(), u"today"_s);

    // And how they are ordered, by due date soonest first unless chosen otherwise.
    QCOMPARE(settings.tasksSort(), u"due"_s);
    QVERIFY(!settings.tasksSortDescending());
    settings.setTasksSort(u"priority"_s);
    settings.setTasksSort(u"mood"_s);
    settings.setTasksSortDescending(true);
    QCOMPARE(Settings(path).tasksSort(), u"priority"_s);
    QVERIFY(Settings(path).tasksSortDescending());
    settings.reset();
    QCOMPARE(settings.tasksSort(), u"due"_s);
    QVERIFY(!settings.tasksSortDescending());
}

void TestSettings::keepsReminderChoices()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(u"settings.ini"_s);
    {
        Settings settings(path);
        QVERIFY(settings.keepRunning());
        QVERIFY(settings.remindersOn());
        QVERIFY(!settings.quickCaptureKey());
        QCOMPARE(settings.reminderTime(), u"09:00"_s);
        QVERIFY(!settings.lastReminderCheck().isValid());
        settings.setKeepRunning(false);
        settings.setRemindersOn(false);
        settings.setQuickCaptureKey(true);
        settings.setReminderTime(u"7:30"_s);
        settings.setReminderTime(u"25:00"_s);
        settings.setLastReminderCheck(QDateTime(QDate(2026, 10, 28), QTime(8, 0)));
    }
    Settings settings(path);
    QVERIFY(!settings.keepRunning());
    QVERIFY(!settings.remindersOn());
    QVERIFY(settings.quickCaptureKey());
    QCOMPARE(settings.reminderTime(), u"07:30"_s);
    QCOMPARE(settings.lastReminderCheck(), QDateTime(QDate(2026, 10, 28), QTime(8, 0)));
    settings.reset();
    QVERIFY(settings.keepRunning());
    QVERIFY(!settings.quickCaptureKey());
    QCOMPARE(settings.reminderTime(), u"09:00"_s);
}

void TestSettings::countsTheVaultsOnARemote()
{
    QTemporaryDir dir;
    Settings settings(dir.filePath(u"settings.ini"_s));
    QCOMPARE(settings.syncRemoteUses(u"s3:bucket"_s), 0);
    settings.setSyncRemote(u"/home/mara/Notes"_s, u"s3:bucket"_s);
    settings.setSyncRemote(u"/home/mara/Work"_s, u"s3:bucket"_s);
    settings.setSyncRemote(u"/home/mara/Other"_s, u"folder:/mnt"_s);
    QCOMPARE(settings.syncRemoteUses(u"s3:bucket"_s), 2);
    settings.setSyncRemote(u"/home/mara/Work"_s, {});
    QCOMPARE(settings.syncRemoteUses(u"s3:bucket"_s), 1);
}

void TestSettings::machineSettingsStayOutOfSettingsIni()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(u"settings.ini"_s);
    {
        Settings settings(path);
        settings.setViMode(true);
        settings.setTheme(u"paper"_s);
        settings.setVaultPath(u"/home/mara/Notes"_s);
        settings.setSidebarWidth(300);
        settings.setLastSeenVersion(u"0.5.8"_s);
        settings.setCollapsedNotebooks({u"Ashfall"_s});
        settings.setSyncRemote(u"/home/mara/Notes"_s, u"s3:bucket"_s);
    }
    QFile portable(path);
    QVERIFY(portable.open(QIODevice::ReadOnly));
    const QByteArray shared = portable.readAll();
    QVERIFY(shared.contains("viMode=true"));
    QVERIFY(shared.contains("theme=paper"));
    QVERIFY2(!shared.contains("/home/mara") && !shared.contains("0.5.8") &&
                 !shared.contains("300") && !shared.contains("Ashfall") && !shared.contains("s3:"),
             shared.constData());

    const QString local = QFileInfo(path).dir().filePath(u"local.ini"_s);
    QFile machine(local);
    QVERIFY(machine.open(QIODevice::ReadOnly));
    QVERIFY(machine.readAll().contains("s3:bucket"));
    machine.close();

    // Copied without local.ini, settings.ini gives back only its own part.
    QVERIFY(QFile::remove(local));
    const Settings copied(path);
    QVERIFY(copied.viMode());
    QCOMPARE(copied.theme(), u"paper"_s);
    QVERIFY(copied.vaultPath().isEmpty());
    QCOMPARE(copied.sidebarWidth(), 0);
    QVERIFY(copied.lastSeenVersion().isEmpty());
    QVERIFY(copied.syncRemote(u"/home/mara/Notes"_s).isEmpty());
}

void TestSettings::themesInTheFolderTravelWithIt()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(u"settings.ini"_s);
    const QString mine = dir.filePath(u"themes/mine.toml"_s);
    Settings(path).setTheme(mine);
    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly));
    QVERIFY(file.readAll().contains("theme=themes/mine.toml"));
    file.close();
    QCOMPARE(Settings(path).theme(), mine);

    // A theme elsewhere keeps its whole path.
    Settings(path).setTheme(u"/somewhere/else.toml"_s);
    QCOMPARE(Settings(path).theme(), u"/somewhere/else.toml"_s);
}

void TestSettings::keepsChosenShortcutsWithThePreferences()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(u"settings.ini"_s);
    {
        Settings settings(path);
        QSignalSpy changed(&settings, &Settings::keysChanged);
        settings.setKeys(u"goTo"_s, u"Ctrl+Shift+G"_s);
        settings.setKeys(u"goTo"_s, u"Ctrl+Shift+G"_s);
        settings.setKeys(u"settings"_s, u"Ctrl+,; F12"_s);
        settings.setKeys(u"syncNow"_s, QString());
        QCOMPARE(changed.count(), 3);
    }
    // In settings.ini, to go to another computer with the rest, and readable by hand.
    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly));
    const QByteArray written = file.readAll();
    QVERIFY2(written.contains("[keys]") && written.contains("goTo=Ctrl+Shift+G"),
             written.constData());
    file.close();

    Settings settings(path);
    QCOMPARE(settings.keys().value(u"goTo"_s).toString(), u"Ctrl+Shift+G"_s);
    QCOMPARE(settings.keys().value(u"settings"_s).toString(), u"Ctrl+,; F12"_s);
    // None at all is kept as none, not as the built-in keys.
    QVERIFY(settings.keys().contains(u"syncNow"_s));
    QVERIFY(settings.keys().value(u"syncNow"_s).toString().isEmpty());
    settings.resetKeys(u"goTo"_s);
    QVERIFY(!settings.keys().contains(u"goTo"_s));
    QVERIFY(!Settings(path).keys().contains(u"goTo"_s));
    settings.reset();
    QVERIFY(settings.keys().isEmpty());
    QVERIFY(Settings(path).keys().isEmpty());
}

QTEST_GUILESS_MAIN(TestSettings)
#include "tst_settings.moc"
