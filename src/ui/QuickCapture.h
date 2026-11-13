#pragma once

#include <QDBusArgument>
#include <QDBusConnection>
#include <QObject>
#include <QQmlEngine>
#include <QVariantMap>

namespace jot {

/// A key that adds a to-do from anywhere on the desktop, asked of the desktop through the
/// GlobalShortcuts portal, as Wayland leaves no other way. The desktop asks the user to allow
/// it the first time, and lets them choose another key. Without the portal, as on an older
/// desktop, it is not available, and a key bound to `jot-gui --quick-add` does the same.
class QuickCapture : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    /// The desktop offers system-wide keys to apps.
    Q_PROPERTY(bool available READ available NOTIFY changed)
    /// The key is set up.
    Q_PROPERTY(bool bound READ bound NOTIFY changed)
    /// The key the desktop gave, in its words, such as "Ctrl+Alt+J"; empty until it says.
    Q_PROPERTY(QString trigger READ trigger NOTIFY changed)

public:
    explicit QuickCapture(QDBusConnection bus = QDBusConnection::sessionBus(),
                          QObject *parent = nullptr);

    static QuickCapture *create(QQmlEngine *, QJSEngine *);
    static inline QuickCapture *s_instance = nullptr;

    /// Asks the desktop for the key, or lets it go.
    void setWanted(bool wanted);

    [[nodiscard]] bool available() const { return m_available; }
    [[nodiscard]] bool bound() const { return !m_session.isEmpty(); }
    [[nodiscard]] QString trigger() const { return m_trigger; }

    /// The key Jot asks for first; the desktop may give another.
    static constexpr const char *kPreferred = "CTRL+ALT+J";
    static constexpr const char *kId = "quick-add";

Q_SIGNALS:
    void changed();
    /// The key was pressed; `token`, when the desktop gave one, lets Jot come to the front.
    void activated(const QString &token);

private Q_SLOTS:
    void sessionCreated(uint response, const QVariantMap &results);
    void shortcutsBound(uint response, const QVariantMap &results);
    void shortcutActivated(const QDBusObjectPath &session, const QString &id, qulonglong timestamp,
                           const QVariantMap &options);

private:
    /// Calls the portal's `method`, its answer coming to `slot` as a Response signal.
    bool request(const QString &method, const QList<QVariant> &arguments, const QString &token,
                 const char *slot);
    void bind();
    void close();

    QDBusConnection m_bus;
    bool m_available = false;
    bool m_wanted = false;
    /// A session was asked for and has not come yet.
    bool m_opening = false;
    QString m_session;
    QString m_trigger;
    int m_tokens = 0;
};

} // namespace jot
