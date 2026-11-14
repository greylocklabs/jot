#include "FakeHttpServer.h"

#include <QDesktopServices>
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQmlExtensionPlugin>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTest>
#include <QtWebEngineQuick/qtwebenginequickglobal.h>

Q_IMPORT_QML_PLUGIN(Jot_UiPlugin)

using namespace Qt::StringLiterals;

/// Takes the links the player hands to the browser, so the test sees them instead.
class Browser : public QObject
{
    Q_OBJECT

public:
    QList<QUrl> opened;

public Q_SLOTS:
    void open(const QUrl &url) { opened.append(url); }
};

class TestWebVideoPlayer : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void staysOnItsSiteAndKeepsNothing();
};

void TestWebVideoPlayer::staysOnItsSiteAndKeepsNothing()
{
    // A stand-in for the site's player, with a link to another site, as to the video's page.
    FakeHttpServer site;
    site.replier = [](const FakeHttpServer::Request &request) {
        FakeHttpServer::Reply reply;
        reply.contentType = "text/html";
        reply.body = request.target.startsWith("/embed")
                         ? QByteArray("<html><body><a id='away' href='http://localhost:1/watch'>"
                                      "watch</a><script>document.title='ready'</script></body>"
                                      "</html>")
                         : QByteArray("<html></html>");
        return reply;
    };
    Browser browser;
    QDesktopServices::setUrlHandler(u"http"_s, &browser, "open");
    QDesktopServices::setUrlHandler(u"https"_s, &browser, "open");

    QQuickWindow window;
    window.resize(640, 360);
    window.show();
    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.loadFromModule("Jot.Ui", "WebVideoPlayer");
    // Told what to play by its card, as the card loads it.
    QQmlComponent cardComponent(&engine);
    cardComponent.loadFromModule("Jot.Ui", "WebVideo");
    std::unique_ptr<QObject> card(
        cardComponent.createWithInitialProperties({{u"embed"_s, site.url(u"/embed/abc"_s)}}));
    QVERIFY2(card, qPrintable(cardComponent.errorString()));
    std::unique_ptr<QObject> made(
        component.createWithInitialProperties({{u"card"_s, QVariant::fromValue(card.get())}}));
    QVERIFY2(made, qPrintable(component.errorString()));
    auto *player = qobject_cast<QQuickItem *>(made.get());
    player->setParentItem(window.contentItem());
    player->setSize({640, 360});

    QObject *web = nullptr;
    for (QObject *child : player->children()) {
        if (child->inherits("QQuickWebEngineView"))
            web = child;
    }
    QVERIFY(web);
    // It asks the site to play at once, in a profile that keeps nothing.
    QTRY_COMPARE_WITH_TIMEOUT(web->property("title").toString(), u"ready"_s, 20000);
    QVERIFY(web->property("url").toUrl().query().contains(u"autoplay=1"_s));
    auto *profile = web->property("profile").value<QObject *>();
    QVERIFY(profile->property("offTheRecord").toBool());

    // A link to another site opens in the browser, and the player stays.
    QVERIFY(QMetaObject::invokeMethod(
        web, "runJavaScript", Q_ARG(QString, u"document.getElementById('away').click()"_s)));
    QTRY_COMPARE_WITH_TIMEOUT(browser.opened.size(), 1, 10000);
    QCOMPARE(browser.opened.first(), QUrl(u"http://localhost:1/watch"_s));
    QVERIFY(web->property("url").toUrl().path().startsWith(u"/embed"_s));
    QDesktopServices::unsetUrlHandler(u"http"_s);
    QDesktopServices::unsetUrlHandler(u"https"_s);
}

int main(int argc, char *argv[])
{
    // As the app does: before the application, for the player to work at all.
    QtWebEngineQuick::initialize();
    QGuiApplication app(argc, argv);
    TestWebVideoPlayer test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_webvideoplayer.moc"
