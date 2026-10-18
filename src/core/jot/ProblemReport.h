#pragma once

#include <QByteArray>
#include <QNetworkAccessManager>
#include <QString>
#include <QUrl>

#include <functional>

namespace jot::problemreport {

/// What a person sends from "Report a problem": a bug or an idea, a one-line
/// summary, what happened in their words, and the diagnostics they chose to
/// include.
struct Report
{
    bool idea = false;
    QString summary;
    QString note;
    QString diagnostics;
};

/// The shared Greylock Labs report service, which files a report as a public
/// issue in greylocklabs/jot. JOT_REPORT_SERVICE overrides it, for tests.
[[nodiscard]] QUrl serviceUrl();

/// The report as the service takes it, with Jot's version and the system it
/// runs on. Longer fields are cut to what the service accepts.
[[nodiscard]] QByteArray payload(const Report &report, const QString &appVersion,
                                 const QString &platform);

/// The report as a new issue on GitHub, for when the service cannot take it.
/// A bug fills in the bug report form. Diagnostics too long for the address
/// are cut from their end, as `doctor --report` cuts them, then the note.
[[nodiscard]] QUrl issueUrl(const Report &report);

/// Sends the report, then calls `done` with the new issue's address, or with
/// an error to show when it could not be filed. Only an issue in
/// greylocklabs/jot counts as filed.
void send(QNetworkAccessManager &network, const QUrl &service, const Report &report,
          std::function<void(const QUrl &issue, const QString &error)> done);

} // namespace jot::problemreport
