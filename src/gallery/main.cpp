#include "ThemeController.h"

#include "greylock/LogFile.h"
#include "greylock/ui/LiveQml.h"

#include <QCommandLineParser>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTextStream>
#include <QTimer>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("jot-gallery"));

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("Renders a Jot theme's tokens and components. Edits to a theme file "
                       "appear live."));
    parser.addHelpOption();
    QCommandLineOption themeOption(QStringLiteral("theme"),
                                   QStringLiteral("Built-in theme id or path to a theme file."),
                                   QStringLiteral("theme"));
    parser.addOption(themeOption);
    QCommandLineOption screenshotOption(
        QStringLiteral("screenshot"),
        QStringLiteral("Save the rendered gallery to a PNG and exit."), QStringLiteral("file"));
    parser.addOption(screenshotOption);
#ifdef JOT_LIVE_QML
    QCommandLineOption liveOption(QStringLiteral("live-qml"),
                                  QStringLiteral("Load QML from the source tree and reload it on "
                                                 "save. Development builds only."));
    parser.addOption(liveOption);
#endif
    parser.process(app);
    greylock::logfile::install(QStringLiteral("jot-gallery"));

    if (const QString theme = parser.value(themeOption); !theme.isEmpty()) {
        const QStringList errors = jot::ThemeController::instance()->load(theme);
        QTextStream err(stderr);
        for (const QString &error : errors)
            err << "jot-gallery: " << theme << ": " << error << "\n";
        if (!errors.isEmpty())
            return 1;
    }

    QQuickStyle::setStyle(QStringLiteral("Basic"));
    QQmlApplicationEngine engine;
    bool live = false;
#ifdef JOT_LIVE_QML
    // A QML mistake while editing should wait for the fix, not end the session.
    greylock::LiveQml liveQml(
        engine,
        {{QStringLiteral("Jot.Ui"), QStringLiteral(JOT_SOURCE_DIR "/src/ui"),
          QStringLiteral(JOT_BINARY_DIR "/Jot/Ui")},
         {QStringLiteral("Jot.Gallery"), QStringLiteral(JOT_SOURCE_DIR "/src/gallery"),
          QStringLiteral(JOT_BINARY_DIR "/Jot/Gallery")}},
        [&engine] { engine.loadFromModule("Jot.Gallery", "Gallery"); });
    if (parser.isSet(liveOption)) {
        live = liveQml.start();
        if (!live)
            QTextStream(stderr) << "jot-gallery: live QML: " << liveQml.errorString() << "\n";
    }
#endif
    if (!live)
        QObject::connect(
            &engine, &QQmlApplicationEngine::objectCreationFailed, &app,
            [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.loadFromModule("Jot.Gallery", "Gallery");

    if (const QString file = parser.value(screenshotOption); !file.isEmpty()) {
        auto *window = engine.rootObjects().isEmpty()
                           ? nullptr
                           : qobject_cast<QQuickWindow *>(engine.rootObjects().first());
        if (!window)
            return 1;
        // Once layout has settled, grow the window to fit the whole page, then
        // give it a moment to render at that size before grabbing.
        QTimer::singleShot(500, window, [window, file] {
            if (auto *content = window->findChild<QObject *>(QStringLiteral("content")))
                window->setHeight(content->property("implicitHeight").toInt());
            QTimer::singleShot(500, window, [window, file] {
                QCoreApplication::exit(window->grabWindow().save(file) ? 0 : 1);
            });
        });
    }
    return app.exec();
}
