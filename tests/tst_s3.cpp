#include "FakeHttpServer.h"

#include "jot/S3Transport.h"
#include "jot/Sync.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>
#include <QUuid>

using namespace Qt::StringLiterals;
using jot::S3Location;
using jot::S3Transport;
using jot::SyncEngine;

/// Against a real S3-compatible store, such as MinIO in CI: JOT_TEST_S3 names the bucket as a
/// sync setting writes it, and JOT_TEST_S3_KEY its key as `<id>:<secret>`. Skipped without them.
class TestS3 : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void init();
    void readsAndWritesSettings();
    void checksTheStoreBeforeUse();
    void writesOnlyOverTheVersionExpected();
    void keepsWhatItDeletesInTheTrash();
    void syncsTwoComputersThroughABucket();
    void deletesNothingWithoutATrashCopy();
    void refusesAStoreThatIgnoresIfMatch();
    void deletesOnlyTheVersionExpected();

private:
    /// A bucket location of its own for each test, under a fresh prefix.
    std::optional<S3Location> fresh() const;
    static void write(const QString &file, const QByteArray &data);
    static QByteArray read(const QString &file);

    QString m_remote;
    QString m_key;
    QTemporaryDir m_dir;
};

void TestS3::init()
{
    m_remote = qEnvironmentVariable("JOT_TEST_S3");
    m_key = qEnvironmentVariable("JOT_TEST_S3_KEY");
}

std::optional<S3Location> TestS3::fresh() const
{
    std::optional<S3Location> location = S3Location::parse(m_remote);
    if (location)
        location->prefix = (location->prefix.isEmpty() ? QString() : location->prefix + u'/') +
                           QUuid::createUuid().toString(QUuid::WithoutBraces);
    return location;
}

void TestS3::write(const QString &file, const QByteArray &data)
{
    QDir().mkpath(QFileInfo(file).path());
    QFile out(file);
    QVERIFY(out.open(QIODevice::WriteOnly));
    out.write(data);
}

QByteArray TestS3::read(const QString &file)
{
    QFile in(file);
    return in.open(QIODevice::ReadOnly) ? in.readAll() : QByteArray();
}

void TestS3::readsAndWritesSettings()
{
    const std::optional<S3Location> aws = S3Location::parse(
        u"s3:https://s3.eu-west-1.amazonaws.com/notes-bucket/jot/main?region=eu-west-1&style=virtual"_s);
    QVERIFY(aws);
    QCOMPARE(aws->endpoint, QUrl(u"https://s3.eu-west-1.amazonaws.com"_s));
    QCOMPARE(aws->region, u"eu-west-1"_s);
    QCOMPARE(aws->bucket, u"notes-bucket"_s);
    QCOMPARE(aws->prefix, u"jot/main"_s);
    QVERIFY(!aws->pathStyle);
    QCOMPARE(S3Location::parse(aws->remote())->remote(), aws->remote());
    const std::optional<S3Location> minio =
        S3Location::parse(u"s3:http://localhost:9000/notes?region=us-east-1"_s);
    QVERIFY(minio && minio->pathStyle && minio->prefix.isEmpty());
    QCOMPARE(minio->endpoint.port(), 9000);
    // A region is needed to sign, and a bucket to put anything in.
    QVERIFY(!S3Location::parse(u"s3:http://localhost:9000/notes"_s));
    QVERIFY(!S3Location::parse(u"s3:http://localhost:9000/?region=us-east-1"_s));
    QVERIFY(!S3Location::parse(u"folder:/mnt"_s));
    QCOMPARE(jot::remoteLabel(aws->remote()),
             u"s3.eu-west-1.amazonaws.com/notes-bucket/jot/main"_s);
    QVERIFY(jot::remoteNeedsSecret(aws->remote()));
    QString error;
    QVERIFY(!jot::transportFor(aws->remote(), {}, &error));
    QVERIFY(error.contains(u"keyring"_s));
}

void TestS3::checksTheStoreBeforeUse()
{
    const std::optional<S3Location> location = fresh();
    if (!location)
        QSKIP("JOT_TEST_S3 is not set");
    S3Transport store(*location, m_key);
    QString error;
    QVERIFY2(store.verify(&error), qPrintable(error));
    // A wrong secret says what the store said.
    S3Transport wrong(*location, m_key.section(u':', 0, 0) + u":not-the-secret"_s);
    QVERIFY(!wrong.verify(&error));
    QVERIFY(!error.isEmpty());
}

void TestS3::writesOnlyOverTheVersionExpected()
{
    const std::optional<S3Location> location = fresh();
    if (!location)
        QSKIP("JOT_TEST_S3 is not set");
    S3Transport store(*location, m_key);
    QString error;
    bool stale = false;
    const std::optional<QString> first =
        store.put(u"Ashfall/Mara Vey.md"_s, "one\n", {}, &stale, &error);
    QVERIFY2(first, qPrintable(error));
    // Not where something is already, nor over another version than the one there.
    QVERIFY(!store.put(u"Ashfall/Mara Vey.md"_s, "two\n", {}, &stale, &error));
    QVERIFY(stale);
    stale = false;
    QVERIFY(!store.put(u"Ashfall/Mara Vey.md"_s, "two\n", u"\"nope\""_s, &stale, &error));
    QVERIFY(stale);
    const std::optional<QString> second =
        store.put(u"Ashfall/Mara Vey.md"_s, "two\n", *first, &stale, &error);
    QVERIFY2(second, qPrintable(error));
    QString rev;
    QCOMPARE(store.get(u"Ashfall/Mara Vey.md"_s, &rev, &error), std::optional(QByteArray("two\n")));
    QCOMPARE(rev, *second);
    const std::optional<QList<jot::RemoteFile>> listed = store.list(&error);
    QVERIFY2(listed, qPrintable(error));
    QCOMPARE(listed->size(), 1);
    QCOMPARE(listed->first().path, u"Ashfall/Mara Vey.md"_s);
    QCOMPARE(listed->first().rev, *second);
}

void TestS3::keepsWhatItDeletesInTheTrash()
{
    const std::optional<S3Location> location = fresh();
    if (!location)
        QSKIP("JOT_TEST_S3 is not set");
    S3Transport store(*location, m_key);
    QString error;
    bool stale = false;
    const std::optional<QString> rev = store.put(u"Old.md"_s, "old\n", {}, &stale, &error);
    QVERIFY(rev);
    QVERIFY(!store.remove(u"Old.md"_s, u"\"another\""_s, &stale, &error));
    QVERIFY(stale);
    QVERIFY2(store.remove(u"Old.md"_s, *rev, &stale, &error), qPrintable(error));
    // Gone from the listing, which leaves the trash out, but kept there.
    QVERIFY(store.list(&error)->isEmpty());
    S3Location whole = *location;
    QString trashRev;
    bool found = false;
    whole.prefix += u"/.jot/trash"_s;
    S3Transport inTrash(whole, m_key);
    const std::optional<QList<jot::RemoteFile>> trashed = inTrash.list(&error);
    QVERIFY2(trashed, qPrintable(error));
    for (const jot::RemoteFile &file : *trashed) {
        found = found || (file.path.endsWith(u"/Old.md"_s) &&
                          inTrash.get(file.path, &trashRev, &error) == QByteArray("old\n"));
    }
    QVERIFY(found);
}

void TestS3::syncsTwoComputersThroughABucket()
{
    const std::optional<S3Location> location = fresh();
    if (!location)
        QSKIP("JOT_TEST_S3 is not set");
    const auto engine = [&](const QString &name) {
        QDir().mkpath(m_dir.filePath(name + u"/vault"_s));
        return std::make_unique<SyncEngine>(m_dir.filePath(name + u"/vault"_s),
                                            std::make_unique<S3Transport>(*location, m_key),
                                            m_dir.filePath(name + u"/state"_s), name);
    };
    const QString laptop = m_dir.filePath(u"laptop/vault"_s);
    const QString desk = m_dir.filePath(u"desk/vault"_s);
    auto onLaptop = engine(u"laptop"_s);
    auto onDesk = engine(u"desk"_s);
    write(laptop + u"/Mara.md"_s, "# Mara\n\nfirst\n\nlast\n");
    QVERIFY(onLaptop->sync().failure.isEmpty());
    QVERIFY(onDesk->sync().failure.isEmpty());
    QCOMPARE(read(desk + u"/Mara.md"_s), QByteArray("# Mara\n\nfirst\n\nlast\n"));
    // Edits to different lines on each merge through the bucket.
    write(laptop + u"/Mara.md"_s, "# Mara\n\nFIRST\n\nlast\n");
    write(desk + u"/Mara.md"_s, "# Mara\n\nfirst\n\nLAST\n");
    QVERIFY(onLaptop->sync().errors.isEmpty());
    QCOMPARE(onDesk->sync().merged, QStringList{u"Mara.md"_s});
    QVERIFY(onLaptop->sync().errors.isEmpty());
    QCOMPARE(read(laptop + u"/Mara.md"_s), QByteArray("# Mara\n\nFIRST\n\nLAST\n"));
}

void TestS3::deletesNothingWithoutATrashCopy()
{
    FakeHttpServer server;
    S3Location location{server.url({}), u"us-east-1"_s, u"b"_s, {}, true};
    S3Transport store(location, u"id:secret"_s);
    // The object is there as expected, but its copy fails inside a 200, as S3 can answer.
    server.replier = [](const FakeHttpServer::Request &request) -> FakeHttpServer::Reply {
        if (request.method == "HEAD")
            return {200, {}, {{"ETag", "\"e1\""}}};
        if (request.method == "PUT")
            return {200,
                    "<Error><Code>InternalError</Code><Message>try again</Message></Error>",
                    {},
                    "application/xml"};
        return {204, {}};
    };
    QString error;
    bool stale = false;
    QVERIFY(!store.remove(u"Mara.md"_s, u"\"e1\""_s, &stale, &error));
    QVERIFY(!stale);
    QVERIFY2(error.contains(u"try again"_s), qPrintable(error));
    for (const FakeHttpServer::Request &request : std::as_const(server.requests))
        QVERIFY(request.method != "DELETE");
    // A version written since the check is not copied, nor deleted.
    server.requests.clear();
    QByteArray condition;
    server.replier = [&condition](const FakeHttpServer::Request &request) -> FakeHttpServer::Reply {
        if (request.method == "HEAD")
            return {200, {}, {{"ETag", "\"e1\""}}};
        condition = request.headers.value("x-amz-copy-source-if-match");
        return {412, "<Error><Code>PreconditionFailed</Code></Error>", {}, "application/xml"};
    };
    QVERIFY(!store.remove(u"Mara.md"_s, u"\"e1\""_s, &stale, &error));
    QVERIFY(stale);
    QCOMPARE(condition, QByteArray("\"e1\""));
}

void TestS3::refusesAStoreThatIgnoresIfMatch()
{
    // A store that heeds If-None-Match but writes over anything given If-Match.
    FakeHttpServer server;
    S3Location location{server.url({}), u"us-east-1"_s, u"b"_s, {}, true};
    S3Transport store(location, u"id:secret"_s);
    bool written = false;
    server.replier = [&written](const FakeHttpServer::Request &request) -> FakeHttpServer::Reply {
        if (request.method == "GET")
            return {200,
                    "<ListBucketResult><IsTruncated>false</IsTruncated></ListBucketResult>",
                    {},
                    "application/xml"};
        if (request.method == "PUT" && request.headers.contains("if-none-match") && written)
            return {412, "<Error><Code>PreconditionFailed</Code></Error>", {}, "application/xml"};
        written = written || request.method == "PUT";
        return {200, {}, {{"ETag", "\"e1\""}}};
    };
    QString error;
    QVERIFY(!store.verify(&error));
    QVERIFY2(error.contains(u"conditional"_s), qPrintable(error));
}

void TestS3::deletesOnlyTheVersionExpected()
{
    FakeHttpServer server;
    S3Location location{server.url({}), u"us-east-1"_s, u"b"_s, {}, true};
    S3Transport store(location, u"id:secret"_s);
    int deletes = 0;
    QByteArray condition;
    int deleteStatus = 412;
    server.replier = [&](const FakeHttpServer::Request &request) -> FakeHttpServer::Reply {
        if (request.method == "HEAD")
            return {200, {}, {{"ETag", "\"e1\""}}};
        if (request.method == "PUT")
            return {200, "<CopyObjectResult/>", {}, "application/xml"};
        ++deletes;
        if (deletes == 1)
            condition = request.headers.value("if-match");
        const int status = deletes == 1 ? deleteStatus : 204;
        return {status,
                status == 204 ? QByteArray() : QByteArray("<Error><Code>X</Code></Error>"),
                {},
                "application/xml"};
    };
    QString error;
    bool stale = false;
    // Changed after the copy: the delete says so, and is not sent again without its condition.
    QVERIFY(!store.remove(u"Mara.md"_s, u"\"e1\""_s, &stale, &error));
    QVERIFY(stale);
    QCOMPARE(condition, QByteArray("\"e1\""));
    QCOMPARE(deletes, 1);
    // Another refusal is an error, again with no second try.
    deletes = 0;
    stale = false;
    deleteStatus = 400;
    QVERIFY(!store.remove(u"Mara.md"_s, u"\"e1\""_s, &stale, &error));
    QVERIFY(!stale);
    QCOMPARE(deletes, 1);
    // A store that does not take If-Match on deletes is asked again without it.
    deletes = 0;
    deleteStatus = 501;
    QVERIFY2(store.remove(u"Mara.md"_s, u"\"e1\""_s, &stale, &error), qPrintable(error));
    QCOMPARE(deletes, 2);
}

QTEST_GUILESS_MAIN(TestS3)
#include "tst_s3.moc"
