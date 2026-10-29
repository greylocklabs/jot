#include "JexBuilder.h"
#include "JoplinDatabaseBuilder.h"

#include "jot/JoplinExport.h"
#include "jot/JoplinImport.h"
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
            u"INSERT INTO revisions (id, item_id, item_updated_time) VALUES ('r1', '%1', %2)"_s
                .arg(kPlain)
                .arg(kNine),
            u"INSERT INTO revisions (id, item_id, item_updated_time) VALUES ('r2', '%1', %2)"_s
                .arg(kPlain)
                .arg(kTen),
        });
    QVERIFY(!database.isEmpty());
    QString error;
    const std::optional<JoplinExport> read = readJoplinDatabase(database, &error);
    QVERIFY2(read, qPrintable(error));
    QCOMPARE(read->notes.size(), 1);
    QVERIFY(read->notes.first().deleted);
    QVERIFY(read->resources.isEmpty());
    QCOMPARE(read->skipped, (QStringList{u"3 encrypted items, which Joplin has to decrypt first"_s,
                                         u"2 note revisions"_s}));
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

QTEST_GUILESS_MAIN(TestJoplinDatabase)
#include "tst_joplindatabase.moc"
