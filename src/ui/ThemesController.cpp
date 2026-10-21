#include "ThemesController.h"

#include "SettingsForeign.h"
#include "ThemeController.h"

#include "greylock/ThemeLoader.h"
#include "jot/Logging.h"
#include "jot/Settings.h"

#include <QCoreApplication>
#include <QFileInfo>

using namespace Qt::StringLiterals;

namespace jot {

namespace {

const QString kDefaultTheme = u"callie"_s;

QColor colorFor(const greylock::ThemeSpec::Colors &c, const QString &key)
{
    static const QHash<QString, QColor greylock::ThemeSpec::Colors::*> members{
        {u"background"_s, &greylock::ThemeSpec::Colors::background},
        {u"surface"_s, &greylock::ThemeSpec::Colors::surface},
        {u"surface-alt"_s, &greylock::ThemeSpec::Colors::surfaceAlt},
        {u"hairline"_s, &greylock::ThemeSpec::Colors::hairline},
        {u"border"_s, &greylock::ThemeSpec::Colors::border},
        {u"text"_s, &greylock::ThemeSpec::Colors::text},
        {u"text-muted"_s, &greylock::ThemeSpec::Colors::textMuted},
        {u"text-faint"_s, &greylock::ThemeSpec::Colors::textFaint},
        {u"accent"_s, &greylock::ThemeSpec::Colors::accent},
        {u"accent-text"_s, &greylock::ThemeSpec::Colors::accentText},
        {u"accent-edge"_s, &greylock::ThemeSpec::Colors::accentEdge},
        {u"edge"_s, &greylock::ThemeSpec::Colors::edge},
        {u"danger"_s, &greylock::ThemeSpec::Colors::danger},
    };
    const auto member = members.value(key);
    return member ? c.*member : QColor();
}

} // namespace

ThemesController::ThemesController(QObject *parent)
    : QObject(parent), m_library(greylock::ThemeLibrary::defaultFolder())
{
    connect(ThemeController::instance(), &ThemeController::changed, this,
            &ThemesController::changed);
}

ThemesController *ThemesController::instance()
{
    static auto *themes = new ThemesController(QCoreApplication::instance());
    return themes;
}

ThemesController *ThemesController::create(QQmlEngine *, QJSEngine *)
{
    ThemesController *themes = instance();
    QJSEngine::setObjectOwnership(themes, QJSEngine::CppOwnership);
    return themes;
}

Settings *ThemesController::settings() const
{
    return SettingsForeign::create(nullptr, nullptr);
}

void ThemesController::restore()
{
    const QString theme = settings()->theme();
    if (theme.isEmpty())
        return;
    const QStringList errors = ThemeController::instance()->load(theme);
    if (errors.isEmpty())
        return;
    qCWarning(lcTheme).noquote() << "the saved theme" << theme
                                 << "does not load, using the default:" << errors.join(u"; "_s);
    settings()->setTheme({});
}

QStringList ThemesController::applyForThisRun(const QString &idOrPath)
{
    const QStringList errors = ThemeController::instance()->load(idOrPath);
    if (errors.isEmpty()) {
        m_override = idOrPath;
        Q_EMIT changed();
    }
    return errors;
}

QVariantList ThemesController::available() const
{
    QVariantList themes;
    for (const QString &id : greylock::ThemeLoader::builtInIds())
        themes << QVariantMap{{u"id"_s, id},
                              {u"name"_s, greylock::ThemeLibrary::displayName(id)},
                              {u"editable"_s, false}};
    for (const QString &path : m_library.themes())
        themes << QVariantMap{{u"id"_s, path},
                              {u"name"_s, greylock::ThemeLibrary::displayName(path)},
                              {u"editable"_s, true}};
    return themes;
}

QString ThemesController::current() const
{
    if (!m_override.isEmpty())
        return m_override;
    const QString theme = settings()->theme();
    return theme.isEmpty() ? kDefaultTheme : theme;
}

bool ThemesController::editable() const
{
    return m_library.themes().contains(QFileInfo(current()).absoluteFilePath());
}

QVariantMap ThemesController::colors() const
{
    const greylock::ThemeSpec::Colors &c = ThemeController::instance()->spec().colors;
    QVariantMap map;
    for (const QString &key : greylock::ThemeLibrary::colorKeys())
        map.insert(key, colorFor(c, key));
    return map;
}

bool ThemesController::use(const QString &idOrPath)
{
    const QStringList errors = ThemeController::instance()->load(idOrPath);
    if (!errors.isEmpty())
        return fail(errors.join(u"; "_s));
    m_override.clear();
    settings()->setTheme(idOrPath == kDefaultTheme ? QString() : idOrPath);
    clearError();
    Q_EMIT changed();
    return true;
}

bool ThemesController::customize()
{
    if (editable())
        return true;
    const QString name = tr("My %1").arg(greylock::ThemeLibrary::displayName(current()));
    const QString path = m_library.copy(current(), name);
    if (path.isEmpty())
        return fail(m_library.error());
    return use(path);
}

bool ThemesController::setColor(const QString &key, const QColor &color)
{
    if (!editable())
        return fail(tr("Built-in themes cannot be changed; customize a copy instead."));
    if (!m_library.setColor(current(), key, color))
        return fail(m_library.error());
    // Applied now rather than when the file watcher notices, so edits feel live.
    return use(current());
}

bool ThemesController::importFrom(const QUrl &file)
{
    const QString path = m_library.importTheme(file.toLocalFile());
    if (path.isEmpty())
        return fail(m_library.error());
    return use(path);
}

bool ThemesController::exportTo(const QUrl &file)
{
    if (!m_library.exportTheme(current(), file.toLocalFile()))
        return fail(m_library.error());
    clearError();
    return true;
}

bool ThemesController::removeCurrent()
{
    if (!editable())
        return fail(tr("Built-in themes cannot be deleted."));
    const QString path = current();
    useDefault();
    if (!m_library.remove(path))
        return fail(m_library.error());
    Q_EMIT changed();
    return true;
}

void ThemesController::useDefault()
{
    use(kDefaultTheme);
}

bool ThemesController::fail(const QString &message)
{
    m_error = message;
    Q_EMIT errorChanged();
    return false;
}

void ThemesController::clearError()
{
    if (m_error.isEmpty())
        return;
    m_error.clear();
    Q_EMIT errorChanged();
}

} // namespace jot
