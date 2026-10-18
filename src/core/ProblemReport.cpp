#include "jot/ProblemReport.h"

#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSysInfo>
#include <QUrlQuery>

#include <algorithm>

using namespace Qt::StringLiterals;

namespace jot::problemreport {

namespace {

// The service's own limits; anything longer is refused rather than cut.
constexpr qsizetype kSummary = 100;
constexpr qsizetype kNote = 4000;
constexpr qsizetype kLogs = 16000;
// GitHub refuses longer new-issue addresses.
constexpr qsizetype kMaxUrlLength = 8000;

const QString kIssues = u"https://github.com/greylocklabs/jot/issues"_s;

/// Logs keep their end, where the failure usually is.
QString lastPart(const QString &text, qsizetype max)
{
    if (text.size() <= max)
        return text;
    const QString mark = u"[earlier lines left out]\n"_s;
    return mark + text.right(max - mark.size());
}

bool isOurIssue(const QUrl &url)
{
    return url.scheme() == u"https"_s && url.host() == u"github.com"_s &&
           url.path().startsWith(u"/greylocklabs/jot/issues/"_s);
}

} // namespace

QUrl serviceUrl()
{
    const QString override = qEnvironmentVariable("JOT_REPORT_SERVICE");
    return QUrl(override.isEmpty()
                    ? u"https://greylocklabs-reports.lararosekelley.workers.dev/report"_s
                    : override);
}

QByteArray payload(const Report &report, const QString &appVersion, const QString &platform)
{
    QJsonObject body{
        {u"app"_s, u"jot"_s},
        {u"kind"_s, report.idea ? u"idea"_s : u"bug"_s},
        {u"summary"_s, report.summary.trimmed().left(kSummary)},
        {u"appVersion"_s, appVersion.left(40)},
        {u"platform"_s, platform.left(100)},
    };
    if (!report.note.trimmed().isEmpty())
        body[u"note"_s] = report.note.trimmed().left(kNote);
    if (!report.diagnostics.trimmed().isEmpty())
        body[u"logs"_s] = lastPart(report.diagnostics.trimmed(), kLogs);
    return QJsonDocument(body).toJson(QJsonDocument::Compact);
}

QUrl issueUrl(const Report &report)
{
    const QString summary = report.summary.trimmed().left(kSummary);
    const QString note = report.note.trimmed().left(kNote);
    const auto build = [&](const QString &diagnostics) {
        QUrlQuery query;
        query.addQueryItem(u"title"_s, summary);
        if (report.idea) {
            QString body = note;
            if (!diagnostics.isEmpty())
                body += u"\n\n```text\n"_s + diagnostics + u"\n```"_s;
            query.addQueryItem(u"body"_s, body.trimmed());
        } else {
            query.addQueryItem(u"template"_s, u"bug_report.yml"_s);
            query.addQueryItem(u"what"_s, note);
            if (!diagnostics.isEmpty())
                query.addQueryItem(u"diagnostics"_s, diagnostics);
        }
        QUrl url(kIssues + u"/new"_s);
        url.setQuery(query);
        return url;
    };

    // The limit is on the encoded address, where a space or newline takes
    // three characters, so the diagnostics shrink until it fits.
    const QString diagnostics = report.diagnostics.trimmed();
    qsizetype keep = diagnostics.size();
    QUrl url = build(diagnostics);
    while (url.toString(QUrl::FullyEncoded).size() > kMaxUrlLength && keep > 0) {
        const qsizetype over = url.toString(QUrl::FullyEncoded).size() - kMaxUrlLength;
        keep -= std::max<qsizetype>(over / 3, 64);
        if (keep < 64)
            keep = 0;
        url = build(keep > 0 ? lastPart(diagnostics, keep) : QString());
    }
    return url;
}

void send(QNetworkAccessManager &network, const QUrl &service, const Report &report,
          std::function<void(const QUrl &issue, const QString &error)> done)
{
    QNetworkRequest request(service);
    request.setHeader(QNetworkRequest::ContentTypeHeader, u"application/json"_s);
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      u"Jot/"_s + QCoreApplication::applicationVersion());
    request.setTransferTimeout(20000);
    QNetworkReply *reply =
        network.post(request, payload(report, QCoreApplication::applicationVersion(),
                                      QSysInfo::prettyProductName()));
    QObject::connect(reply, &QNetworkReply::finished, reply, [reply, done = std::move(done)] {
        reply->deleteLater();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QJsonObject answer = QJsonDocument::fromJson(reply->readAll()).object();
        const QUrl issue(answer.value(u"url"_s).toString());
        if (status == 201 && isOurIssue(issue)) {
            done(issue, {});
            return;
        }
        if (status == 201) {
            done({},
                 QCoreApplication::translate(
                     "ProblemReport", "The report service answered with an unexpected address."));
            return;
        }
        if (status == 429) {
            done({}, QCoreApplication::translate(
                         "ProblemReport", "Too many reports from here. Try again in a minute."));
            return;
        }
        if (status == 0) {
            done({}, QCoreApplication::translate("ProblemReport",
                                                 "Could not reach the report service: %1")
                         .arg(reply->errorString()));
            return;
        }
        const QString reason = answer.value(u"error"_s).toString();
        done({}, QCoreApplication::translate("ProblemReport", "The report service said no: %1")
                     .arg(reason.isEmpty() ? QString::number(status) : reason));
    });
}

} // namespace jot::problemreport
