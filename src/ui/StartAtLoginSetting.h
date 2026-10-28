#pragma once

#include "greylock/StartAtLogin.h"

#include <QObject>
#include <QQmlEngine>

namespace jot {

/// Core's start-at-login setting for QML, which cannot see core's own types.
class StartAtLoginSetting : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(StartAtLogin)
    QML_SINGLETON
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY changed)
    /// False for a vault opened for one run, which never touches the session.
    Q_PROPERTY(bool available READ available NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)

public:
    explicit StartAtLoginSetting(QObject *parent = nullptr) : QObject(parent)
    {
        connect(core(), &greylock::StartAtLogin::changed, this, &StartAtLoginSetting::changed);
    }

    [[nodiscard]] bool enabled() const { return core()->enabled(); }
    void setEnabled(bool enabled) { core()->setEnabled(enabled); }
    [[nodiscard]] bool available() const { return core()->available(); }
    [[nodiscard]] QString error() const { return core()->error(); }

Q_SIGNALS:
    void changed();

private:
    static greylock::StartAtLogin *core() { return greylock::StartAtLogin::instance(); }
};

} // namespace jot
