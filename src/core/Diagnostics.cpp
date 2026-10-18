#include "jot/Diagnostics.h"

#include "jot/LogFile.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QHash>
#include <QLocale>
#include <QRegularExpression>
#include <QSysInfo>
#include <QTimeZone>
#include <QTimer>
#include <QUrlQuery>

#include <algorithm>

namespace jot::diagnostics {

namespace {

constexpr int kLogLines = 30;

QString lastLines(const QString &path, int count)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    QStringList lines = QString::fromUtf8(file.readAll()).split(u'\n', Qt::SkipEmptyParts);
    if (lines.size() > count)
        lines = lines.mid(lines.size() - count);
    return lines.join(u'\n');
}

QString environment(const QString &extra)
{
    QStringList lines{
        QStringLiteral("Jot %1").arg(QCoreApplication::applicationVersion()),
        QStringLiteral("Qt %1").arg(QString::fromLatin1(qVersion())),
        QStringLiteral("OS: %1, kernel %2, %3")
            .arg(QSysInfo::prettyProductName(), QSysInfo::kernelVersion(),
                 QSysInfo::currentCpuArchitecture()),
        QStringLiteral("Session: %1, desktop %2")
            .arg(qEnvironmentVariable("XDG_SESSION_TYPE", QStringLiteral("unknown")),
                 qEnvironmentVariable("XDG_CURRENT_DESKTOP", QStringLiteral("unknown"))),
        QStringLiteral("Locale: %1, time zone %2")
            .arg(QLocale().name(), QString::fromUtf8(QTimeZone::systemTimeZoneId())),
    };
    if (!extra.isEmpty())
        lines.append(extra);
    return lines.join(u'\n');
}

} // namespace

QString redact(const QString &text)
{
    static const QRegularExpression email(
        QStringLiteral(R"([A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Za-z]{2,})"));
    QHash<QString, int> seen;
    QString result;
    qsizetype from = 0;
    for (const QRegularExpressionMatch &match : email.globalMatch(text)) {
        result += text.mid(from, match.capturedStart() - from);
        const QString address = match.captured().toLower();
        if (!seen.contains(address))
            seen.insert(address, int(seen.size()) + 1);
        result += QStringLiteral("<email-%1>").arg(seen.value(address));
        from = match.capturedEnd();
    }
    result += text.mid(from);
    // The home folder's name is usually the user's name.
    const QString home = QDir::homePath();
    if (home.size() > 1)
        result.replace(home, QStringLiteral("~"));
    return result;
}

QString collect(const QString &extra)
{
    QStringList sections{environment(extra),
                         QStringLiteral("Logs: %1").arg(logfile::defaultDirectory())};
    const QDir logs(logfile::defaultDirectory());
    for (const QString &name : {QStringLiteral("jot-gui.log"), QStringLiteral("jot.log")}) {
        const QString tail = lastLines(logs.filePath(name), kLogLines);
        if (!tail.isEmpty())
            sections.append(
                QStringLiteral("Last %1 lines of %2:\n%3").arg(kLogLines).arg(name, tail));
    }
    return redact(sections.join(QStringLiteral("\n\n")));
}

QString shortenedMark()
{
    return QStringLiteral("\n[shortened; run `jot doctor` for the rest]");
}

QUrl fitIssueUrl(const std::function<QUrl(const QString &)> &build, const QString &text,
                 const QString &mark)
{
    // The limit is on the encoded URL, where a space or newline takes three
    // characters, so the text is cut until the encoded form fits.
    QUrl url = build(text);
    QString body = text;
    while (url.toString(QUrl::FullyEncoded).size() > kMaxIssueUrlLength && !body.isEmpty()) {
        const qsizetype over = url.toString(QUrl::FullyEncoded).size() - kMaxIssueUrlLength;
        body.chop(std::max<qsizetype>(over / 3, 64));
        url = build(body + mark);
    }
    return url;
}

QUrl issueUrl(const QString &report)
{
    return fitIssueUrl(
        [](const QString &body) {
            QUrl url(QStringLiteral("https://github.com/greylocklabs/jot/issues/new"));
            QUrlQuery query;
            query.addQueryItem(QStringLiteral("template"), QStringLiteral("bug_report.yml"));
            query.addQueryItem(QStringLiteral("diagnostics"), body);
            url.setQuery(query);
            return url;
        },
        report, shortenedMark());
}

} // namespace jot::diagnostics
