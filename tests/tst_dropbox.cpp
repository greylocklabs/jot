#include "FakeHttpServer.h"

#include "jot/DropboxTransport.h"
#include "jot/Sync.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTest>
#include <QUrlQuery>

using namespace Qt::StringLiterals;
using jot::DropboxApi;
using jot::DropboxTransport;

/// A Dropbox held in memory, answering the API's calls as Dropbox does.
class TestDropbox : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void init();
    void signsIn();
    void listsPageByPage();
    void writesOnlyOverTheRevisionExpected();
    void sendsNamesPastAsciiSafely();
    void syncsTwoComputers();

private:
    struct File
    {
        QByteArray data;
        QString rev;
    };
    FakeHttpServer::Reply answer(const FakeHttpServer::Request &request);
    DropboxApi api() const;
    std::unique_ptr<DropboxTransport> transport() const
    {
        return std::make_unique<DropboxTransport>(u"key"_s, u"refresh"_s, u"/Notes"_s, api());
    }

    std::unique_ptr<FakeHttpServer> m_server;
    QMap<QString, File> m_files;
    int m_revs = 0;
    /// Entries a list page holds, so paging shows.
    int m_pageSize = 100;
};

void TestDropbox::init()
{
    m_server = std::make_unique<FakeHttpServer>();
    m_server->replier = [this](const FakeHttpServer::Request &request) { return answer(request); };
    m_files.clear();
    m_pageSize = 100;
}

DropboxApi TestDropbox::api() const
{
    return {m_server->url({}), m_server->url({}), m_server->url(u"/oauth2/authorize"_s)};
}

FakeHttpServer::Reply TestDropbox::answer(const FakeHttpServer::Request &request)
{
    const auto json = [](const QJsonObject &object) {
        return QJsonDocument(object).toJson(QJsonDocument::Compact);
    };
    if (request.target == "/oauth2/token") {
        const QUrlQuery form(QString::fromUtf8(request.body));
        if (form.queryItemValue(u"grant_type"_s) == u"refresh_token" &&
            form.queryItemValue(u"refresh_token"_s) == u"refresh")
            return {200, json({{u"access_token"_s, u"access"_s}})};
        if (form.queryItemValue(u"grant_type"_s) == u"authorization_code" &&
            form.queryItemValue(u"code"_s) == u"good" &&
            !form.queryItemValue(u"code_verifier"_s).isEmpty())
            return {200, json({{u"refresh_token"_s, u"refresh"_s}, {u"access_token"_s, u"a"_s}})};
        return {400, json({{u"error"_s, u"invalid_grant"_s},
                           {u"error_description"_s, u"code doesn't exist"_s}})};
    }
    if (request.headers.value("authorization") != "Bearer access")
        return {401, json({{u"error_summary"_s, u"invalid_access_token/"_s}})};
    const QJsonObject args =
        QJsonDocument::fromJson(request.target.startsWith("/2/files/download") ||
                                        request.target.startsWith("/2/files/upload")
                                    ? request.headers.value("dropbox-api-arg")
                                    : request.body)
            .object();
    const QString path = args.value(u"path"_s).toString();
    if (request.target == "/2/files/list_folder" ||
        request.target == "/2/files/list_folder/continue") {
        const int from =
            request.target.endsWith("continue") ? args.value(u"cursor"_s).toString().toInt() : 0;
        QJsonArray entries;
        const QStringList paths = m_files.keys();
        for (int i = from; i < std::min<int>(from + m_pageSize, paths.size()); ++i)
            entries.append(QJsonObject{{u".tag"_s, u"file"_s},
                                       {u"path_display"_s, paths.at(i)},
                                       {u"rev"_s, m_files.value(paths.at(i)).rev}});
        entries.append(QJsonObject{{u".tag"_s, u"folder"_s}, {u"path_display"_s, u"/Notes/Sub"_s}});
        const bool more = from + m_pageSize < paths.size();
        return {200, json({{u"entries"_s, entries},
                           {u"has_more"_s, more},
                           {u"cursor"_s, QString::number(from + m_pageSize)}})};
    }
    if (request.target == "/2/files/download") {
        if (!m_files.contains(path))
            return {409, json({{u"error_summary"_s, u"path/not_found/"_s}})};
        return {200,
                m_files.value(path).data,
                {{"Dropbox-API-Result", json({{u"rev"_s, m_files.value(path).rev}})}},
                "application/octet-stream"};
    }
    if (request.target == "/2/files/upload") {
        const QJsonValue mode = args.value(u"mode"_s);
        const QString expected =
            mode.isObject() ? mode.toObject().value(u"update"_s).toString() : QString();
        const QString there = m_files.value(path).rev;
        if (there != expected)
            return {409, json({{u"error_summary"_s, u"path/conflict/file/"_s}})};
        const QString rev = u"r%1"_s.arg(++m_revs);
        m_files.insert(path, {request.body, rev});
        return {200, json({{u"rev"_s, rev}, {u"path_display"_s, path}})};
    }
    if (request.target == "/2/files/delete_v2") {
        if (!m_files.contains(path) ||
            m_files.value(path).rev != args.value(u"parent_rev"_s).toString())
            return {409, json({{u"error_summary"_s, u"path_lookup/not_found/"_s}})};
        m_files.remove(path);
        return {200, json({{u"metadata"_s, QJsonObject{{u"path_display"_s, path}}}})};
    }
    return {404, "{}"};
}

void TestDropbox::signsIn()
{
    const jot::DropboxSignIn signIn = jot::DropboxSignIn::start(u"key"_s, api());
    const QUrlQuery query(signIn.url);
    QCOMPARE(query.queryItemValue(u"client_id"_s), u"key"_s);
    QCOMPARE(query.queryItemValue(u"token_access_type"_s), u"offline"_s);
    QCOMPARE(query.queryItemValue(u"code_challenge_method"_s), u"S256"_s);
    QVERIFY(!query.queryItemValue(u"code_challenge"_s).isEmpty());
    QVERIFY(!query.hasQueryItem(u"redirect_uri"_s));
    QVERIFY(signIn.verifier.size() >= 43);
    QString error;
    QCOMPARE(signIn.finish(u"key"_s, u" good \n"_s, &error, api()), std::optional(u"refresh"_s));
    QVERIFY(!signIn.finish(u"key"_s, u"bad"_s, &error, api()));
    QCOMPARE(error, u"code doesn't exist"_s);
}

void TestDropbox::listsPageByPage()
{
    for (int i = 0; i < 5; ++i)
        m_files.insert(u"/Notes/n%1.md"_s.arg(i), {"x", u"r%1"_s.arg(i)});
    m_files.insert(u"/Notes/.jot/trash/old.md"_s, {"gone", u"t"_s});
    m_pageSize = 2;
    const std::unique_ptr<DropboxTransport> owned = transport();
    DropboxTransport &dropbox = *owned;
    QString error;
    const std::optional<QList<jot::RemoteFile>> files = dropbox.list(&error);
    QVERIFY2(files, qPrintable(error));
    QCOMPARE(files->size(), 5);
    QCOMPARE(files->first().path, u"n0.md"_s);
    QCOMPARE(files->first().rev, u"r0"_s);
    // A folder not made yet is an empty one, not a failure.
    DropboxTransport elsewhere(u"key"_s, u"refresh"_s, u"/Missing"_s, api());
    m_server->replier = [](const FakeHttpServer::Request &request) -> FakeHttpServer::Reply {
        if (request.target == "/oauth2/token")
            return {200, R"({"access_token":"access"})"};
        return {409, R"({"error_summary":"path/not_found/"})"};
    };
    QCOMPARE(elsewhere.list(&error)->size(), 0);
}

void TestDropbox::writesOnlyOverTheRevisionExpected()
{
    const std::unique_ptr<DropboxTransport> owned = transport();
    DropboxTransport &dropbox = *owned;
    QString error;
    bool stale = false;
    const std::optional<QString> first = dropbox.put(u"Mara.md"_s, "one\n", {}, &stale, &error);
    QVERIFY2(first, qPrintable(error));
    QVERIFY(!dropbox.put(u"Mara.md"_s, "two\n", {}, &stale, &error));
    QVERIFY(stale);
    const std::optional<QString> second =
        dropbox.put(u"Mara.md"_s, "two\n", *first, &stale, &error);
    QVERIFY(second);
    QString rev;
    QCOMPARE(dropbox.get(u"Mara.md"_s, &rev, &error), std::optional(QByteArray("two\n")));
    QCOMPARE(rev, *second);
    stale = false;
    QVERIFY(!dropbox.remove(u"Mara.md"_s, *first, &stale, &error));
    QVERIFY(stale);
    QVERIFY(dropbox.remove(u"Mara.md"_s, *second, &stale, &error));
    QVERIFY(m_files.isEmpty());
    // Only one access token, got once.
    int tokens = 0;
    for (const FakeHttpServer::Request &request : std::as_const(m_server->requests))
        tokens += request.target == "/oauth2/token";
    QCOMPARE(tokens, 1);
}

void TestDropbox::sendsNamesPastAsciiSafely()
{
    const std::unique_ptr<DropboxTransport> owned = transport();
    DropboxTransport &dropbox = *owned;
    QString error;
    bool stale = false;
    QVERIFY(dropbox.put(u"Café/Crème.md"_s, "x", {}, &stale, &error));
    const QByteArray header = m_server->requests.last().headers.value("dropbox-api-arg");
    QVERIFY2(header.contains("Caf\\u00e9"), header.constData());
    QVERIFY(m_files.contains(u"/Notes/Café/Crème.md"_s));
}

void TestDropbox::syncsTwoComputers()
{
    QTemporaryDir dir;
    const auto engine = [&](const QString &name) {
        QDir().mkpath(dir.filePath(name + u"/vault"_s));
        return std::make_unique<jot::SyncEngine>(dir.filePath(name + u"/vault"_s), transport(),
                                                 dir.filePath(name + u"/state"_s), name);
    };
    auto laptop = engine(u"laptop"_s);
    auto desk = engine(u"desk"_s);
    QFile note(dir.filePath(u"laptop/vault/Mara.md"_s));
    QVERIFY(note.open(QIODevice::WriteOnly));
    note.write("# Mara\n");
    note.close();
    QVERIFY(laptop->sync().failure.isEmpty());
    QVERIFY(desk->sync().failure.isEmpty());
    QFile there(dir.filePath(u"desk/vault/Mara.md"_s));
    QVERIFY(there.open(QIODevice::ReadOnly));
    QCOMPARE(there.readAll(), QByteArray("# Mara\n"));
}

QTEST_GUILESS_MAIN(TestDropbox)
#include "tst_dropbox.moc"
