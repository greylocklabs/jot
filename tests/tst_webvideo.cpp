#include "jot/WebVideo.h"

#include <QTest>

using namespace jot;
using namespace Qt::StringLiterals;

class TestWebVideo : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void readsTheSitesItPlays_data();
    void readsTheSitesItPlays();
    void leavesOtherLinksAsLinks_data();
    void leavesOtherLinksAsLinks();
};

void TestWebVideo::readsTheSitesItPlays_data()
{
    QTest::addColumn<QString>("link");
    QTest::addColumn<QString>("site");
    QTest::addColumn<QString>("embed");
    const QString yt = u"https://www.youtube-nocookie.com/embed/dQw4w9WgXcQ"_s;
    QTest::newRow("watch") << u"https://www.youtube.com/watch?v=dQw4w9WgXcQ"_s << u"YouTube"_s
                           << yt;
    QTest::newRow("mobile") << u"https://m.youtube.com/watch?v=dQw4w9WgXcQ&feature=share"_s
                            << u"YouTube"_s << yt;
    QTest::newRow("short link") << u"https://youtu.be/dQw4w9WgXcQ?si=abc"_s << u"YouTube"_s << yt;
    QTest::newRow("shorts") << u"https://youtube.com/shorts/dQw4w9WgXcQ"_s << u"YouTube"_s << yt;
    QTest::newRow("embed") << u"https://www.youtube.com/embed/dQw4w9WgXcQ"_s << u"YouTube"_s << yt;
    QTest::newRow("music") << u"https://music.youtube.com/watch?v=dQw4w9WgXcQ&si=x"_s
                           << u"YouTube"_s << yt;
    QTest::newRow("start in seconds")
        << u"https://youtu.be/dQw4w9WgXcQ?t=194"_s << u"YouTube"_s << yt + u"?start=194"_s;
    QTest::newRow("start in minutes") << u"https://www.youtube.com/watch?v=dQw4w9WgXcQ&t=1m30s"_s
                                      << u"YouTube"_s << yt + u"?start=90"_s;
    QTest::newRow("vimeo") << u"https://vimeo.com/76979871"_s << u"Vimeo"_s
                           << u"https://player.vimeo.com/video/76979871?dnt=1"_s;
    QTest::newRow("unlisted vimeo")
        << u"https://vimeo.com/76979871/af3e1b2c9d"_s << u"Vimeo"_s
        << u"https://player.vimeo.com/video/76979871?h=af3e1b2c9d&dnt=1"_s;
    QTest::newRow("vimeo player") << u"https://player.vimeo.com/video/76979871"_s << u"Vimeo"_s
                                  << u"https://player.vimeo.com/video/76979871?dnt=1"_s;
}

void TestWebVideo::readsTheSitesItPlays()
{
    QFETCH(QString, link);
    QFETCH(QString, site);
    QFETCH(QString, embed);
    const std::optional<WebVideo> video = webVideo(link);
    QVERIFY(video);
    QCOMPARE(video->site, site);
    QCOMPARE(video->embed.toString(), embed);
    QCOMPARE(video->link.toString(), link);
}

void TestWebVideo::leavesOtherLinksAsLinks_data()
{
    QTest::addColumn<QString>("link");
    QTest::newRow("another site") << u"https://example.com/video.mp4"_s;
    QTest::newRow("a channel") << u"https://www.youtube.com/@someone"_s;
    QTest::newRow("a playlist") << u"https://www.youtube.com/playlist?list=PL123"_s;
    QTest::newRow("a short id") << u"https://youtu.be/abc"_s;
    QTest::newRow("vimeo, not a video") << u"https://vimeo.com/channels/staffpicks"_s;
    QTest::newRow("a vault file") << u"crossing.mp4"_s;
    QTest::newRow("not the web") << u"file:///home/me/video.mp4"_s;
}

void TestWebVideo::leavesOtherLinksAsLinks()
{
    QFETCH(QString, link);
    QVERIFY(!webVideo(link));
}

QTEST_GUILESS_MAIN(TestWebVideo)
#include "tst_webvideo.moc"
