#include "ReleaseInfo.h"
#include "SettingsForeign.h"

#include "jot/Settings.h"

#include <QTemporaryDir>
#include <QTest>

using namespace jot;
using namespace Qt::StringLiterals;

class TestRelease : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void notesComeFromTheBundledChangelog();
    void setsBreakingChangesApart();
    void updateNoticeShowsOncePerNewVersion();
};

void TestRelease::notesComeFromTheBundledChangelog()
{
    const ReleaseInfo release;
    QVERIFY(!release.notesVersion().isEmpty());
    QVERIFY(release.notesDate().isValid());
    // Any kind of section; a release of fixes alone has no "New".
    QVERIFY(release.notes().contains(u"### "_s));
    QCOMPARE(release.support(), QUrl(u"https://ko-fi.com/sayonara_lara"_s));
}

void TestRelease::setsBreakingChangesApart()
{
    const auto [rest, breaking] = ReleaseInfo::splitBreaking(
        u"### Breaking\n\n- Old markers are not read.\n\n### Added\n\n- New ones.\n\n"
        u"### Fixed\n\n- A bug.\n"_s);
    QCOMPARE(breaking, u"- Old markers are not read."_s);
    QCOMPARE(rest, u"### Added\n\n- New ones.\n\n### Fixed\n\n- A bug."_s);
    // Notes with nothing breaking are left whole.
    QCOMPARE(ReleaseInfo::splitBreaking(u"### Added\n\n- New.\n"_s),
             (std::pair{u"### Added\n\n- New."_s, QString()}));
}

void TestRelease::updateNoticeShowsOncePerNewVersion()
{
    QTemporaryDir dir;
    Settings settings(dir.filePath(u"settings.ini"_s));
    SettingsForeign::s_instance = &settings;
    ReleaseInfo release;

    // A first run only records the version.
    QCoreApplication::setApplicationVersion(u"0.1.0"_s);
    QVERIFY(!release.takeUpdateNotice());
    QCOMPARE(settings.lastSeenVersion(), u"0.1.0"_s);
    QVERIFY(!release.takeUpdateNotice());

    // A new version says so once.
    QCoreApplication::setApplicationVersion(u"0.2.0"_s);
    QVERIFY(release.takeUpdateNotice());
    QVERIFY(!release.takeUpdateNotice());
    SettingsForeign::s_instance = nullptr;
}

QTEST_GUILESS_MAIN(TestRelease)
#include "tst_release.moc"
