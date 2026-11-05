#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QTemporaryDir>
#include <QTest>

using namespace Qt::StringLiterals;

/// `jot check` run as a CI job would: on files, with no vault and no app settings.
class TestCheckCommand : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void init();
    void passesWhatFits();
    void saysWhereEachProblemIs();
    void refusesWhatItCannotCheck();

private:
    struct Result
    {
        int code = -1;
        QString out;
        QString err;
    };
    Result run(const QStringList &arguments) const;
    void write(const QString &name, const QByteArray &text) const;

    std::unique_ptr<QTemporaryDir> m_dir;
};

void TestCheckCommand::init()
{
    m_dir = std::make_unique<QTemporaryDir>();
    write(u"cast.schema.json"_s,
          "{\"type\": \"array\", \"items\": {\"required\": [\"name\"], \"properties\": "
          "{\"age\": {\"type\": \"integer\"}}}}");
}

TestCheckCommand::Result TestCheckCommand::run(const QStringList &arguments) const
{
    QProcess jot;
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    // Its own empty folders, never the user's settings, and no update check.
    for (const char *name :
         {"XDG_CONFIG_HOME", "XDG_DATA_HOME", "XDG_STATE_HOME", "XDG_CACHE_HOME"})
        environment.insert(QString::fromLatin1(name), m_dir->filePath(QString::fromLatin1(name)));
    environment.insert(u"JOT_NO_UPDATE_CHECK"_s, u"1"_s);
    environment.insert(u"NO_COLOR"_s, u"1"_s);
    jot.setProcessEnvironment(environment);
    jot.setWorkingDirectory(m_dir->path());
    jot.start(QStringLiteral(JOT_BINARY), QStringList{u"check"_s} + arguments);
    jot.waitForFinished(30000);
    return {jot.exitCode(), QString::fromUtf8(jot.readAllStandardOutput()),
            QString::fromUtf8(jot.readAllStandardError())};
}

void TestCheckCommand::write(const QString &name, const QByteArray &text) const
{
    QFile file(m_dir->filePath(name));
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(text);
}

void TestCheckCommand::passesWhatFits()
{
    write(u"cast.json"_s, "[{\"name\": \"Mara\", \"age\": 31}]\n");
    write(u"rows.csv"_s, "a,b\n1,2\n");
    const Result result = run({u"cast.json"_s, u"rows.csv"_s});
    QCOMPARE(result.code, 0);
    QCOMPARE(result.out, QString());
}

void TestCheckCommand::saysWhereEachProblemIs()
{
    write(u"cast.json"_s, "[\n  {\"name\": \"Mara\", \"age\": \"x\"},\n  {\"age\": 3}\n]\n");
    write(u"broken.yaml"_s, "- a: [1\n");
    const Result result = run({u"cast.json"_s, u"broken.yaml"_s});
    QCOMPARE(result.code, 1);
    const QStringList lines = result.out.split(u'\n', Qt::SkipEmptyParts);
    QCOMPARE(lines.size(), 3);
    QCOMPARE(lines.at(0), u"cast.json:2:27: age: should be a whole number"_s);
    QCOMPARE(lines.at(1), u"cast.json:3:3: name: is required"_s);
    QVERIFY2(lines.at(2).startsWith(u"broken.yaml:2:1: "_s), qPrintable(lines.at(2)));

    const Result json = run({u"--json"_s, u"cast.json"_s});
    QCOMPARE(json.code, 1);
    const QJsonArray found = QJsonDocument::fromJson(json.out.toUtf8()).array();
    QCOMPARE(found.size(), 2);
    QCOMPARE(found.at(0).toObject().value(u"line"_s).toInt(), 2);
    QCOMPARE(found.at(0).toObject().value(u"path"_s).toString(), u"cast.json"_s);
}

void TestCheckCommand::refusesWhatItCannotCheck()
{
    write(u"notes.md"_s, "# hi\n");
    QCOMPARE(run({u"notes.md"_s}).code, 2);
    QCOMPARE(run({u"missing.json"_s}).code, 2);
    QCOMPARE(run({}).code, 2);
}

QTEST_GUILESS_MAIN(TestCheckCommand)
#include "tst_checkcommand.moc"
