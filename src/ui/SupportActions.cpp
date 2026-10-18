#include "SupportActions.h"

#include "VaultController.h"

#include "jot/Diagnostics.h"
#include "jot/LogFile.h"
#include "jot/ProblemReport.h"
#include "jot/Settings.h"
#include "jot/UpdateCheck.h"

#include <QClipboard>
#include <QDesktopServices>
#include <QFileInfo>
#include <QGuiApplication>
#include <QPointer>
#include <QUrl>

using namespace Qt::StringLiterals;

namespace jot {

QString SupportActions::collect()
{
    return diagnostics::collect(
        QStringLiteral("Qt platform: %1").arg(QGuiApplication::platformName()));
}

void SupportActions::copyDebugInfo()
{
    QGuiApplication::clipboard()->setText(collect());
    Q_EMIT copied();
}

void SupportActions::openLogs()
{
    QDesktopServices::openUrl(QUrl::fromLocalFile(logfile::defaultDirectory()));
}

void SupportActions::openFolder(const QString &which)
{
    if (which == u"vault"_s) {
        if (const VaultController *vault = VaultController::instance(); vault && vault->isOpen())
            QDesktopServices::openUrl(QUrl::fromLocalFile(vault->root()));
        return;
    }
    const QString file =
        which == u"config"_s ? Settings::defaultPath() : UpdateCheck::defaultPath();
    QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(file).absolutePath()));
}

void SupportActions::copyText(const QString &text)
{
    QGuiApplication::clipboard()->setText(text);
    Q_EMIT copied();
}

void SupportActions::reportOnGitHub(bool idea, const QString &summary, const QString &note,
                                    const QString &diagnostics)
{
    QDesktopServices::openUrl(problemreport::issueUrl({idea, summary, note, diagnostics}));
}

void SupportActions::gatherDiagnostics()
{
    Q_EMIT diagnosticsReady(collect());
}

void SupportActions::sendReport(bool idea, const QString &summary, const QString &note,
                                const QString &diagnostics)
{
    problemreport::send(m_network, problemreport::serviceUrl(), {idea, summary, note, diagnostics},
                        [self = QPointer(this)](const QUrl &issue, const QString &error) {
                            if (!self)
                                return;
                            if (error.isEmpty())
                                Q_EMIT self->reportSent(issue);
                            else
                                Q_EMIT self->reportFailed(error);
                        });
}

} // namespace jot
