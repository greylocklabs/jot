#include "FakeHttpServer.h"

#include "jot/DriveTransport.h"
#include "jot/Sync.h"

#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QUrlQuery>

using namespace Qt::StringLiterals;
using jot::DriveApi;
using jot::DriveTransport;

/// A Google Drive held in memory, answering the calls Jot makes as Drive does.
class TestDrive : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void init();
    void signsInThroughTheBrowser();
    void findsOrMakesItsFolder();
    void keepsFilesInFoldersByTheirPaths();
    void writesOnlyOverTheVersionExpected();
    void syncsTwoComputers();
    void makesNothingTwiceUnderOneName();

private:
    struct Item
    {
        QString name;
        QString parent;
        bool folder = false;
        QByteArray data;
        qint64 version = 1;
        bool trashed = false;
    };
    FakeHttpServer::Reply answer(const FakeHttpServer::Request &request);
    DriveApi api() const
    {
        return {m_server->url({}), m_server->url(u"/token"_s), m_server->url(u"/auth"_s)};
    }
    std::unique_ptr<DriveTransport> drive() const
    {
        return std::make_unique<DriveTransport>(u"id"_s, u"secret"_s, u"refresh"_s, u"top"_s,
                                                api());
    }

    std::unique_ptr<FakeHttpServer> m_server;
    QMap<QString, Item> m_items;
    int m_ids = 0;
};

void TestDrive::init()
{
    m_server = std::make_unique<FakeHttpServer>();
    m_server->replier = [this](const FakeHttpServer::Request &request) { return answer(request); };
    m_items.clear();
    m_items.insert(u"top"_s, {u"Jot"_s, u"root"_s, true, {}});
}

FakeHttpServer::Reply TestDrive::answer(const FakeHttpServer::Request &request)
{
    const auto json = [](const QJsonValue &value) {
        return value.isObject() ? QJsonDocument(value.toObject()).toJson(QJsonDocument::Compact)
                                : QByteArray();
    };
    const QUrl url(QString::fromLatin1(request.target));
    const QUrlQuery query(url);
    const QString path = url.path();
    if (path == u"/token") {
        const QUrlQuery form(QString::fromUtf8(request.body));
        if (form.queryItemValue(u"grant_type"_s) == u"refresh_token")
            return {200, json(QJsonObject{{u"access_token"_s, u"access"_s}})};
        if (form.queryItemValue(u"code"_s) == u"good" &&
            !form.queryItemValue(u"code_verifier"_s).isEmpty())
            return {200, json(QJsonObject{{u"refresh_token"_s, u"refresh"_s}})};
        return {400, json(QJsonObject{{u"error"_s, u"invalid_grant"_s}})};
    }
    if (request.headers.value("authorization") != "Bearer access")
        return {401, json(QJsonObject{{u"error"_s, QJsonObject{{u"message"_s, u"no"_s}}}})};
    const auto described = [this](const QString &id) {
        const Item &item = m_items[id];
        return QJsonObject{{u"id"_s, id},
                           {u"name"_s, item.name},
                           {u"mimeType"_s, item.folder
                                               ? QString::fromLatin1(DriveTransport::kFolderType)
                                               : u"text/plain"_s},
                           {u"version"_s, QString::number(item.version)},
                           {u"trashed"_s, item.trashed}};
    };
    static const QRegularExpression parentIs(u"'([^']+)' in parents"_s);
    static const QRegularExpression nameIs(u"name = '([^']+)'"_s);
    if (request.method == "GET" && path == u"/drive/v3/files") {
        const QString q = query.queryItemValue(u"q"_s, QUrl::FullyDecoded);
        const QString parent = parentIs.match(q).captured(1);
        const QString name = nameIs.match(q).captured(1);
        QJsonArray files;
        for (auto it = m_items.cbegin(); it != m_items.cend(); ++it) {
            const bool kind = !q.contains(u"mimeType"_s) ||
                              (q.contains(u"mimeType != "_s) ? !it->folder : it->folder);
            if (it->parent == parent && !it->trashed && kind &&
                (name.isEmpty() || it->name == name))
                files.append(described(it.key()));
        }
        return {200, json(QJsonObject{{u"files"_s, files}})};
    }
    static const QRegularExpression one(u"^/drive/v3/files/([^/]+)$"_s);
    if (const QRegularExpressionMatch m = one.match(path); m.hasMatch()) {
        const QString id = m.captured(1);
        if (!m_items.contains(id))
            return {404, "{}"};
        if (request.method == "GET" && query.queryItemValue(u"alt"_s) == u"media")
            return {200, m_items[id].data, {}, "application/octet-stream"};
        if (request.method == "GET")
            return {200, json(described(id))};
        if (request.method == "PATCH") {
            m_items[id].trashed =
                QJsonDocument::fromJson(request.body).object().value(u"trashed"_s).toBool();
            return {200, json(described(id))};
        }
    }
    if (request.method == "POST" && path == u"/drive/v3/files") {
        const QJsonObject metadata = QJsonDocument::fromJson(request.body).object();
        const QString id = u"f%1"_s.arg(++m_ids);
        m_items.insert(id, {metadata.value(u"name"_s).toString(),
                            metadata.value(u"parents"_s).toArray().first().toString(),
                            true,
                            {}});
        return {200, json(described(id))};
    }
    if (request.method == "POST" && path == u"/upload/drive/v3/files") {
        // The metadata part, then the bytes, between the boundaries.
        const QByteArray boundary = "--" + request.headers.value("content-type").split('=').last();
        const qsizetype meta = request.body.indexOf("\r\n\r\n") + 4;
        const QJsonObject metadata =
            QJsonDocument::fromJson(
                request.body.mid(meta, request.body.indexOf("\r\n--", meta) - meta))
                .object();
        const qsizetype second =
            request.body.indexOf("\r\n\r\n", request.body.indexOf(boundary, meta)) + 4;
        const QByteArray data =
            request.body.mid(second, request.body.lastIndexOf("\r\n" + boundary) - second);
        const QString id = u"f%1"_s.arg(++m_ids);
        m_items.insert(id,
                       {metadata.value(u"name"_s).toString(),
                        metadata.value(u"parents"_s).toArray().first().toString(), false, data});
        return {200, json(described(id))};
    }
    static const QRegularExpression upload(u"^/upload/drive/v3/files/([^/]+)$"_s);
    if (const QRegularExpressionMatch m = upload.match(path);
        m.hasMatch() && request.method == "PATCH") {
        Item &item = m_items[m.captured(1)];
        item.data = request.body;
        ++item.version;
        return {200, json(described(m.captured(1)))};
    }
    return {404, "{}"};
}

void TestDrive::signsInThroughTheBrowser()
{
    jot::DriveSignIn signIn(api());
    const QUrl url = signIn.start(u"id"_s);
    const QUrlQuery query(url);
    QCOMPARE(query.queryItemValue(u"scope"_s, QUrl::FullyDecoded),
             u"https://www.googleapis.com/auth/drive.file"_s);
    QCOMPARE(query.queryItemValue(u"code_challenge_method"_s), u"S256"_s);
    const QUrl redirect(query.queryItemValue(u"redirect_uri"_s, QUrl::FullyDecoded));
    QCOMPARE(redirect.host(), u"127.0.0.1"_s);
    // The browser comes back with the code, as Google sends it.
    QNetworkAccessManager browser;
    QUrl back = redirect;
    back.setQuery(u"state="_s + query.queryItemValue(u"state"_s) + u"&code=good"_s);
    QTimer::singleShot(50, &browser, [&] { browser.get(QNetworkRequest(back)); });
    QString error;
    QCOMPARE(signIn.finish(u"id"_s, u"secret"_s, 5000, &error), std::optional(u"refresh"_s));
    // Nobody coming back fails rather than waiting for ever.
    jot::DriveSignIn late(api());
    late.start(u"id"_s);
    QVERIFY(!late.finish(u"id"_s, u"secret"_s, 100, &error));
    QVERIFY(!error.isEmpty());
    // Nor when the app quits meanwhile: it stops at once, not when the wait runs out.
    jot::DriveSignIn quitting(api());
    quitting.start(u"id"_s);
    std::atomic<bool> cancel = false;
    QTimer::singleShot(50, [&cancel] { cancel = true; });
    QElapsedTimer took;
    took.start();
    QVERIFY(!quitting.finish(u"id"_s, u"secret"_s, 60000, &error, &cancel));
    QVERIFY(took.elapsed() < 5000);
}

void TestDrive::findsOrMakesItsFolder()
{
    m_items.clear();
    const std::unique_ptr<DriveTransport> transport = drive();
    QString error;
    const std::optional<QString> made = transport->folderNamed(u"Jot"_s, &error);
    QVERIFY2(made, qPrintable(error));
    QCOMPARE(m_items.value(*made).parent, u"root"_s);
    // Another computer finds the same one.
    QCOMPARE(drive()->folderNamed(u"Jot"_s, &error), made);
    QCOMPARE(m_items.size(), 1);
}

void TestDrive::keepsFilesInFoldersByTheirPaths()
{
    const std::unique_ptr<DriveTransport> transport = drive();
    QString error;
    bool stale = false;
    QVERIFY(
        transport->put(u"Ashfall/Characters/Mara's notes.md"_s, "# Mara\n", {}, &stale, &error));
    QVERIFY(transport->put(u"Ashfall/Pitch.md"_s, "pitch\n", {}, &stale, &error));
    // One folder for Ashfall, made once.
    int folders = 0;
    for (const Item &item : std::as_const(m_items))
        folders += item.folder && item.name == u"Ashfall";
    QCOMPARE(folders, 1);
    const std::optional<QList<jot::RemoteFile>> files = drive()->list(&error);
    QVERIFY2(files, qPrintable(error));
    QStringList paths;
    for (const jot::RemoteFile &file : *files)
        paths << file.path;
    paths.sort();
    QCOMPARE(paths, (QStringList{u"Ashfall/Characters/Mara's notes.md"_s, u"Ashfall/Pitch.md"_s}));
    QString rev;
    QCOMPARE(drive()->get(u"Ashfall/Pitch.md"_s, &rev, &error),
             std::optional(QByteArray("pitch\n")));
    QCOMPARE(rev, u"1"_s);
}

void TestDrive::writesOnlyOverTheVersionExpected()
{
    const std::unique_ptr<DriveTransport> transport = drive();
    QString error;
    bool stale = false;
    const std::optional<QString> first = transport->put(u"Mara.md"_s, "one\n", {}, &stale, &error);
    QVERIFY(first);
    QVERIFY(!transport->put(u"Mara.md"_s, "two\n", {}, &stale, &error));
    QVERIFY(stale);
    stale = false;
    QVERIFY(!transport->put(u"Mara.md"_s, "two\n", u"9"_s, &stale, &error));
    QVERIFY(stale);
    const std::optional<QString> second =
        transport->put(u"Mara.md"_s, "two\n", *first, &stale, &error);
    QCOMPARE(second, std::optional(u"2"_s));
    stale = false;
    QVERIFY(!transport->remove(u"Mara.md"_s, *first, &stale, &error));
    QVERIFY(stale);
    QVERIFY(transport->remove(u"Mara.md"_s, *second, &stale, &error));
    // In Drive's trash, not gone.
    bool trashed = false;
    for (const Item &item : std::as_const(m_items))
        trashed = trashed || (item.name == u"Mara.md" && item.trashed);
    QVERIFY(trashed);
    QVERIFY(drive()->list(&error)->isEmpty());
}

void TestDrive::syncsTwoComputers()
{
    QTemporaryDir dir;
    const auto engine = [&](const QString &name) {
        QDir().mkpath(dir.filePath(name + u"/vault"_s));
        return std::make_unique<jot::SyncEngine>(dir.filePath(name + u"/vault"_s), drive(),
                                                 dir.filePath(name + u"/state"_s), name);
    };
    auto laptop = engine(u"laptop"_s);
    auto desk = engine(u"desk"_s);
    QDir().mkpath(dir.filePath(u"laptop/vault/Ashfall"_s));
    QFile note(dir.filePath(u"laptop/vault/Ashfall/Mara.md"_s));
    QVERIFY(note.open(QIODevice::WriteOnly));
    note.write("# Mara\n");
    note.close();
    QVERIFY(laptop->sync().failure.isEmpty());
    QVERIFY(desk->sync().failure.isEmpty());
    QFile there(dir.filePath(u"desk/vault/Ashfall/Mara.md"_s));
    QVERIFY(there.open(QIODevice::ReadOnly));
    QCOMPARE(there.readAll(), QByteArray("# Mara\n"));
}

void TestDrive::makesNothingTwiceUnderOneName()
{
    // One computer lists; another then makes a note, and a folder with one in it.
    const std::unique_ptr<DriveTransport> first = drive();
    const std::unique_ptr<DriveTransport> second = drive();
    QString error;
    QVERIFY(first->list(&error));
    bool stale = false;
    QVERIFY(second->put(u"Mara.md"_s, "theirs\n", {}, &stale, &error));
    QVERIFY(second->put(u"Ashfall/Pitch.md"_s, "pitch\n", {}, &stale, &error));
    // The first does not make the note again beside it, nor a second Ashfall.
    QVERIFY(!first->put(u"Mara.md"_s, "mine\n", {}, &stale, &error));
    QVERIFY(stale);
    QVERIFY(first->put(u"Ashfall/Crossings.md"_s, "x\n", {}, &stale, &error));
    int maras = 0;
    int ashfalls = 0;
    for (const Item &item : std::as_const(m_items)) {
        maras += item.name == u"Mara.md";
        ashfalls += item.folder && item.name == u"Ashfall";
    }
    QCOMPARE(maras, 1);
    QCOMPARE(ashfalls, 1);
}

QTEST_GUILESS_MAIN(TestDrive)
#include "tst_drive.moc"
