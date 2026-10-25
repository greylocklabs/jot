#include "greylock/ReleaseNotes.h"

#include <QTest>

using namespace greylock;
using namespace Qt::StringLiterals;

class TestReleaseNotes : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void bundledMatchesTheBuild();
};

void TestReleaseNotes::bundledMatchesTheBuild()
{
    const auto notes = ReleaseNotes::latest(ReleaseNotes::bundledChangelog());
    QVERIFY(notes);
    QCOMPARE(notes->version, QStringLiteral(JOT_VERSION));
    QVERIFY(!notes->body.isEmpty());
}

QTEST_GUILESS_MAIN(TestReleaseNotes)
#include "tst_releasenotes.moc"
