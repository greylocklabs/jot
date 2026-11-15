#include "greylock/LogFile.h"
#include "jot/Diagnostics.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>
#include <QUrlQuery>

using namespace jot;
using namespace Qt::StringLiterals;

class TestDiagnostics : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void init();

    void emailsAreNumberedConsistently();
    void homeFolderIsHidden();
    void reportCoversVersionsAndLogsAndHidesEmails();

private:
    std::unique_ptr<QTemporaryDir> m_dir;
};

void TestDiagnostics::init()
{
    // Every path the report reads from, kept out of the real home folder.
    m_dir = std::make_unique<QTemporaryDir>();
    qputenv("XDG_CONFIG_HOME", m_dir->filePath(u"config"_s).toUtf8());
    qputenv("XDG_CACHE_HOME", m_dir->filePath(u"cache"_s).toUtf8());
    qputenv("XDG_STATE_HOME", m_dir->filePath(u"state"_s).toUtf8());
}

void TestDiagnostics::emailsAreNumberedConsistently()
{
    QCOMPARE(diagnostics::redact(
                 u"a@x.com then B@X.com, a@x.com again, team@group.calendar.google.com"_s),
             u"<email-1> then <email-2>, <email-1> again, <email-3>"_s);
}

void TestDiagnostics::homeFolderIsHidden()
{
    const QString text = QDir::homePath() + u"/.local/state/jot/logs"_s;
    QCOMPARE(diagnostics::redact(text), u"~/.local/state/jot/logs"_s);
}

void TestDiagnostics::reportCoversVersionsAndLogsAndHidesEmails()
{
    QCoreApplication::setApplicationVersion(u"0.1.0"_s);
    QVERIFY(QDir().mkpath(greylock::logfile::defaultDirectory()));
    QFile log(QDir(greylock::logfile::defaultDirectory()).filePath(u"jot.log"_s));
    QVERIFY(log.open(QIODevice::WriteOnly));
    log.write("2026-10-14 12:00:00 jot.sync info: signed in as me@example.com\n");
    log.close();

    const QString report = diagnostics::collect(u"Extra: yes"_s);

    QVERIFY2(report.startsWith(u"Jot 0.1.0\nQt "_s), qPrintable(report));
    QVERIFY(report.contains(u"Extra: yes"_s));
    QVERIFY(report.contains(u"Last 30 lines of jot.log:\n"_s));
    QVERIFY(report.contains(u"signed in as <email-1>"_s));
    QVERIFY(!report.contains(u"me@example.com"_s));
    QVERIFY(!report.contains(QDir::homePath()));
}

QTEST_GUILESS_MAIN(TestDiagnostics)
#include "tst_diagnostics.moc"
