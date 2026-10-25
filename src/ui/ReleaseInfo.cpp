#include "ReleaseInfo.h"

#include "greylock/Identity.h"

#include "SettingsForeign.h"

#include "greylock/ReleaseNotes.h"
#include "greylock/Releases.h"
#include "greylock/UpdateCheck.h"
#include "jot/Settings.h"

#include <QCoreApplication>

using namespace Qt::StringLiterals;

namespace jot {

ReleaseInfo::ReleaseInfo(QObject *parent) : QObject(parent)
{
    if (const auto notes =
            greylock::ReleaseNotes::latest(greylock::ReleaseNotes::bundledChangelog())) {
        m_notesVersion = notes->version;
        m_notesDate = notes->date.startOfDay();
        m_notes = notes->body;
    }
}

void ReleaseInfo::watchForUpdates(Settings *settings, greylock::UpdateCheck *check)
{
    m_settings = settings;
    m_check = check;
    connect(m_check, &greylock::UpdateCheck::checked, this, &ReleaseInfo::showUpdate);
    connect(m_settings, &Settings::checkForUpdatesChanged, this, [this] {
        showUpdate();
        checkIfDue();
    });
    // Looked at hourly, and asked once a day; the first look waits until the
    // window has settled.
    m_checkTimer.setInterval(std::chrono::hours(1));
    connect(&m_checkTimer, &QTimer::timeout, this, &ReleaseInfo::checkIfDue);
    m_checkTimer.start();
    QTimer::singleShot(std::chrono::seconds(30), this, &ReleaseInfo::checkIfDue);
    showUpdate();
}

void ReleaseInfo::checkIfDue()
{
    if (m_check && m_settings && m_check->wanted(m_settings->checkForUpdates()) && m_check->due())
        m_check->check();
}

void ReleaseInfo::showUpdate()
{
    std::optional<greylock::Release> newer;
    if (m_check && m_settings && m_check->wanted(m_settings->checkForUpdates()))
        newer = m_check->newerThan(QVersionNumber::fromString(version()));
    const QString update = newer ? newer->version.toString() : QString();
    const QUrl page = newer ? newer->page : QUrl();
    if (update == m_update && page == m_updatePage)
        return;
    m_update = update;
    m_updatePage = page;
    Q_EMIT updateChanged();
}

QString ReleaseInfo::version() const
{
    return QCoreApplication::applicationVersion();
}

QUrl ReleaseInfo::website() const
{
    return QUrl(u"https://jotwriter.org/"_s);
}

QUrl ReleaseInfo::privacy() const
{
    return QUrl(u"https://jotwriter.org/privacy.html"_s);
}

QUrl ReleaseInfo::terms() const
{
    return QUrl(u"https://jotwriter.org/terms.html"_s);
}

QUrl ReleaseInfo::source() const
{
    return QUrl(u"https://github.com/greylocklabs/jot"_s);
}

QUrl ReleaseInfo::support() const
{
    return greylock::releases::supportPage();
}

QUrl ReleaseInfo::history() const
{
    return QUrl(u"https://github.com/greylocklabs/jot/blob/main/CHANGELOG.md"_s);
}

bool ReleaseInfo::takeUpdateNotice()
{
    Settings *settings = SettingsForeign::create(nullptr, nullptr);
    const QString last = settings->lastSeenVersion();
    settings->setLastSeenVersion(version());
    return !last.isEmpty() && last != version() && !m_notes.isEmpty();
}

bool ReleaseInfo::devel() const
{
    return greylock::identity::isDevel();
}

} // namespace jot
