#include "FakeHttpServer.h"

#include <QDesktopServices>
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQmlExtensionPlugin>
#include <QQuickItem>
#include <QQuickWindow>
#include <QScopeGuard>
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
    // A stand-in for the site's player, which follows a link to another site, as a click on
    // its link to the video's page would.
    FakeHttpServer site;
    site.replier = [](const FakeHttpServer::Request &request) {
        FakeHttpServer::Reply reply;
        reply.contentType = "text/html";
        reply.body = request.target.startsWith("/embed")
                         ? QByteArray("<html><body><a id='away' href='http://localhost:1/watch'>"
                                      "watch</a><script>setTimeout(() => "
                                      "document.getElementById('away').click(), 200)</script>"
                                      "</body></html>")
                         : QByteArray("<html></html>");
        return reply;
    };
    Browser browser;
    QDesktopServices::setUrlHandler(u"http"_s, &browser, "open");
    QDesktopServices::setUrlHandler(u"https"_s, &browser, "open");
    const auto unset = qScopeGuard([] {
        QDesktopServices::unsetUrlHandler(u"http"_s);
        QDesktopServices::unsetUrlHandler(u"https"_s);
    });

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

    // The site is asked to play at once, by a page it can name, as YouTube wants.
    QTRY_VERIFY_WITH_TIMEOUT(!site.requests.isEmpty(), 20000);
    const FakeHttpServer::Request &asked = site.requests.first();
    QVERIFY2(asked.target.contains("autoplay=1"), asked.target.constData());
    QVERIFY2(asked.headers.value("referer").startsWith("http://jotwriter.org/"),
             asked.headers.value("referer").constData());
    // In a profile that keeps nothing.
    QVERIFY(web->property("profile").value<QObject *>()->property("offTheRecord").toBool());

    // The link to another site opens in the browser, and the player stays.
    QTRY_COMPARE_WITH_TIMEOUT(browser.opened.size(), 1, 10000);
    QCOMPARE(browser.opened.first(), QUrl(u"http://localhost:1/watch"_s));
    QVERIFY(web->property("url").toUrl().toString().startsWith(u"http://jotwriter.org/"_s));
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
