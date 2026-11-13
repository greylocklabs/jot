#include "QuickCapture.h"

#include <QDBusObjectPath>
#include <QSignalSpy>
#include <QTest>

using namespace Qt::StringLiterals;

class TestQuickCapture : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void isNotThereWithoutThePortal();
    void passesOnTheDesktopsToken();
};

void TestQuickCapture::isNotThereWithoutThePortal()
{
    // A bus with no portal on it, as on an older desktop or with no session at all.
    jot::QuickCapture capture(QDBusConnection(u"jot-test-no-bus"_s));
    QSignalSpy changed(&capture, &jot::QuickCapture::changed);
    QVERIFY(!capture.available());
    capture.setWanted(true);
    QVERIFY(!capture.bound());
    QVERIFY(capture.trigger().isEmpty());
    capture.setWanted(false);
    QVERIFY(changed.isEmpty());
}

void TestQuickCapture::passesOnTheDesktopsToken()
{
    // The key pressed, with the token that lets the window come to the front over another.
    jot::QuickCapture capture(QDBusConnection(u"jot-test-no-bus"_s));
    QSignalSpy activated(&capture, &jot::QuickCapture::activated);
    const QVariantMap options{{u"activation_token"_s, u"kwin-42"_s}};
    QVERIFY(QMetaObject::invokeMethod(
        &capture, "shortcutActivated", Q_ARG(QDBusObjectPath, QDBusObjectPath()),
        Q_ARG(QString, u"quick-add"_s), Q_ARG(qulonglong, 0), Q_ARG(QVariantMap, options)));
    QCOMPARE(activated.count(), 1);
    QCOMPARE(activated.first().first().toString(), u"kwin-42"_s);
}

QTEST_GUILESS_MAIN(TestQuickCapture)
#include "tst_quickcapture.moc"
