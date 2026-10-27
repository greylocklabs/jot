#include "jot/Rename.h"

#include "jot/Layout.h"

#include <QDir>
#include <QTemporaryDir>
#include <QTest>

using namespace jot;
using namespace Qt::StringLiterals;

class TestRename : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void init();
    void cleanup();

    void retargetsInTheStyleWritten_data();
    void retargetsInTheStyleWritten();
    void rewritesLinksThatReachedTheNote();
    void leavesOtherLinksAndCodeAlone();
    void keepsTheMovedNotesOwnPathsWorking();
    void reportsWhatItWrote();
    void parenthesesStayInsideTheLink();
    void movesANotebookAndKeepsItsLinksWorking();
    void refusesToMoveANotebookIntoItself();
    void aMovedNotebookKeepsItsEmoji();

private:
    void write(const QString &path, const QString &text);
    QString read(const QString &path);
    void open();

    std::unique_ptr<QTemporaryDir> m_dir;
    std::unique_ptr<Vault> m_vault;
    std::unique_ptr<VaultIndex> m_index;
};

void TestRename::init()
{
    m_dir = std::make_unique<QTemporaryDir>();
    m_vault = std::make_unique<Vault>(m_dir->filePath(u"vault"_s));
    QVERIFY(QDir().mkpath(m_vault->root()));
}

void TestRename::cleanup()
{
    m_index.reset();
}

void TestRename::write(const QString &path, const QString &text)
{
    QVERIFY(QDir().mkpath(QFileInfo(m_vault->absolute(path)).path()));
    QString error;
    QVERIFY2(notefile::write(m_vault->absolute(path), text, &error), qPrintable(error));
}

QString TestRename::read(const QString &path)
{
    QString error;
    return notefile::read(m_vault->absolute(path), &error).value_or(error);
}

void TestRename::open()
{
    m_index = std::make_unique<VaultIndex>(*m_vault, m_dir->filePath(u"index.sqlite"_s));
    QString error;
    QVERIFY2(m_index->open(&error), qPrintable(error));
    m_index->refresh();
}

void TestRename::retargetsInTheStyleWritten_data()
{
    QTest::addColumn<QString>("raw");
    QTest::addColumn<bool>("wiki");
    QTest::addColumn<QString>("source");
    QTest::addColumn<QString>("to");
    QTest::addColumn<QString>("expected");
    QTest::newRow("name") << u"Mara"_s << true << u"Scene.md"_s << u"Cast/Mara Vey.md"_s
                          << u"Mara Vey"_s;
    QTest::newRow("name and heading")
        << u"Mara#Voice"_s << true << u"Scene.md"_s << u"Cast/Mara Vey.md"_s << u"Mara Vey#Voice"_s;
    QTest::newRow("wiki path") << u"Old/Mara"_s << true << u"Scene.md"_s << u"Cast/Mara.md"_s
                               << u"Cast/Mara"_s;
    QTest::newRow("markdown") << u"Mara.md"_s << false << u"Scene.md"_s << u"Cast/Mara.md"_s
                              << u"Cast/Mara.md"_s;
    QTest::newRow("markdown up") << u"../Mara.md"_s << false << u"Acts/Scene.md"_s
                                 << u"Cast/Mara.md"_s << u"../Cast/Mara.md"_s;
    QTest::newRow("no extension") << u"Mara"_s << false << u"Scene.md"_s << u"Cast/Mara.md"_s
                                  << u"Cast/Mara"_s;
}

void TestRename::retargetsInTheStyleWritten()
{
    QFETCH(QString, raw);
    QFETCH(bool, wiki);
    QFETCH(QString, source);
    QFETCH(QString, to);
    QFETCH(QString, expected);
    QCOMPARE(retarget(raw, wiki, source, to), expected);
}

void TestRename::rewritesLinksThatReachedTheNote()
{
    write(u"Mara.md"_s, u"mara\n"_s);
    write(u"Scene.md"_s,
          u"---\ntitle: Scene [[Mara]]\n---\n[[Mara]], [[Mara|her]], ![[Mara#Voice]]\n"
          u"and [the ferrywoman](Mara.md)\n"_s);
    write(u"Acts/One.md"_s, u"[up](../Mara.md)\n"_s);
    open();

    QString error;
    const std::optional<QStringList> rewritten =
        renameNote(*m_vault, *m_index, u"Mara.md"_s, u"Cast/Mara Vey.md"_s, &error);
    QVERIFY2(rewritten, qPrintable(error));
    QCOMPARE(*rewritten, (QStringList{u"Acts/One.md"_s, u"Scene.md"_s}));
    // Front matter is the user's, not a link.
    QCOMPARE(
        read(u"Scene.md"_s),
        u"---\ntitle: Scene [[Mara]]\n---\n[[Mara Vey]], [[Mara Vey|her]], ![[Mara Vey#Voice]]\n"
        u"and [the ferrywoman](Cast/Mara%20Vey.md)\n"_s);
    QCOMPARE(read(u"Acts/One.md"_s), u"[up](../Cast/Mara%20Vey.md)\n"_s);
    QCOMPARE(m_index->backlinks(u"Cast/Mara Vey.md"_s).size(), 5);
    QVERIFY(m_index->unresolved().isEmpty());
}

void TestRename::leavesOtherLinksAndCodeAlone()
{
    write(u"Mara.md"_s, u"mara\n"_s);
    write(u"Old/Mara.md"_s, u"another Mara\n"_s);
    write(u"Scene.md"_s, u"[[Old/Mara]] `[[Mara]]`\n```\n[[Mara]]\n```\n[[Mara]]\n"_s);
    open();

    QString error;
    QVERIFY(renameNote(*m_vault, *m_index, u"Mara.md"_s, u"Mara Vey.md"_s, &error));
    QCOMPARE(read(u"Scene.md"_s), u"[[Old/Mara]] `[[Mara]]`\n```\n[[Mara]]\n```\n[[Mara Vey]]\n"_s);
}

void TestRename::keepsTheMovedNotesOwnPathsWorking()
{
    write(u"Mara.md"_s, u"[ferry](Places/Ferry.md), [[Ferry]], [self](Mara.md#top)\n"_s);
    write(u"Places/Ferry.md"_s, u"ferry\n"_s);
    open();

    QString error;
    QVERIFY(renameNote(*m_vault, *m_index, u"Mara.md"_s, u"Cast/Mara.md"_s, &error));
    QCOMPARE(read(u"Cast/Mara.md"_s),
             u"[ferry](../Places/Ferry.md), [[Ferry]], [self](Mara.md#top)\n"_s);
    QVERIFY(m_index->unresolved().isEmpty());
}

void TestRename::reportsWhatItWrote()
{
    write(u"Mara.md"_s, u"mara\n"_s);
    write(u"Scene.md"_s, u"[[Mara]]\n"_s);
    open();

    QStringList expected;
    QString error;
    QVERIFY(
        renameNote(*m_vault, *m_index, u"Mara.md"_s, u"Mara Vey.md"_s, &error,
                   [&expected](const QString &path, const QString &) { expected.append(path); }));
    QCOMPARE(expected, QStringList{u"Scene.md"_s});

    write(u"Taken.md"_s, u"x\n"_s);
    m_index->update({u"Taken.md"_s});
    QVERIFY(!renameNote(*m_vault, *m_index, u"Mara Vey.md"_s, u"Taken.md"_s, &error));
    QVERIFY(error.contains(u"already exists"_s));
    QCOMPARE(read(u"Scene.md"_s), u"[[Mara Vey]]\n"_s);
}

void TestRename::parenthesesStayInsideTheLink()
{
    write(u"Plan.md"_s, u"plan\n"_s);
    write(u"Scene.md"_s, u"[x](Plan.md) and [y](<Plan.md>)\n"_s);
    open();

    QString error;
    QVERIFY(renameNote(*m_vault, *m_index, u"Plan.md"_s, u"Plan (draft).md"_s, &error));
    QCOMPARE(read(u"Scene.md"_s), u"[x](<Plan (draft).md>) and [y](<Plan (draft).md>)\n"_s);
    QCOMPARE(m_index->backlinks(u"Plan (draft).md"_s).size(), 2);
    // And it can be renamed again from there.
    QVERIFY(renameNote(*m_vault, *m_index, u"Plan (draft).md"_s, u"Plan.md"_s, &error));
    QCOMPARE(read(u"Scene.md"_s), u"[x](<Plan.md>) and [y](<Plan.md>)\n"_s);
}

void TestRename::movesANotebookAndKeepsItsLinksWorking()
{
    write(u"Pitch.md"_s,
          u"[Mara](Cast/Mara.md), [[Mara]], [[Cast/Soren]], [deep](Cast/Minor/Tam.md)\n"_s);
    write(u"Board.md"_s, u"![face](Cast/Mara.assets/face.png)\n"_s);
    write(u"Cast/Mara.md"_s,
          u"[brother](Soren.md), [pitch](../Pitch.md), [[Pitch]], ![](Mara.assets/face.png)\n"_s);
    write(u"Cast/Soren.md"_s, u"soren\n"_s);
    write(u"Cast/Minor/Tam.md"_s, u"[mara](../Mara.md)\n"_s);
    write(u"Cast/Mara.assets/face.png"_s, u"png"_s);
    open();

    // Deeper, so paths that lead out of the notebook have further to go.
    QString error;
    const std::optional<QStringList> written =
        moveNotebook(*m_vault, *m_index, u"Cast"_s, u"Story/People"_s, &error);
    QVERIFY2(written, qPrintable(error));

    QVERIFY(!QFileInfo::exists(m_vault->absolute(u"Cast"_s)));
    QVERIFY(QFileInfo::exists(m_vault->absolute(u"Story/People/Mara.assets/face.png"_s)));
    QCOMPARE(read(u"Pitch.md"_s),
             u"[Mara](Story/People/Mara.md), [[Mara]], [[Story/People/Soren]], "
             "[deep](Story/People/Minor/Tam.md)\n"_s);
    // Inside, paths between its own notes still work; the one out has a level more.
    // Its own picture moved with it, so that link stays; one from outside follows it.
    QCOMPARE(read(u"Story/People/Mara.md"_s),
             u"[brother](Soren.md), [pitch](../../Pitch.md), [[Pitch]], "
             "![](Mara.assets/face.png)\n"_s);
    QCOMPARE(read(u"Board.md"_s), u"![face](Story/People/Mara.assets/face.png)\n"_s);
    QCOMPARE(read(u"Story/People/Minor/Tam.md"_s), u"[mara](../Mara.md)\n"_s);
    QCOMPARE(*written, (QStringList{u"Board.md"_s, u"Pitch.md"_s, u"Story/People/Mara.md"_s}));

    QCOMPARE(m_index->resolve(u"Story/People/Soren"_s, {}), u"Story/People/Soren.md"_s);
    QVERIFY(!m_index->note(u"Cast/Mara.md"_s));
    QCOMPARE(m_index->notesIn(u"Story"_s).size(), 3);
}

void TestRename::refusesToMoveANotebookIntoItself()
{
    write(u"Cast/Mara.md"_s, u"mara\n"_s);
    open();
    QString error;
    QVERIFY(!moveNotebook(*m_vault, *m_index, u"Cast"_s, u"Cast/Old"_s, &error));
    QVERIFY(!error.isEmpty());
    QVERIFY(QFileInfo::exists(m_vault->absolute(u"Cast/Mara.md"_s)));

    QVERIFY(m_vault->createFolder({}, u"Cast"_s, &error));
    QCOMPARE(*m_vault->createFolder({}, u"Cast"_s, &error), u"Cast 3"_s);
    // Names are cleaned as note file names are, so none comes out hidden.
    QCOMPARE(*m_vault->createFolder({}, u".hidden"_s, &error), u"hidden"_s);
    QVERIFY(!m_vault->moveFolder(u"Cast"_s, u"Cast 2"_s, &error));
}

void TestRename::aMovedNotebookKeepsItsEmoji()
{
    write(u"Cast/Minor/Tam.md"_s, u"tam\n"_s);
    Layout layout;
    layout.setEmoji(u"Cast"_s, u"a"_s);
    layout.setEmoji(u"Cast/Minor"_s, u"b"_s);
    QString error;
    QVERIFY(layout.write(*m_vault, &error));
    open();

    QVERIFY2(moveNotebook(*m_vault, *m_index, u"Cast"_s, u"Story/People"_s, &error),
             qPrintable(error));
    QCOMPARE(
        Layout::read(*m_vault).emojis(),
        (QHash<QString, QString>{{u"Story/People"_s, u"a"_s}, {u"Story/People/Minor"_s, u"b"_s}}));
}

QTEST_GUILESS_MAIN(TestRename)
#include "tst_rename.moc"
