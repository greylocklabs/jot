#include "LiveQml.h"
#include "ReleaseInfo.h"
#include "SettingsForeign.h"
#include "ThemeController.h"
#include "ThemesController.h"
#include "ToDoReminders.h"
#include "VaultController.h"

#include "greylock/Autostart.h"
#include "greylock/Desktop.h"
#include "greylock/Identity.h"
#include "greylock/Notifications.h"
#include "greylock/SingleInstance.h"
#include "greylock/StartAtLogin.h"
#include "greylock/Tray.h"
#include "greylock/UpdateCheck.h"
#include "jot/LogFile.h"
#include "jot/Settings.h"

#include <QCommandLineParser>
#include <QDBusConnection>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTemporaryDir>
#include <QTextStream>
#include <QTimer>

using namespace Qt::StringLiterals;

int main(int argc, char *argv[])
{
    greylock::desktop::useDesktopFileDialogs();
    QGuiApplication app(argc, argv);
    // The identity's folder name keeps Qt's own config and data folders apart for a
    // development build, and matches the CLI's.
    app.setApplicationName(greylock::identity::dirName());
    app.setApplicationDisplayName(greylock::identity::displayName());
    app.setApplicationVersion(QStringLiteral(JOT_VERSION));
    // Lets Wayland associate the window with the .desktop entry.
    app.setDesktopFileName(greylock::identity::appId());

    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption themeOption(
        u"theme"_s, u"Built-in theme id or path to a theme file. Defaults to $JOT_THEME."_s,
        u"theme"_s, qEnvironmentVariable("JOT_THEME"));
    parser.addOption(themeOption);
    QCommandLineOption screenshotOption(
        u"screenshot"_s, u"Save the window to a PNG once it has rendered, then exit."_s, u"file"_s);
    parser.addOption(screenshotOption);
    QCommandLineOption sizeOption(u"size"_s, u"Window size in pixels, e.g. 1280x840."_s, u"size"_s);
    parser.addOption(sizeOption);
    QCommandLineOption vaultOption(u"vault"_s, u"Open this folder of notes, just this once."_s,
                                   u"folder"_s);
    parser.addOption(vaultOption);
    QCommandLineOption noteOption(
        u"note"_s, u"With --vault: show this note, by its path in the vault."_s, u"path"_s);
    parser.addOption(noteOption);
    QCommandLineOption tasksOption(u"tasks"_s, u"With --vault: show the tasks."_s);
    parser.addOption(tasksOption);
    QCommandLineOption backgroundOption(
        QString::fromLatin1(greylock::Autostart::kBackgroundOption),
        u"Start without showing the window, as when started at login."_s);
    parser.addOption(backgroundOption);
#ifdef JOT_LIVE_QML
    QCommandLineOption liveOption(
        u"live-qml"_s,
        u"Load QML from the source tree and reload it on save. Development builds only."_s);
    parser.addOption(liveOption);
#endif
    parser.process(app);
    QSize windowSize;
    if (parser.isSet(sizeOption)) {
        const QStringList parts = parser.value(sizeOption).split(u'x');
        windowSize = QSize(parts.value(0).toInt(), parts.value(1).toInt());
        if (parts.size() != 2 || windowSize.isEmpty()) {
            QTextStream(stderr) << "jot-gui: --size wants a size like 1280x840\n";
            return 2;
        }
    }
    jot::logfile::install(u"jot-gui"_s);

    // A vault given for one run, as for screenshots, runs beside the Jot already open; anything
    // else hands over to that one, so reminders never come twice.
    const bool standalone = parser.isSet(vaultOption);
    greylock::SingleInstance single(greylock::identity::appId(), QDBusConnection::sessionBus());
    if (!standalone && !single.claim())
        return 0;

    // A vault given for one run, as for screenshots, leaves the user's settings alone.
    QTemporaryDir scratch;
    jot::Settings settings(parser.isSet(vaultOption) ? scratch.filePath(u"settings.ini"_s)
                                                     : jot::Settings::defaultPath());
    jot::SettingsForeign::s_instance = &settings;
    jot::VaultController vault(settings);
    jot::VaultController::setInstance(&vault);

    QTextStream err(stderr);
    if (const QString theme = parser.value(themeOption); !theme.isEmpty()) {
        const QStringList errors = jot::ThemesController::instance()->applyForThisRun(theme);
        for (const QString &error : errors)
            err << "jot-gui: " << theme << ": " << error << "\n";
        if (!errors.isEmpty())
            return 1;
    } else {
        jot::ThemesController::instance()->restore();
    }
    for (const QString &warning : jot::ThemeController::instance()->warnings())
        err << "jot-gui: theme: " << warning << "\n";
    err.flush();

    if (parser.isSet(vaultOption)) {
        if (!vault.openPath(parser.value(vaultOption))) {
            err << "jot-gui: " << vault.error() << "\n";
            return 1;
        }
        vault.setCurrent(parser.value(noteOption));
        vault.setShowingTasks(parser.isSet(tasksOption));
    } else {
        vault.openLast();
    }

    // Closing the window can leave Jot running in the tray, so reminders still come.
    const QString logo = u":/jot/assets/logo.png"_s;
    greylock::FreedesktopNotifications notifications(logo);
    jot::ToDoReminders reminders;
    greylock::Autostart autostart(
        greylock::identity::appId(), QCoreApplication::applicationFilePath(),
        QCoreApplication::translate("main", "Keeps Jot running for to-do reminders"));
    if (!standalone) {
        reminders.setup(&vault, &notifications, &settings);
        app.setQuitOnLastWindowClosed(!settings.keepRunning());
        greylock::StartAtLogin::instance()->setup(&autostart);
        QObject::connect(&settings, &jot::Settings::remindersChanged, &app, [&] {
            app.setQuitOnLastWindowClosed(!settings.keepRunning());
            // Starting at login only makes sense while Jot keeps running.
            if (!settings.keepRunning())
                greylock::StartAtLogin::instance()->setEnabled(false);
        });
    }

    // Basic style: everything visual comes from the theme, not a platform style.
    QQuickStyle::setStyle(u"Basic"_s);
    greylock::UpdateCheck updates;
    QQmlApplicationEngine engine;
    // "Jot Devel" for a development build, so the two windows are told apart.
    // Started at login, Jot waits hidden until it is opened or a reminder is clicked; that only
    // makes sense while it keeps running.
    const bool hidden = parser.isSet(backgroundOption) && settings.keepRunning() && !standalone;
    engine.setInitialProperties(
        {{u"title"_s, greylock::identity::displayName()}, {u"visible"_s, !hidden}});
    bool live = false;
#ifdef JOT_LIVE_QML
    // A QML mistake while editing should wait for the fix, not end the session.
    jot::LiveQml liveQml(engine,
                         {{u"Jot.Ui"_s, QStringLiteral(JOT_SOURCE_DIR "/src/ui"),
                           QStringLiteral(JOT_BINARY_DIR "/Jot/Ui")}},
                         [&engine] { engine.loadFromModule("Jot.Ui", "Main"); });
    if (parser.isSet(liveOption)) {
        live = liveQml.start();
        if (!live)
            err << "jot-gui: live QML: " << liveQml.errorString() << "\n" << Qt::flush;
    }
#endif
    if (!live)
        QObject::connect(
            &engine, &QQmlApplicationEngine::objectCreationFailed, &app,
            [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.loadFromModule("Jot.Ui", "Main");

    // A vault given for one run, as for screenshots, never asks GitHub about releases.
    if (!parser.isSet(vaultOption)) {
        if (auto *release = engine.singletonInstance<jot::ReleaseInfo *>("Jot.Ui", "Release"))
            release->watchForUpdates(&settings, &updates);
    }

    auto *window = engine.rootObjects().isEmpty()
                       ? nullptr
                       : qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    // A second launch, a reminder or the tray brings the window back, and can ask it to do
    // something by its command's name.
    const auto showWindow = [&engine](const QString &command = {}) {
        if (engine.rootObjects().isEmpty())
            return;
        // Looked up each time: live QML replaces the window.
        QObject *root = engine.rootObjects().first();
        if (auto *shown = qobject_cast<QQuickWindow *>(root)) {
            shown->show();
            shown->raise();
            shown->requestActivate();
        }
        if (!command.isEmpty())
            QMetaObject::invokeMethod(root, "run", Q_ARG(QVariant, command));
    };
    QObject::connect(&single, &greylock::SingleInstance::activated, &engine, [&] { showWindow(); });
    QObject::connect(&reminders, &jot::ToDoReminders::openRequested, &engine,
                     [&](const QString &path, int line) {
                         vault.setShowingTasks(false);
                         vault.openAt(path, line);
                         showWindow();
                     });
    QObject::connect(&reminders, &jot::ToDoReminders::todayRequested, &engine, [&] {
        settings.setTasksView(u"today"_s);
        vault.setShowingTasks(true);
        showWindow();
    });

    // While Jot keeps running without its window, the tray shows it is there.
    greylock::TrayItem tray(QDBusConnection::sessionBus(), logo);
    if (!standalone) {
        tray.setMenu({{u"open"_s, QCoreApplication::translate("main", "Open Jot")},
                      {u"add"_s, QCoreApplication::translate("main", "Add a to-do")},
                      {},
                      {u"quit"_s, QCoreApplication::translate("main", "Quit Jot")}});
        tray.setVisible(settings.keepRunning());
        QObject::connect(&settings, &jot::Settings::remindersChanged, &tray,
                         [&] { tray.setVisible(settings.keepRunning()); });
        QObject::connect(&tray, &greylock::TrayItem::activated, &engine, [&] { showWindow(); });
        QObject::connect(&tray, &greylock::TrayItem::triggered, &engine, [&](const QString &key) {
            if (key == u"open")
                showWindow();
            else if (key == u"add")
                showWindow(u"quickAdd"_s);
            else if (key == u"quit")
                QCoreApplication::quit();
        });
    }

    if (windowSize.isValid() && window)
        window->resize(windowSize);

    if (const QString screenshot = parser.value(screenshotOption); !screenshot.isEmpty()) {
        if (!window)
            return 1;
        QTimer::singleShot(1000, window, [window, screenshot] {
            QCoreApplication::exit(window->grabWindow().save(screenshot) ? 0 : 1);
        });
    }
    return app.exec();
}
