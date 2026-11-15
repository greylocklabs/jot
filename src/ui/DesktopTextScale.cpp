#include "DesktopTextScale.h"

#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>

#include <algorithm>

using namespace Qt::StringLiterals;

namespace jot {

namespace {

const QString kService = u"org.freedesktop.portal.Desktop"_s;
const QString kPath = u"/org/freedesktop/portal/desktop"_s;
const QString kSettings = u"org.freedesktop.portal.Settings"_s;
const QString kGnome = u"org.gnome.desktop.interface"_s;
const QString kGnomeKey = u"text-scaling-factor"_s;
const QString kKde = u"org.kde.kdeglobals.General"_s;
const QString kKdeKey = u"font"_s;
constexpr double kKdePoints = 10;

} // namespace

DesktopTextScale::DesktopTextScale(QDBusConnection bus, QObject *parent)
    : QObject(parent), m_bus(std::move(bus))
{}

void DesktopTextScale::start()
{
    m_bus.connect(kService, kPath, kSettings, u"SettingChanged"_s, this,
                  SLOT(settingChanged(QString, QString, QDBusVariant)));
    read(kGnome, kGnomeKey);
    read(kKde, kKdeKey);
}

qreal DesktopTextScale::scale() const
{
    return scaleFrom(m_gnomeFactor, m_kdeFont);
}

qreal DesktopTextScale::scaleFrom(std::optional<double> gnomeFactor, const QString &kdeFont)
{
    double scale = gnomeFactor.value_or(1.0) > 0 ? gnomeFactor.value_or(1.0) : 1.0;
    // A Qt font description: family, then point size, then the rest.
    bool read = false;
    const double points = kdeFont.section(u',', 1, 1).toDouble(&read);
    if (read && points > 0)
        scale *= points / kKdePoints;
    return std::clamp(scale, 0.5, 3.0);
}

void DesktopTextScale::read(const QString &group, const QString &key)
{
    QDBusMessage call = QDBusMessage::createMethodCall(kService, kPath, kSettings, u"ReadOne"_s);
    call << group << key;
    auto *watcher = new QDBusPendingCallWatcher(m_bus.asyncCall(call, 2000), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, group, key](auto *done) {
        const QDBusPendingReply<QDBusVariant> reply = *done;
        if (!reply.isError())
            take(group, key, reply.value().variant());
        done->deleteLater();
    });
}

void DesktopTextScale::settingChanged(const QString &group, const QString &key,
                                      const QDBusVariant &value)
{
    take(group, key, value.variant());
}

void DesktopTextScale::take(const QString &group, const QString &key, const QVariant &value)
{
    const qreal before = scale();
    if (group == kGnome && key == kGnomeKey)
        m_gnomeFactor = value.toDouble();
    else if (group == kKde && key == kKdeKey)
        m_kdeFont = value.toString();
    else
        return;
    if (!qFuzzyCompare(before, scale()))
        Q_EMIT scaleChanged(scale());
}

} // namespace jot
