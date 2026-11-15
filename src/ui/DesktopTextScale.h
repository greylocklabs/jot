#pragma once

#include <QDBusConnection>
#include <QDBusVariant>
#include <QObject>

#include <optional>

namespace jot {

/// How large the desktop draws its text, as the settings portal tells it, so Jot's text can
/// follow: GNOME's text scaling factor, and KDE's font size against its usual 10 points.
class DesktopTextScale : public QObject
{
    Q_OBJECT

public:
    explicit DesktopTextScale(QDBusConnection bus = QDBusConnection::sessionBus(),
                              QObject *parent = nullptr);

    /// Asks the portal, and follows its changes; scaleChanged() says what it answers.
    void start();
    [[nodiscard]] qreal scale() const;

    /// The scale for GNOME's `text-scaling-factor` and KDE's `font`, either missing.
    [[nodiscard]] static qreal scaleFrom(std::optional<double> gnomeFactor, const QString &kdeFont);

Q_SIGNALS:
    void scaleChanged(qreal scale);

private Q_SLOTS:
    void settingChanged(const QString &group, const QString &key, const QDBusVariant &value);

private:
    void read(const QString &group, const QString &key);
    void take(const QString &group, const QString &key, const QVariant &value);

    QDBusConnection m_bus;
    std::optional<double> m_gnomeFactor;
    QString m_kdeFont;
};

} // namespace jot
