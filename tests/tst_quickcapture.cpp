#include "QuickCapture.h"

#include <QSignalSpy>
#include <QTest>

using namespace Qt::StringLiterals;

class TestQuickCapture : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void isNotThereWithoutThePortal();
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

QTEST_GUILESS_MAIN(TestQuickCapture)
#include "tst_quickcapture.moc"
