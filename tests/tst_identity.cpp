#include "greylock/Identity.h"
#include "jot/LogFile.h"

#include <QTest>

using namespace jot;
using namespace Qt::StringLiterals;

class TestIdentity : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void developmentBuildsStandApart();
    void thisBuildIsWhatCMakeSays();
};

void TestIdentity::developmentBuildsStandApart()
{
    // An installed Jot and a development build never share a name or folder.
    const greylock::identity::Names plain = greylock::identity::namesFor(false);
    QCOMPARE(plain.appId, u"org.callieapp.Jot"_s);
    QCOMPARE(plain.dirName, u"jot"_s);
    QCOMPARE(plain.displayName, u"Jot"_s);
    const greylock::identity::Names devel = greylock::identity::namesFor(true);
    QCOMPARE(devel.appId, u"org.callieapp.Jot.Devel"_s);
    QCOMPARE(devel.dirName, u"jot-devel"_s);
    QCOMPARE(devel.displayName, u"Jot Devel"_s);
}

void TestIdentity::thisBuildIsWhatCMakeSays()
{
    QCOMPARE(greylock::identity::isDevel(), bool(JOT_EXPECT_DEVEL));
    QCOMPARE(greylock::identity::appId(), greylock::identity::namesFor(JOT_EXPECT_DEVEL).appId);
    QVERIFY(
        logfile::defaultDirectory().endsWith(u'/' + greylock::identity::dirName() + u"/logs"_s));
}

QTEST_GUILESS_MAIN(TestIdentity)
#include "tst_identity.moc"
