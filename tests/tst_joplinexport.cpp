#include "jot/JoplinExport.h"

#include <QHash>
#include <QTemporaryDir>
#include <QTest>

using namespace jot;
using namespace Qt::StringLiterals;

namespace {

/// One tar entry: a 512-byte header, then `data` padded to whole blocks. `type` is '0' for a
/// file, 'x' for a pax header and 'L' for a GNU long name; `prefix` is ustar's path prefix.
QByteArray tarEntry(const QByteArray &name, const QByteArray &data, char type = '0',
                    const QByteArray &prefix = {})
{
    // Each field has a fixed width; the value is written at its start, padded with NULs.
    const auto put = [](QByteArray &header, qsizetype at, const QByteArray &value) {
        std::copy(value.cbegin(), value.cend(), header.begin() + at);
    };
    QByteArray header(512, '\0');
    put(header, 0, name);
    put(header, 100, "0000644");
    put(header, 124, QByteArray::number(data.size(), 8).rightJustified(11, '0'));
    put(header, 148, QByteArray(8, ' '));
    header[156] = type;
    put(header, 257,
        QByteArray("ustar\0"
                   "00",
                   8));
    put(header, 345, prefix);
    int sum = 0;
    for (const char c : std::as_const(header))
        sum += static_cast<unsigned char>(c);
    put(header, 148, QByteArray::number(sum, 8).rightJustified(6, '0') + '\0');
    return header + data + QByteArray((512 - data.size() % 512) % 512, '\0');
}

/// A ustar archive of `files`, by path, as Joplin writes a JEX.
QByteArray tar(const QList<std::pair<QString, QByteArray>> &files)
{
    QByteArray out;
    for (const auto &[path, data] : files)
        out += tarEntry(path.toUtf8(), data);
    return out + QByteArray(1024, '\0');
}

/// A raw item as Joplin exports it: the title and body, if any, then its properties.
QByteArray item(const QString &title, const QString &body, const QString &props)
{
    QString text;
    if (!title.isEmpty())
        text = title + u"\n\n"_s + (body.isEmpty() ? QString() : body + u"\n\n"_s);
    return (text + props).toUtf8();
}

const QString kTimes = uR"(created_time: 2026-10-01T09:00:00.000Z
updated_time: 2026-10-02T10:00:00.000Z
user_created_time: 2026-10-01T09:00:00.000Z
user_updated_time: 2026-10-02T10:00:00.000Z
encryption_cipher_text:
encryption_applied: 0)"_s;

} // namespace

class TestJoplinExport : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void readsNotebooksNotesTagsAndResources();
    void keepsABodyWhoseLinesLookLikeProperties();
    void readsToDosTrashAndConflicts();
    void leavesWhereANoteWasWrittenOut();
    void skipsEncryptedItemsAndRevisions();
    void findsFilesByPaxGnuAndPrefixedPaths();
    void readsAnExportFromAFile();
    void turnsAwayWhatIsNotAnExport();
};

void TestJoplinExport::readsNotebooksNotesTagsAndResources()
{
    const QByteArray jex = tar({
        {u"aa.md"_s, item(u"Stories"_s, {}, u"id: aa\n"_s + kTimes + uR"(
parent_id:
icon: {"emoji":"🦊","name":"fox","type":1}
deleted_time: 0
type_: 2)"_s)},
        {u"ab.md"_s,
         item(u"Places"_s, {},
              u"id: ab\n"_s + kTimes + u"\nparent_id: aa\ndeleted_time: 0\ntype_: 2"_s)},
        {u"n1.md"_s, item(u"The ferry"_s, u"Crossing at dawn.\n\n![map](:/r1)"_s,
                          u"id: n1\nparent_id: ab\n"_s + kTimes + uR"(
is_todo: 0
todo_due: 0
todo_completed: 0
markup_language: 1
source_url: https://example.org/ferry
author: Mara
is_conflict: 0
deleted_time: 0
type_: 1)"_s)},
        {u"t1.md"_s, item(u"travel"_s, {}, u"id: t1\n"_s + kTimes + u"\nparent_id: \ntype_: 5"_s)},
        {u"nt1.md"_s,
         (u"id: nt1\nnote_id: n1\ntag_id: t1\n"_s + kTimes + u"\ntype_: 6"_s).toUtf8()},
        {u"r1.md"_s, item(u"map.png"_s, {},
                          u"id: r1\nmime: image/png\nfilename: map.png\nfile_extension: png\n"_s +
                              kTimes + u"\ntype_: 4"_s)},
        {u"resources/r1.png"_s, QByteArray("\x89PNG", 4)},
    });

    QString error;
    const auto ex = readJoplinExport(jex, &error);
    QVERIFY2(ex, qPrintable(error));
    QCOMPARE(ex->notebooks.size(), 2);
    QCOMPARE(ex->notebooks[0].title, u"Stories"_s);
    QCOMPARE(ex->notebooks[0].emoji, u"🦊"_s);
    QCOMPARE(ex->notebooks[1].parentId, u"aa"_s);

    QCOMPARE(ex->notes.size(), 1);
    const auto &note = ex->notes.first();
    QCOMPARE(note.title, u"The ferry"_s);
    QCOMPARE(note.notebookId, u"ab"_s);
    QCOMPARE(note.body, u"Crossing at dawn.\n\n![map](:/r1)"_s);
    QCOMPARE(note.created, QDateTime(QDate(2026, 10, 1), QTime(9, 0), QTimeZone::UTC));
    QCOMPARE(note.updated, QDateTime(QDate(2026, 10, 2), QTime(10, 0), QTimeZone::UTC));
    QCOMPARE(note.sourceUrl, u"https://example.org/ferry"_s);
    QCOMPARE(note.author, u"Mara"_s);
    QVERIFY(!note.html);
    QVERIFY(!note.isTodo);

    QCOMPARE(ex->tags.size(), 1);
    QCOMPARE(ex->noteTags.value(u"n1"_s), QStringList{u"t1"_s});
    QCOMPARE(ex->resources.size(), 1);
    QCOMPARE(ex->resources.first().fileName, u"map.png"_s);
    QCOMPARE(ex->resources.first().data, QByteArray("\x89PNG", 4));
    QVERIFY(ex->skipped.isEmpty());
}

void TestJoplinExport::keepsABodyWhoseLinesLookLikeProperties()
{
    // Only what follows the last blank line is properties, so a body of "key: value" lines,
    // or one with blank lines of its own, comes through whole.
    const QString body = u"status: draft\n\nowner: me\n\n\nend"_s;
    const auto ex = readJoplinExport(tar(
        {{u"n1.md"_s, item(u"Notes: a list"_s, body, u"id: n1\n"_s + kTimes + u"\ntype_: 1"_s)}}));
    QVERIFY(ex);
    QCOMPARE(ex->notes.first().title, u"Notes: a list"_s);
    QCOMPARE(ex->notes.first().body, body);

    // A note with a title and no body.
    const auto empty = readJoplinExport(
        tar({{u"n2.md"_s, item(u"Empty"_s, {}, u"id: n2\n"_s + kTimes + u"\ntype_: 1"_s)}}));
    QVERIFY(empty);
    QCOMPARE(empty->notes.first().title, u"Empty"_s);
    QVERIFY(empty->notes.first().body.isEmpty());
}

void TestJoplinExport::readsToDosTrashAndConflicts()
{
    const auto ex = readJoplinExport(tar({
        {u"n1.md"_s, item(u"Call the ferry office"_s, {}, u"id: n1\n"_s + kTimes + uR"(
is_todo: 1
todo_due: 1793091600000
todo_completed: 0
markup_language: 2
is_conflict: 1
deleted_time: 2026-10-20T08:00:00.000Z
type_: 1)"_s)},
    }));
    QVERIFY(ex);
    const auto &todo = ex->notes.first();
    QVERIFY(todo.isTodo);
    QCOMPARE(todo.due, QDateTime::fromMSecsSinceEpoch(1793091600000, QTimeZone::UTC));
    QVERIFY(!todo.completed.isValid());
    QVERIFY(todo.html);
    QVERIFY(todo.conflict);
    QVERIFY(todo.deleted);
}

void TestJoplinExport::leavesWhereANoteWasWrittenOut()
{
    const auto ex = readJoplinExport(tar({
        {u"n1.md"_s,
         item(
             u"Somewhere"_s, u"text"_s,
             u"id: n1\n"_s + kTimes +
                 u"\nlatitude: 40.67820000\nlongitude: -73.94420000\naltitude: 12.0000\ntype_: 1"_s)},
    }));
    QVERIFY(ex);
    // The export model has nowhere to keep a location, and none leaks into the body.
    QCOMPARE(ex->notes.first().body, u"text"_s);
}

void TestJoplinExport::skipsEncryptedItemsAndRevisions()
{
    const auto ex = readJoplinExport(tar({
        {u"n1.md"_s, item(u"Kept"_s, u"text"_s, u"id: n1\n"_s + kTimes + u"\ntype_: 1"_s)},
        {u"n2.md"_s,
         (u"id: n2\nencryption_cipher_text: JED01...\nencryption_applied: 1\ntype_: 1"_s).toUtf8()},
        {u"v1.md"_s, (u"id: v1\nitem_id: n1\n"_s + kTimes + u"\ntype_: 13"_s).toUtf8()},
        {u"m1.md"_s, (u"id: m1\n"_s + kTimes + u"\ntype_: 9"_s).toUtf8()},
    }));
    QVERIFY(ex);
    QCOMPARE(ex->notes.size(), 1);
    QCOMPARE(ex->skipped,
             (QStringList{u"1 encrypted item, which Joplin has to decrypt first"_s,
                          u"1 note revision"_s, u"1 item of a kind Jot does not read"_s}));
}

void TestJoplinExport::findsFilesByPaxGnuAndPrefixedPaths()
{
    // Three attachments, each named another way tar allows: a pax header, a GNU long-name
    // entry and the ustar prefix field.
    const QString ids[] = {u"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"_s,
                           u"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"_s,
                           u"cccccccccccccccccccccccccccccccc"_s};
    QByteArray archive =
        tarEntry("n1.md", item(u"Uses them"_s, {}, u"id: n1\n"_s + kTimes + u"\ntype_: 1"_s));
    for (const QString &id : ids)
        archive +=
            tarEntry((id + u".md"_s).toUtf8(), item(id + u".png"_s, {},
                                                    u"id: "_s + id + u"\nfile_extension: png\n"_s +
                                                        kTimes + u"\ntype_: 4"_s));
    const QByteArray pax = "path=resources/" + ids[0].toUtf8() + ".png\n";
    const QByteArray record = QByteArray::number(pax.size() + 3) + ' ' + pax;
    archive += tarEntry("PaxHeader/x", record, 'x') + tarEntry("x", "pax");
    archive += tarEntry("././@LongLink", "resources/" + ids[1].toUtf8() + ".png", 'L') +
               tarEntry("y", "gnu");
    archive += tarEntry(ids[2].toUtf8() + ".png", "prefix", '0', "resources");
    archive += QByteArray(1024, '\0');

    QString error;
    const auto ex = readJoplinExport(archive, &error);
    QVERIFY2(ex, qPrintable(error));
    QHash<QString, QByteArray> data;
    for (const auto &resource : ex->resources)
        data.insert(resource.id, resource.data);
    QCOMPARE(data.value(ids[0]), QByteArray("pax"));
    QCOMPARE(data.value(ids[1]), QByteArray("gnu"));
    QCOMPARE(data.value(ids[2]), QByteArray("prefix"));
}

void TestJoplinExport::readsAnExportFromAFile()
{
    QTemporaryDir dir;
    QFile file(dir.filePath(u"notes.jex"_s));
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(
        tar({{u"n1.md"_s, item(u"Kept"_s, u"text"_s, u"id: n1\n"_s + kTimes + u"\ntype_: 1"_s)}}));
    file.close();
    QString error;
    const auto ex = readJoplinExport(file.fileName(), &error);
    QVERIFY2(ex, qPrintable(error));
    QCOMPARE(ex->notes.first().title, u"Kept"_s);
}

void TestJoplinExport::turnsAwayWhatIsNotAnExport()
{
    QString error;
    QVERIFY(!readJoplinExport(QByteArray("just some text"), &error));
    QVERIFY(error.contains(u"not a tar archive"_s));

    QVERIFY(!readJoplinExport(tar({{u"README.md"_s, QByteArray("# Hello\n")}}), &error));
    QVERIFY(error.contains(u"no Joplin notes"_s));

    QVERIFY(!readJoplinExport(u"/no/such/file.jex"_s, &error));
    QVERIFY(!error.isEmpty());
}

QTEST_GUILESS_MAIN(TestJoplinExport)
#include "tst_joplinexport.moc"
