#include "JexBuilder.h"
#include "JoplinDatabaseBuilder.h"

#include "jot/JoplinExport.h"
#include "jot/JoplinImport.h"
#include "jot/JoplinPatch.h"
#include "jot/NoteHistory.h"
#include "jot/Vault.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>
#include <QTimeZone>

using namespace jot;
using namespace Qt::StringLiterals;

namespace {

const QString kTravel = u"0123456789abcdef0123456789abcdef"_s;
const QString kPlaces = u"1123456789abcdef0123456789abcdef"_s;
const QString kPacking = u"fedcba9876543210fedcba9876543210"_s;
const QString kPhoto = u"aa23456789abcdef0123456789abcdef"_s;
const QString kPlain = u"bb23456789abcdef0123456789abcdef"_s;
const QString kGone = u"cc23456789abcdef0123456789abcdef"_s;
const QString kTag = u"dd23456789abcdef0123456789abcdef"_s;

// 2026-10-12 09:15:00 UTC, and an hour on.
constexpr qint64 kNine = 1791796500000;
constexpr qint64 kTen = kNine + 3600000;

/// One patch as Joplin stores it: at `at`, `lead` kept, `before` made `after`, `trail` kept.
QString patch(qsizetype at, const QString &lead, const QString &before, const QString &after,
              const QString &trail = {})
{
    const auto json = [](QString s) {
        return u'"' +
               s.replace(u'\\', u"\\\\"_s).replace(u'"', u"\\\""_s).replace(u'\n', u"\\n"_s) + u'"';
    };
    QStringList diffs;
    for (const auto &[op, part] : {std::pair{0, lead}, {-1, before}, {1, after}, {0, trail}}) {
        if (!part.isEmpty())
            diffs.append(u"[%1,%2]"_s.arg(op).arg(json(part)));
    }
    return u"{\"diffs\":[%1],\"start1\":%2,\"start2\":%2,\"length1\":%3,\"length2\":%4}"_s
        .arg(diffs.join(u','))
        .arg(at)
        .arg(lead.size() + before.size() + trail.size())
        .arg(lead.size() + after.size() + trail.size());
}

/// Patches as a revision holds them.
QString patches(const QStringList &each)
{
    return u'[' + each.join(u',') + u']';
}

/// A revision of `note` from `time`, its title and body patched by `titleDiff` and `bodyDiff`.
QString revision(const QString &id, const QString &note, qint64 time, const QString &titleDiff,
                 const QString &bodyDiff)
{
    const auto sql = [](QString s) { return u'\'' + s.replace(u'\'', u"''"_s) + u'\''; };
    return u"INSERT INTO revisions (id, item_id, item_updated_time, title_diff, body_diff) "
           "VALUES (%1, %2, %3, %4, %5)"_s.arg(sql(id), sql(note))
               .arg(time)
               .arg(sql(titleDiff), sql(bodyDiff));
}

} // namespace

class TestJoplinDatabase : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void init();

    void readsWhatAnExportWouldHold();
    void leavesOutWhatCannotBeRead();
    void importsLikeAnExport();
    void tellsADatabaseFromAnExport();
    void findsJoplinOnThisComputer();
    void appliesJoplinsPatches();
    void piecesRevisionsTogether();
    void keepsRevisionsAsPastVersions();

private:
    QString profile(const QString &name, const QStringList &rows);

    std::unique_ptr<QTemporaryDir> m_dir;
};

void TestJoplinDatabase::init()
{
    m_dir = std::make_unique<QTemporaryDir>();
}

QString TestJoplinDatabase::profile(const QString &name, const QStringList &rows)
{
    const QString folder = m_dir->filePath(name);
    if (!QDir().mkpath(folder + u"/resources"_s))
        return {};
    const QString database = folder + u"/database.sqlite"_s;
    const QString error = makeJoplinDatabase(database, rows);
    if (!error.isEmpty())
        qWarning() << error;
    return error.isEmpty() ? database : QString();
}

void TestJoplinDatabase::readsWhatAnExportWouldHold()
{
    const QString database = profile(
        u"joplin-desktop"_s,
        {
            u"INSERT INTO folders (id, title, icon) VALUES ('%1', 'Travel', "
            "'{\"emoji\":\"🧳\",\"name\":\"luggage\",\"type\":1}')"_s.arg(kTravel),
            u"INSERT INTO folders (id, parent_id, title) VALUES ('%1', '%2', 'Places')"_s.arg(
                kPlaces, kTravel),
            u"INSERT INTO notes (id, parent_id, title, body, user_created_time, "
            "user_updated_time, is_todo, todo_due, latitude, author) VALUES ('%1', '%2', "
            "'Packing list', '- [ ] passport', %3, %4, 1, %4, 40.6, 'Mara')"_s
                .arg(kPacking, kPlaces)
                .arg(kNine)
                .arg(kTen),
            u"INSERT INTO tags (id, title) VALUES ('%1', 'trips')"_s.arg(kTag),
            u"INSERT INTO note_tags (note_id, tag_id) VALUES ('%1', '%2')"_s.arg(kPacking, kTag),
            u"INSERT INTO resources (id, title, mime, filename, file_extension) VALUES ('%1', "
            "'Beach', 'image/png', 'beach.png', 'png')"_s.arg(kPhoto),
            u"INSERT INTO resources (id, title, mime) VALUES ('%1', 'Notes', 'text/plain')"_s.arg(
                kPlain),
            u"INSERT INTO resources (id, title, mime, file_extension) VALUES ('%1', 'Lost', "
            "'image/png', 'png')"_s.arg(kGone),
        });
    QVERIFY(!database.isEmpty());
    const QDir resources(QFileInfo(database).dir().filePath(u"resources"_s));
    for (const auto &[name, data] : {std::pair{kPhoto + u".png"_s, QByteArray("png bytes")},
                                     std::pair{kPlain, QByteArray("plain bytes")}}) {
        QFile file(resources.filePath(name));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(data);
    }

    QString error;
    const std::optional<JoplinExport> read = readJoplinDatabase(database, &error);
    QVERIFY2(read, qPrintable(error));
    QCOMPARE(read->notebooks.size(), 2);
    QCOMPARE(read->notebooks.at(0).emoji, u"🧳"_s);
    QCOMPARE(read->notebooks.at(1).parentId, kTravel);

    QCOMPARE(read->notes.size(), 1);
    const JoplinExport::Note &note = read->notes.first();
    QCOMPARE(note.notebookId, kPlaces);
    QCOMPARE(note.title, u"Packing list"_s);
    QCOMPARE(note.body, u"- [ ] passport"_s);
    QCOMPARE(note.created, QDateTime::fromMSecsSinceEpoch(kNine, QTimeZone::UTC));
    QCOMPARE(note.updated, QDateTime::fromMSecsSinceEpoch(kTen, QTimeZone::UTC));
    QVERIFY(note.isTodo);
    QCOMPARE(note.due, QDateTime::fromMSecsSinceEpoch(kTen, QTimeZone::UTC));
    QVERIFY(!note.completed.isValid());
    QCOMPARE(note.author, u"Mara"_s);
    QVERIFY(!note.html && !note.conflict && !note.deleted);
    QCOMPARE(read->noteTags.value(kPacking), QStringList{kTag});
    QCOMPARE(read->tags.first().title, u"trips"_s);

    // Its file is <id>.<extension>, or <id> alone without one, and is not read until copied in;
    // a file not there is none.
    QCOMPARE(read->resources.size(), 3);
    QCOMPARE(read->resources.at(0).file, resources.filePath(kPhoto + u".png"_s));
    QVERIFY(read->resources.at(0).data.isNull());
    QCOMPARE(read->resources.at(0).fileName, u"beach.png"_s);
    QCOMPARE(read->resources.at(1).file, resources.filePath(kPlain));
    QVERIFY(read->resources.at(2).file.isEmpty());
    QVERIFY(read->skipped.isEmpty());
}

void TestJoplinDatabase::leavesOutWhatCannotBeRead()
{
    const QString database = profile(
        u"joplin-desktop"_s,
        {
            u"INSERT INTO notes (id, title, encryption_applied) VALUES ('%1', '', 1)"_s.arg(
                kPacking),
            u"INSERT INTO notes (id, title, deleted_time) VALUES ('%1', 'Old', %2)"_s.arg(kPlain)
                .arg(kNine),
            u"INSERT INTO resources (id, title, encryption_blob_encrypted) VALUES ('%1', 'x', 1)"_s
                .arg(kPhoto),
            // Encrypted details and file both: one item, counted once.
            u"INSERT INTO resources (id, title, encryption_applied, encryption_blob_encrypted) "
            "VALUES ('%1', '', 1, 1)"_s.arg(kGone),
        });
    QVERIFY(!database.isEmpty());
    QString error;
    const std::optional<JoplinExport> read = readJoplinDatabase(database, &error);
    QVERIFY2(read, qPrintable(error));
    QCOMPARE(read->notes.size(), 1);
    QVERIFY(read->notes.first().deleted);
    QVERIFY(read->resources.isEmpty());
    QCOMPARE(read->skipped, QStringList{u"3 encrypted items, which Joplin has to decrypt first"_s});
}

void TestJoplinDatabase::importsLikeAnExport()
{
    const QString database = profile(
        u"joplin-desktop"_s,
        {
            u"INSERT INTO folders (id, title) VALUES ('%1', 'Travel')"_s.arg(kTravel),
            u"INSERT INTO notes (id, parent_id, title, body, user_updated_time) VALUES ('%1', "
            "'%2', 'Packing list', '![beach](:/%3)', %4)"_s.arg(kPacking, kTravel, kPhoto)
                .arg(kTen),
            u"INSERT INTO resources (id, title, mime, filename, file_extension) VALUES ('%1', "
            "'Beach', 'image/png', 'beach.png', 'png')"_s.arg(kPhoto),
        });
    QVERIFY(!database.isEmpty());
    QFile photo(QFileInfo(database).dir().filePath(u"resources/"_s + kPhoto + u".png"_s));
    QVERIFY(photo.open(QIODevice::WriteOnly));
    photo.write("png bytes");
    photo.close();

    const Vault vault(m_dir->filePath(u"Notes"_s));
    QVERIFY(QDir().mkpath(vault.root()));
    QString error;
    const std::optional<JoplinExport> read = readJoplin(database, &error);
    QVERIFY2(read, qPrintable(error));
    const std::optional<JoplinImport> done = importJoplin(vault, *read, {}, false, &error);
    QVERIFY2(done, qPrintable(error));
    QCOMPARE(done->count(JoplinImport::Outcome::Created), 1);
    QCOMPARE(done->attachments, 1);
    const std::optional<QString> text =
        notefile::read(vault.absolute(u"Travel/Packing list.md"_s), &error);
    QVERIFY2(text, qPrintable(error));
    QVERIFY2(text->contains(u"![beach](Packing%20list.assets/beach.png)"_s), qPrintable(*text));
}

void TestJoplinDatabase::tellsADatabaseFromAnExport()
{
    const QString database =
        profile(u"joplin-desktop"_s, {u"INSERT INTO folders (id, title) VALUES ('%1', "
                                      "'Travel')"_s.arg(kTravel)});
    QVERIFY(!database.isEmpty());
    QString error;
    // The profile folder, or the database in it, whatever it is called.
    QVERIFY2(readJoplin(QFileInfo(database).path(), &error), qPrintable(error));
    const QString renamed = m_dir->filePath(u"backup.bin"_s);
    QVERIFY(QFile::copy(database, renamed));
    QVERIFY2(readJoplin(renamed, &error), qPrintable(error));

    // A JEX export, as before.
    const QString jex = m_dir->filePath(u"export.jex"_s);
    QFile file(jex);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(tar({{kTravel + u".md"_s,
                     item(u"Travel"_s, {}, u"id: "_s + kTravel + u"\nparent_id: \ntype_: 2"_s)}}));
    file.close();
    const std::optional<JoplinExport> fromJex = readJoplin(jex, &error);
    QVERIFY2(fromJex, qPrintable(error));
    QCOMPARE(fromJex->notebooks.size(), 1);

    // Another program's database, and none at all, are refused.
    QSqlDatabase other = QSqlDatabase::addDatabase(u"QSQLITE"_s, u"other"_s);
    other.setDatabaseName(m_dir->filePath(u"other.sqlite"_s));
    QVERIFY(other.open());
    QVERIFY(QSqlQuery(other).exec(u"CREATE TABLE things (id TEXT)"_s));
    other.close();
    other = {};
    QSqlDatabase::removeDatabase(u"other"_s);
    QVERIFY(!readJoplin(m_dir->filePath(u"other.sqlite"_s), &error));
    QVERIFY2(error.contains(u"not a Joplin database"_s), qPrintable(error));
    QVERIFY(!readJoplinDatabase(m_dir->filePath(u"none/database.sqlite"_s), &error));
    QVERIFY(!readJoplin(m_dir->filePath(u"empty"_s), &error));
}

void TestJoplinDatabase::findsJoplinOnThisComputer()
{
    // Never the real one: home and the config folder are this test's own.
    qputenv("HOME", m_dir->filePath(u"home"_s).toUtf8());
    qputenv("XDG_CONFIG_HOME", m_dir->filePath(u"home/.config"_s).toUtf8());
    QVERIFY(joplinProfileDatabase().isEmpty());

    const QString flatpak = m_dir->filePath(
        u"home/.var/app/net.cozic.joplin_desktop/config/joplin-desktop/database.sqlite"_s);
    QVERIFY(QDir().mkpath(QFileInfo(flatpak).path()));
    QVERIFY(makeJoplinDatabase(flatpak, {}).isEmpty());
    QCOMPARE(joplinProfileDatabase(), flatpak);

    const QString package = m_dir->filePath(u"home/.config/joplin-desktop/database.sqlite"_s);
    QVERIFY(QDir().mkpath(QFileInfo(package).path()));
    QVERIFY(makeJoplinDatabase(package, {}).isEmpty());
    QCOMPARE(joplinProfileDatabase(), package);
}

void TestJoplinDatabase::appliesJoplinsPatches()
{
    QCOMPARE(applyJoplinPatch(u"same"_s, {}), u"same"_s);
    const auto hello = applyJoplinPatch({}, patches({patch(0, {}, {}, u"Hello world\nBye"_s)}));
    QCOMPARE(hello, u"Hello world\nBye"_s);
    QCOMPARE(
        applyJoplinPatch(*hello, patches({patch(2, u"llo "_s, u"world"_s, u"Jot"_s, u"\nB"_s)})),
        u"Hello Jot\nBye"_s);

    // Two in one: the second's place counts the first's change, as Joplin's do.
    QCOMPARE(
        applyJoplinPatch(u"one two three"_s, patches({patch(0, {}, u"one"_s, u"1"_s, u" two"_s),
                                                      patch(5, u" "_s, u"three"_s, u"3"_s)})),
        u"1 two 3"_s);
    // One that went elsewhere moves where the next is looked for: of the two cats, the first.
    QCOMPARE(applyJoplinPatch(u"zzzz cat. cat."_s, patches({patch(4, {}, u"zzzz"_s, {}),
                                                            patch(6, {}, u"cat"_s, u"dog"_s)})),
             u" dog. cat."_s);
    // Not where it says, as when an earlier revision is gone: the nearest place it fits.
    QCOMPARE(applyJoplinPatch(u"intro. Hello world"_s,
                              patches({patch(2, u"llo "_s, u"world"_s, u"Jot"_s)})),
             u"intro. Hello Jot"_s);

    QVERIFY(!applyJoplinPatch(u"Hello"_s, patches({patch(0, {}, u"world"_s, u"Jot"_s)})));
    QVERIFY(!applyJoplinPatch(u"Hello"_s, u"@@ -1,5 +1,3 @@"_s));
    QVERIFY(!applyJoplinPatch(u"Hello"_s, u"[{\"diffs\":[[7,\"x\"]],\"start2\":0}]"_s));
}

void TestJoplinDatabase::piecesRevisionsTogether()
{
    // Out of order in the table, as rows can be: they are pieced together by time.
    const QString database = profile(
        u"joplin-desktop"_s,
        {
            u"INSERT INTO notes (id, title, body) VALUES ('%1', 'Packing list', 'socks, hat')"_s
                .arg(kPacking),
            revision(u"r2"_s, kPacking, kTen, {}, patches({patch(0, u"socks"_s, {}, u", hat"_s)})),
            revision(u"r1"_s, kPacking, kNine, patches({patch(0, {}, {}, u"Packing"_s)}),
                     patches({patch(0, {}, {}, u"socks"_s)})),
            // A revision of a note Joplin no longer has.
            revision(u"r0"_s, kPlain, kNine, {}, patches({patch(0, {}, {}, u"gone"_s)})),
        });
    QVERIFY(!database.isEmpty());
    QString error;
    std::optional<JoplinExport> read = readJoplinDatabase(database, &error);
    QVERIFY2(read, qPrintable(error));
    const QList<JoplinExport::Revision> revisions = read->notes.first().revisions;
    QCOMPARE(revisions.size(), 2);
    QCOMPARE(revisions.at(0).updated, QDateTime::fromMSecsSinceEpoch(kNine, QTimeZone::UTC));
    QCOMPARE(revisions.at(0).title, u"Packing"_s);
    QCOMPARE(revisions.at(0).body, u"socks"_s);
    QCOMPARE(revisions.at(1).title, u"Packing"_s);
    QCOMPARE(revisions.at(1).body, u"socks, hat"_s);
    QVERIFY(read->skipped.isEmpty());

    // One that will not apply ends the note's revisions there, and says so.
    const QString broken = profile(
        u"broken"_s,
        {
            u"INSERT INTO notes (id, title, body) VALUES ('%1', 'Packing list', 'x')"_s.arg(
                kPacking),
            revision(u"r1"_s, kPacking, kNine, {}, patches({patch(0, {}, {}, u"socks"_s)})),
            revision(u"r2"_s, kPacking, kTen, {}, patches({patch(0, {}, u"boots"_s, u"hat"_s)})),
            revision(u"r3"_s, kPacking, kTen + 1000, {},
                     patches({patch(0, {}, u"socks"_s, u"boots"_s)})),
        });
    QVERIFY(!broken.isEmpty());
    read = readJoplinDatabase(broken, &error);
    QVERIFY2(read, qPrintable(error));
    QCOMPARE(read->notes.first().revisions.size(), 1);
    QCOMPARE(read->skipped, QStringList{u"2 note revisions that could not be pieced together"_s});
}

void TestJoplinDatabase::keepsRevisionsAsPastVersions()
{
    const QString database = profile(
        u"joplin-desktop"_s,
        {
            u"INSERT INTO folders (id, title) VALUES ('%1', 'Travel')"_s.arg(kTravel),
            u"INSERT INTO notes (id, parent_id, title, body, user_created_time, "
            "user_updated_time) VALUES ('%1', '%2', 'Packing list', '![beach](:/%3) socks, hat', "
            "%4, %5)"_s.arg(kPacking, kTravel, kPhoto)
                .arg(kNine)
                .arg(kTen + 60000),
            u"INSERT INTO resources (id, title, mime, filename, file_extension) VALUES ('%1', "
            "'Beach', 'image/png', 'beach.png', 'png')"_s.arg(kPhoto),
            // One links the note's photo, one an attachment only it used; the last is the note
            // as it is, so it is not a past version.
            revision(u"r1"_s, kPacking, kNine, {},
                     patches({patch(0, {}, {}, u"![beach](:/%1) socks"_s.arg(kPhoto))})),
            revision(u"r2"_s, kPacking, kTen, {},
                     patches({patch(u"![beach](:/%1) socks"_s.arg(kPhoto).size(), {}, {},
                                    u" ![old](:/%1)"_s.arg(kGone))})),
            revision(u"r3"_s, kPacking, kTen + 60000, {},
                     patches({patch(0, u"![beach](:/%1) socks"_s.arg(kPhoto),
                                    u" ![old](:/%1)"_s.arg(kGone), u", hat"_s)})),
        });
    QVERIFY(!database.isEmpty());
    QFile photo(QFileInfo(database).dir().filePath(u"resources/"_s + kPhoto + u".png"_s));
    QVERIFY(photo.open(QIODevice::WriteOnly));
    photo.write("png bytes");
    photo.close();

    const Vault vault(m_dir->filePath(u"Notes"_s));
    QVERIFY(QDir().mkpath(vault.root()));
    QString error;
    const std::optional<JoplinExport> read = readJoplin(database, &error);
    QVERIFY2(read, qPrintable(error));
    QCOMPARE(read->notes.first().revisions.size(), 3);

    // A dry run counts them and keeps none.
    std::optional<JoplinImport> done = importJoplin(vault, *read, {}, true, &error);
    QVERIFY2(done, qPrintable(error));
    QCOMPARE(done->versions, 2);
    const QString note = u"Travel/Packing list.md"_s;
    QVERIFY(NoteHistory(vault).versions(note).isEmpty());

    done = importJoplin(vault, *read, {}, false, &error);
    QVERIFY2(done, qPrintable(error));
    QCOMPARE(done->versions, 2);
    const QList<NoteHistory::Version> versions = NoteHistory(vault).versions(note);
    QCOMPARE(versions.size(), 2);
    QCOMPARE(versions.at(0).written, QDateTime::fromMSecsSinceEpoch(kTen, QTimeZone::UTC));
    QCOMPARE(versions.at(1).written, QDateTime::fromMSecsSinceEpoch(kNine, QTimeZone::UTC));
    const auto text = [&](const NoteHistory::Version &version) {
        return notefile::read(vault.absolute(version.path), &error).value_or(QString());
    };
    // Front matter as the note has it; the photo's link made relative, and the attachment only
    // an old version used left as it was rather than brought in.
    QVERIFY2(text(versions.at(1)).startsWith(u"---\ncreated: "_s),
             qPrintable(text(versions.at(1))));
    QVERIFY2(text(versions.at(1)).endsWith(u"![beach](Packing%20list.assets/beach.png) socks\n"_s),
             qPrintable(text(versions.at(1))));
    QVERIFY2(text(versions.at(0)).contains(u"![old](:/%1)"_s.arg(kGone)),
             qPrintable(text(versions.at(0))));
    QCOMPARE(done->attachments, 1);

    // Importing again keeps nothing twice.
    done = importJoplin(vault, *read, {}, false, &error);
    QVERIFY2(done, qPrintable(error));
    QCOMPARE(done->versions, 0);
    QCOMPARE(NoteHistory(vault).versions(note).size(), 2);
}

QTEST_GUILESS_MAIN(TestJoplinDatabase)
#include "tst_joplindatabase.moc"
