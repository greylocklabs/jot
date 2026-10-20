#include "ThemeController.h"

#include "greylock/Color.h"
#include "greylock/ThemeLoader.h"
#include "jot/Logging.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QFontDatabase>
#include <QTimer>

namespace jot {

ThemeController::ThemeController(QObject *parent)
    : QObject(parent), m_spec(greylock::ThemeLoader::defaultTheme()),
      m_watcher(new QFileSystemWatcher(this))
{
    // Bundled so the default theme looks the same everywhere; a theme may still
    // name any installed family.
    for (const char *font :
         {":/jot/fonts/Nunito-Variable.ttf", ":/jot/fonts/Fraunces-Variable.ttf"}) {
        if (QFontDatabase::addApplicationFont(QString::fromLatin1(font)) < 0)
            qCWarning(lcTheme) << "could not load the bundled font" << font;
    }
    connect(m_watcher, &QFileSystemWatcher::fileChanged, this, [this] {
        // Editors often save by writing a new file and renaming it, which briefly
        // removes the watched path, so reload a moment later and re-watch.
        QTimer::singleShot(100, this, &ThemeController::reloadWatchedFile);
    });
}

ThemeController *ThemeController::instance()
{
    static ThemeController *theme = new ThemeController(QCoreApplication::instance());
    return theme;
}

ThemeController *ThemeController::create(QQmlEngine *, QJSEngine *)
{
    ThemeController *theme = instance();
    QJSEngine::setObjectOwnership(theme, QJSEngine::CppOwnership);
    return theme;
}

QStringList ThemeController::load(const QString &idOrPath)
{
    const greylock::ThemeLoadResult result = greylock::ThemeLoader::load(idOrPath);
    if (!result.ok())
        return result.errors;

    if (!m_watchedPath.isEmpty())
        m_watcher->removePath(m_watchedPath);
    m_watchedPath = greylock::ThemeLoader::isPath(idOrPath) ? QFileInfo(idOrPath).absoluteFilePath()
                                                            : QString();
    if (!m_watchedPath.isEmpty())
        m_watcher->addPath(m_watchedPath);

    m_spec = result.theme;
    m_warnings = result.warnings;
    qCInfo(lcTheme) << "loaded theme" << m_spec.name << "from" << idOrPath;
    for (const QString &warning : m_warnings)
        qCInfo(lcTheme) << "theme warning:" << warning;
    Q_EMIT changed();
    return {};
}

void ThemeController::reloadWatchedFile()
{
    if (m_watchedPath.isEmpty())
        return;
    if (!m_watcher->files().contains(m_watchedPath) && QFileInfo::exists(m_watchedPath))
        m_watcher->addPath(m_watchedPath);

    const greylock::ThemeLoadResult result =
        greylock::ThemeLoader::loadFile(m_watchedPath, greylock::ThemeLoader::defaultTheme());
    if (!result.ok()) {
        // A half-typed edit should not wipe the look; keep the last good theme.
        for (const QString &error : result.errors)
            qCWarning(lcTheme).noquote() << m_watchedPath << "not applied:" << error;
        return;
    }
    m_spec = result.theme;
    m_warnings = result.warnings;
    qCInfo(lcTheme) << "reloaded theme" << m_spec.name;
    Q_EMIT changed();
}

QColor ThemeController::tint(const QColor &color, qreal alpha) const
{
    QColor tinted = color;
    tinted.setAlphaF(float(alpha));
    return tinted;
}

QColor ThemeController::dangerEdge() const
{
    return m_spec.colors.danger.darker(140);
}

QColor ThemeController::dangerText() const
{
    // Whichever of the theme's own text and background reads better on the red.
    const QColor &red = m_spec.colors.danger;
    const QColor &text = m_spec.colors.text, &ground = m_spec.colors.background;
    return greylock::color::contrastRatio(text, red) >= greylock::color::contrastRatio(ground, red)
               ? text
               : ground;
}

QColor ThemeController::shadowColor() const
{
    QColor shadow = m_spec.shadow.color;
    shadow.setAlphaF(float(m_spec.shadow.opacity));
    return shadow;
}

double ThemeController::contrast(const QColor &a, const QColor &b) const
{
    return greylock::color::contrastRatio(a, b);
}

QVariantList ThemeController::swatches() const
{
    const greylock::ThemeSpec::Colors &c = m_spec.colors;
    const std::pair<const char *, QColor> tokens[] = {
        {"background", c.background},  {"surface", c.surface},      {"surface-alt", c.surfaceAlt},
        {"hairline", c.hairline},      {"border", c.border},        {"text", c.text},
        {"text-muted", c.textMuted},   {"text-faint", c.textFaint}, {"accent", c.accent},
        {"accent-text", c.accentText}, {"danger", c.danger},        {"edge", c.edge},
        {"accent-edge", c.accentEdge},
    };
    QVariantList list;
    for (const auto &[name, color] : tokens)
        list.append(QVariantMap{{QStringLiteral("name"), QLatin1String(name)},
                                {QStringLiteral("color"), color}});
    return list;
}

int ThemeController::easingBounce() const
{
    return m_spec.motion.bounce > 0 ? QEasingCurve::OutBack : QEasingCurve::OutCubic;
}

QString ThemeController::noteFontFamily(const QString &choice) const
{
    if (choice == u"mono")
        return m_spec.type.monoFamily.isEmpty() ? QStringLiteral("monospace")
                                                : m_spec.type.monoFamily;
    if (choice == u"serif")
        return m_spec.type.displayFamily.isEmpty() ? QStringLiteral("serif")
                                                   : m_spec.type.displayFamily;
    return editorFontFamily();
}

} // namespace jot
