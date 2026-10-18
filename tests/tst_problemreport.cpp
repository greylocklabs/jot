#include "FakeHttpServer.h"

#include "jot/ProblemReport.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QTest>
#include <QUrlQuery>

using namespace jot;
using namespace Qt::StringLiterals;

class TestProblemReport : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void payloadCarriesTheReport();
    void longDiagnosticsKeepTheirEnd();
    void sendsAndReturnsTheIssue();
    void explainsWhyAReportWasNotFiled();
    void onlyAnIssueHereCountsAsFiled();
    void issueUrlFillsInTheForm();
    void issueUrlFitsLongDiagnostics();
    void theServiceCanBeOverridden();

private:
    struct Result
    {
        QUrl issue;
        QString error;
        bool done = false;
    };
    static Result send(const QUrl &service, const problemreport::Report &report);
};

TestProblemReport::Result TestProblemReport::send(const QUrl &service,
                                                  const problemreport::Report &report)
{
    QNetworkAccessManager network;
    Result result;
    problemreport::send(
        network, service, report,
        [&result](const QUrl &issue, const QString &error) { result = {issue, error, true}; });
    [&] { QTRY_VERIFY_WITH_TIMEOUT(result.done, 10000); }();
    return result;
}

void TestProblemReport::payloadCarriesTheReport()
{
    const QJsonObject body =
        QJsonDocument::fromJson(
            problemreport::payload({false, u"  Search misses a note  "_s, u" "_s, u"Jot 0.0.7"_s},
                                   u"0.0.7"_s, u"Fedora Linux 44"_s))
            .object();
    QCOMPARE(body[u"app"_s].toString(), u"jot"_s);
    QCOMPARE(body[u"kind"_s].toString(), u"bug"_s);
    QCOMPARE(body[u"summary"_s].toString(), u"Search misses a note"_s);
    QVERIFY2(!body.contains(u"note"_s), "a blank note is left out");
    QCOMPARE(body[u"logs"_s].toString(), u"Jot 0.0.7"_s);
    QCOMPARE(body[u"appVersion"_s].toString(), u"0.0.7"_s);
    QCOMPARE(body[u"platform"_s].toString(), u"Fedora Linux 44"_s);

    const QJsonObject idea =
        QJsonDocument::fromJson(
            problemreport::payload({true, u"Note templates"_s, u"For meeting notes"_s, {}},
                                   u"0.0.7"_s, u"Debian"_s))
            .object();
    QCOMPARE(idea[u"kind"_s].toString(), u"idea"_s);
    QCOMPARE(idea[u"note"_s].toString(), u"For meeting notes"_s);
    QVERIFY2(!idea.contains(u"logs"_s), "no diagnostics, no logs");
}

void TestProblemReport::longDiagnosticsKeepTheirEnd()
{
    const QString diagnostics = u"first line\n"_s + QString(20000, u'x') + u"\nthe failure"_s;
    const QString logs =
        QJsonDocument::fromJson(
            problemreport::payload({false, u"s"_s, {}, diagnostics}, u"1"_s, u"p"_s))
            .object()[u"logs"_s]
            .toString();
    QVERIFY(logs.size() <= 16000);
    QVERIFY(logs.startsWith(u"[earlier lines left out]"_s));
    QVERIFY(logs.endsWith(u"the failure"_s));
}

void TestProblemReport::sendsAndReturnsTheIssue()
{
    FakeHttpServer service;
    service.respond(201, R"({"number":7,"url":"https://github.com/greylocklabs/jot/issues/7"})");
    const Result result = send(service.url(u"/report"_s), {false, u"Broken"_s, u"It broke"_s, {}});
    QCOMPARE(result.error, QString());
    QCOMPARE(result.issue, QUrl(u"https://github.com/greylocklabs/jot/issues/7"_s));

    QCOMPARE(service.requests.size(), 1);
    const FakeHttpServer::Request &request = service.requests.first();
    QCOMPARE(request.method, "POST");
    QCOMPARE(request.target, "/report");
    QVERIFY(request.headers.value("content-type").startsWith("application/json"));
    QCOMPARE(QJsonDocument::fromJson(request.body).object()[u"note"_s].toString(), u"It broke"_s);
}

void TestProblemReport::explainsWhyAReportWasNotFiled()
{
    FakeHttpServer service;
    service.respond(429, R"({"error":"too many reports; try again in a minute"})");
    Result result = send(service.url(u"/report"_s), {false, u"s"_s, {}, {}});
    QVERIFY(result.issue.isEmpty());
    QVERIFY2(result.error.contains(u"Too many reports"_s), qPrintable(result.error));

    service.respond(400, R"({"error":"missing or overlong summary"})");
    result = send(service.url(u"/report"_s), {false, u"s"_s, {}, {}});
    QVERIFY2(result.error.contains(u"missing or overlong summary"_s), qPrintable(result.error));

    // A port nothing listens on.
    QTcpServer closed;
    QVERIFY(closed.listen(QHostAddress::LocalHost, 0));
    const quint16 port = closed.serverPort();
    closed.close();
    result = send(QUrl(u"http://127.0.0.1:%1/report"_s.arg(port)), {false, u"s"_s, {}, {}});
    QVERIFY2(result.error.startsWith(u"Could not reach the report service"_s),
             qPrintable(result.error));
}

void TestProblemReport::onlyAnIssueHereCountsAsFiled()
{
    FakeHttpServer service;
    for (const char *url : {"file:///etc/passwd", "https://example.org/greylocklabs/jot/issues/7",
                            "https://github.com/someone/else/issues/7"}) {
        service.respond(201, QByteArray(R"({"url":")") + url + R"("})");
        const Result result = send(service.url(u"/report"_s), {false, u"s"_s, {}, {}});
        QVERIFY2(result.issue.isEmpty(), url);
        QVERIFY2(result.error.contains(u"unexpected address"_s), qPrintable(result.error));
    }
}

void TestProblemReport::issueUrlFillsInTheForm()
{
    const QUrlQuery bug(
        problemreport::issueUrl({false, u" Broken "_s, u"It broke"_s, u"Jot 0.0.7"_s}));
    QCOMPARE(bug.queryItemValue(u"template"_s), u"bug_report.yml"_s);
    QCOMPARE(bug.queryItemValue(u"title"_s), u"Broken"_s);
    QCOMPARE(bug.queryItemValue(u"what"_s), u"It broke"_s);
    QCOMPARE(bug.queryItemValue(u"diagnostics"_s), u"Jot 0.0.7"_s);

    const QUrlQuery without(problemreport::issueUrl({false, u"Broken"_s, {}, {}}));
    QVERIFY2(!without.hasQueryItem(u"diagnostics"_s), "diagnostics left out stay out");

    const QUrl idea = problemreport::issueUrl({true, u"Note templates"_s, u"For meetings"_s, {}});
    QCOMPARE(idea.host(), u"github.com"_s);
    QCOMPARE(idea.path(), u"/greylocklabs/jot/issues/new"_s);
    const QUrlQuery ideaQuery(idea);
    QVERIFY2(!ideaQuery.hasQueryItem(u"template"_s), "an idea is not a bug report");
    QCOMPARE(ideaQuery.queryItemValue(u"body"_s), u"For meetings"_s);
}

void TestProblemReport::issueUrlFitsLongDiagnostics()
{
    const QString diagnostics = u"the version\n"_s + QString(20000, u' ') + u"\nthe end"_s;
    const QUrl url = problemreport::issueUrl({false, u"s"_s, u"n"_s, diagnostics});
    QVERIFY(url.toString(QUrl::FullyEncoded).size() <= 8000);
    const QUrlQuery query(url);
    QCOMPARE(query.queryItemValue(u"what"_s), u"n"_s);
    const QString kept = query.queryItemValue(u"diagnostics"_s, QUrl::FullyDecoded);
    QVERIFY(kept.startsWith(u"the version"_s));
    QVERIFY(kept.endsWith(u"for the rest]"_s));

    // Newlines take three characters each once encoded.
    const QUrl note = problemreport::issueUrl(
        {false, u"s"_s, u"a"_s + QString(3998, u'\n') + u"z"_s, diagnostics});
    QVERIFY(note.toString(QUrl::FullyEncoded).size() <= 8000);
    QVERIFY2(!QUrlQuery(note).hasQueryItem(u"diagnostics"_s), "the diagnostics go first");
}

void TestProblemReport::theServiceCanBeOverridden()
{
    QVERIFY(problemreport::serviceUrl().toString().startsWith(u"https://"_s));
    qputenv("JOT_REPORT_SERVICE", "http://127.0.0.1:1/report");
    QCOMPARE(problemreport::serviceUrl(), QUrl(u"http://127.0.0.1:1/report"_s));
    qunsetenv("JOT_REPORT_SERVICE");
}

QTEST_GUILESS_MAIN(TestProblemReport)
#include "tst_problemreport.moc"
