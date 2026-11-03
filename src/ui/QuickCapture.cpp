#include "QuickCapture.h"

#include "jot/Logging.h"

#include "greylock/Identity.h"

#include <QCoreApplication>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusObjectPath>
#include <QDBusPendingCall>
#include <QRandomGenerator>

using namespace Qt::StringLiterals;

namespace {

const QString kService = u"org.freedesktop.portal.Desktop"_s;
const QString kPath = u"/org/freedesktop/portal/desktop"_s;
const QString kShortcuts = u"org.freedesktop.portal.GlobalShortcuts"_s;

/// A shortcut as the portal takes and gives them: its id and what is known of it.
struct PortalShortcut
{
    QString id;
    QVariantMap properties;
};

} // namespace

Q_DECLARE_METATYPE(PortalShortcut)

namespace {

QDBusArgument &operator<<(QDBusArgument &argument, const PortalShortcut &shortcut)
{
    argument.beginStructure();
    argument << shortcut.id << shortcut.properties;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, PortalShortcut &shortcut)
{
    argument.beginStructure();
    argument >> shortcut.id >> shortcut.properties;
    argument.endStructure();
    return argument;
}

/// A path the portal sends as a string or an object path.
QString pathOf(const QVariant &value)
{
    return value.userType() == qMetaTypeId<QDBusObjectPath>()
               ? value.value<QDBusObjectPath>().path()
               : value.toString();
}

} // namespace

namespace jot {

QuickCapture::QuickCapture(QDBusConnection bus, QObject *parent)
    : QObject(parent), m_bus(std::move(bus))
{
    qDBusRegisterMetaType<PortalShortcut>();
    qDBusRegisterMetaType<QList<PortalShortcut>>();
    if (!m_bus.isConnected() || !m_bus.interface() ||
        !m_bus.interface()->isServiceRegistered(kService))
        return;
    QDBusMessage get = QDBusMessage::createMethodCall(
        kService, kPath, u"org.freedesktop.DBus.Properties"_s, u"Get"_s);
    get << kShortcuts << u"version"_s;
    m_available = m_bus.call(get, QDBus::Block, 2000).type() == QDBusMessage::ReplyMessage;
    if (m_available)
        m_bus.connect(kService, kPath, kShortcuts, u"Activated"_s, this,
                      SLOT(shortcutActivated(QDBusObjectPath, QString, qulonglong, QVariantMap)));
}

QuickCapture *QuickCapture::create(QQmlEngine *, QJSEngine *)
{
    if (!s_instance)
        s_instance = new QuickCapture(QDBusConnection::sessionBus(), QCoreApplication::instance());
    QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
    return s_instance;
}

void QuickCapture::setWanted(bool wanted)
{
    m_wanted = wanted;
    if (!m_available)
        return;
    if (!wanted) {
        close();
        return;
    }
    if (bound() || m_opening)
        return;
    // An app outside a sandbox says who it is first, so the desktop can keep its key.
    QDBusMessage registry = QDBusMessage::createMethodCall(
        kService, kPath, u"org.freedesktop.host.portal.Registry"_s, u"Register"_s);
    registry << greylock::identity::appId() << QVariantMap();
    m_bus.call(registry, QDBus::Block, 2000);
    const QString token = u"jot_session_%1"_s.arg(QRandomGenerator::global()->generate());
    m_opening = request(u"CreateSession"_s, {}, token, SLOT(sessionCreated(uint, QVariantMap)));
}

bool QuickCapture::request(const QString &method, const QList<QVariant> &arguments,
                           const QString &token, const char *slot)
{
    // The answer comes as a Response on a path made of this connection's name and the token,
    // listened for before asking, so it cannot come first.
    const QString handle =
        u"jot_%1_%2"_s.arg(++m_tokens).arg(QRandomGenerator::global()->generate());
    QString sender = m_bus.baseService().mid(1);
    sender.replace(u'.', u'_');
    const QString path = kPath + u"/request/"_s + sender + u'/' + handle;
    m_bus.connect(kService, path, u"org.freedesktop.portal.Request"_s, u"Response"_s, this, slot);
    QDBusMessage call = QDBusMessage::createMethodCall(kService, kPath, kShortcuts, method);
    QVariantMap options{{u"handle_token"_s, handle}};
    if (method == u"CreateSession")
        options.insert(u"session_handle_token"_s, token);
    QList<QVariant> all = arguments;
    all.append(options);
    call.setArguments(all);
    return m_bus.send(call);
}

void QuickCapture::sessionCreated(uint response, const QVariantMap &results)
{
    m_opening = false;
    if (response != 0) {
        qCWarning(lcUi) << "the desktop did not start a shortcut session:" << response;
        return;
    }
    m_session = pathOf(results.value(u"session_handle"_s));
    Q_EMIT changed();
    if (!m_wanted) {
        close();
        return;
    }
    bind();
}

void QuickCapture::bind()
{
    const QList<PortalShortcut> shortcuts = {
        {QString::fromLatin1(kId),
         {{u"description"_s, tr("Add a to-do to Jot's inbox")},
          {u"preferred_trigger"_s, QString::fromLatin1(kPreferred)}}}};
    request(u"BindShortcuts"_s,
            {QVariant::fromValue(QDBusObjectPath(m_session)), QVariant::fromValue(shortcuts),
             QString()},
            {}, SLOT(shortcutsBound(uint, QVariantMap)));
}

void QuickCapture::shortcutsBound(uint response, const QVariantMap &results)
{
    if (response != 0) {
        qCInfo(lcUi) << "the desktop did not give a quick capture key:" << response;
        return;
    }
    const auto shortcuts =
        qdbus_cast<QList<PortalShortcut>>(results.value(u"shortcuts"_s).value<QDBusArgument>());
    for (const PortalShortcut &shortcut : shortcuts) {
        if (shortcut.id == QLatin1StringView(kId))
            m_trigger = shortcut.properties.value(u"trigger_description"_s).toString();
    }
    qCInfo(lcUi) << "quick capture key:" << m_trigger;
    Q_EMIT changed();
}

void QuickCapture::shortcutActivated(const QDBusObjectPath &session, const QString &id, qulonglong,
                                     const QVariantMap &)
{
    if (session.path() == m_session && id == QLatin1StringView(kId))
        Q_EMIT activated();
}

void QuickCapture::close()
{
    if (m_session.isEmpty())
        return;
    m_bus.send(QDBusMessage::createMethodCall(kService, m_session,
                                              u"org.freedesktop.portal.Session"_s, u"Close"_s));
    m_session.clear();
    m_trigger.clear();
    Q_EMIT changed();
}

} // namespace jot
