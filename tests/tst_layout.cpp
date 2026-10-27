#include "jot/Layout.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using namespace jot;
using namespace Qt::StringLiterals;

class TestLayout : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void init();

    void readsTheFormatsExample();
    void writesWhatItReads();
    void writesNothingUntilThereIsSomethingToKeep();
    void leavesOutWhatIsGone();
    void quotesNamesThatNeedIt();
    void followsAMovedNotebook();
    void keepsWhatItDoesNotUse();
    void leavesAnUnreadableFileAlone();
    void keepsPinnedNotes();

private:
    void write(const QString &path, const QString &text);
    QString read(const QString &path);

    std::unique_ptr<QTemporaryDir> m_dir;
    std::unique_ptr<Vault> m_vault;
};

void TestLayout::init()
{
    m_dir = std::make_unique<QTemporaryDir>();
    m_vault = std::make_unique<Vault>(m_dir->path());
    for (const QString &folder : {u"Ashfall/Chapters"_s, u"Inbox"_s})
        QVERIFY(QDir().mkpath(m_vault->absolute(folder)));
    for (const QString &note : {u"Ashfall/Chapters/Prologue.md"_s, u"Ashfall/Chapters/Ch 2.md"_s})
        write(note, u"x\n"_s);
}

void TestLayout::write(const QString &path, const QString &text)
{
    QVERIFY(QDir().mkpath(QFileInfo(m_vault->absolute(path)).path()));
    QString error;
    QVERIFY2(notefile::write(m_vault->absolute(path), text, &error), qPrintable(error));
}

QString TestLayout::read(const QString &path)
{
    QString error;
    return notefile::read(m_vault->absolute(path), &error).value_or(QString());
}

void TestLayout::readsTheFormatsExample()
{
    write(Layout::kPath, u"order:\n"
                         "  Ashfall/Chapters:\n"
                         "    - Prologue.md\n"
                         "    - Ch 2.md\n"
                         "folders:\n"
                         "  Ashfall:\n"
                         "    icon: book\n"
                         "    color: pink\n"
                         "    emoji: \"\U0001F4DA\"\n"
                         "  Inbox:\n"
                         "    emoji: \U0001F4E5 # a comment\n"_s);
    const Layout layout = Layout::read(*m_vault).value();
    QCOMPARE(layout.emoji(u"Ashfall"_s), u"\U0001F4DA"_s);
    QCOMPARE(layout.emoji(u"Inbox"_s), u"\U0001F4E5"_s);
    QCOMPARE(layout.emoji(u"Ashfall/Chapters"_s), QString());
    QCOMPARE(layout.emojis().size(), 2);
    // What Jot does not use yet is kept.
    QVERIFY(layout.text().contains(u"    - Ch 2.md\n"_s));
    QVERIFY(layout.text().contains(u"    icon: book\n"_s));
}

void TestLayout::writesWhatItReads()
{
    Layout layout;
    layout.setEmoji(u"Inbox"_s, u"\U0001F4E5"_s);
    layout.setEmoji(u"Ashfall"_s, u" \U0001F525 "_s);
    QString error;
    QVERIFY2(layout.write(*m_vault, &error), qPrintable(error));
    QCOMPARE(read(Layout::kPath), u"folders:\n"
                                  "  Ashfall:\n"
                                  "    emoji: \U0001F525\n"
                                  "  Inbox:\n"
                                  "    emoji: \U0001F4E5\n"_s);
    QCOMPARE(Layout::read(*m_vault).value().emojis(), layout.emojis());

    // Taking the last emoji away leaves nothing about the folder.
    layout.setEmoji(u"Inbox"_s, {});
    layout.setEmoji(u"Ashfall"_s, {});
    QVERIFY(layout.write(*m_vault, &error));
    QCOMPARE(read(Layout::kPath), QString());
}

void TestLayout::writesNothingUntilThereIsSomethingToKeep()
{
    QString error;
    QVERIFY(Layout().write(*m_vault, &error));
    QVERIFY(!QFileInfo::exists(m_vault->absolute(Layout::kPath)));
    QVERIFY(Layout::follow(*m_vault, u"Inbox"_s, u"Later"_s, &error));
    QVERIFY(!QFileInfo::exists(m_vault->absolute(Layout::kPath)));
}

void TestLayout::leavesOutWhatIsGone()
{
    write(Layout::kPath, u"order:\n"
                         "  Ashfall/Chapters:\n"
                         "    - Prologue.md\n"
                         "    - Gone.md\n"
                         "  Gone:\n"
                         "    - a.md\n"
                         "folders:\n"
                         "  Gone:\n"
                         "    emoji: x\n"
                         "  Inbox:\n"
                         "    emoji: y\n"_s);
    QString error;
    QVERIFY(Layout::read(*m_vault).value().write(*m_vault, &error));
    QCOMPARE(read(Layout::kPath), u"order:\n"
                                  "  Ashfall/Chapters:\n"
                                  "    - Prologue.md\n"
                                  "folders:\n"
                                  "  Inbox:\n"
                                  "    emoji: y\n"_s);
}

void TestLayout::quotesNamesThatNeedIt()
{
    const QString odd = u"#1: \"Firsts\""_s;
    QVERIFY(QDir().mkpath(m_vault->absolute(odd)));
    Layout layout;
    layout.setEmoji(odd, u"\U0001F947"_s);
    QString error;
    QVERIFY(layout.write(*m_vault, &error));
    QVERIFY(read(Layout::kPath).contains(u"  \"#1: \\\"Firsts\\\"\":\n"_s));
    QCOMPARE(Layout::read(*m_vault).value().emoji(odd), u"\U0001F947"_s);
}

void TestLayout::followsAMovedNotebook()
{
    Layout layout;
    layout.setEmoji(u"Ashfall"_s, u"a"_s);
    layout.setEmoji(u"Ashfall/Chapters"_s, u"b"_s);
    layout.setEmoji(u"Inbox"_s, u"c"_s);
    QString error;
    QVERIFY(layout.write(*m_vault, &error));

    QVERIFY(m_vault->moveFolder(u"Ashfall"_s, u"Inbox/Ashfall"_s, &error));
    QVERIFY2(Layout::follow(*m_vault, u"Ashfall"_s, u"Inbox/Ashfall"_s, &error), qPrintable(error));
    const Layout moved = Layout::read(*m_vault).value();
    QCOMPARE(moved.emoji(u"Inbox/Ashfall"_s), u"a"_s);
    QCOMPARE(moved.emoji(u"Inbox/Ashfall/Chapters"_s), u"b"_s);
    QCOMPARE(moved.emoji(u"Inbox"_s), u"c"_s);
    QCOMPARE(moved.emoji(u"Ashfall"_s), QString());
}

void TestLayout::keepsWhatItDoesNotUse()
{
    write(Layout::kPath, u"# Shared with the team.\n"
                         "folders:\n"
                         "  Inbox:\n"
                         "    color: pink\n"
                         "    badges: [new, hot]\n"
                         "    sort:\n"
                         "      by: title\n"
                         "      reverse: true\n"
                         "views:\n"
                         "  # Kanban, later.\n"
                         "  board:\n"
                         "    - Inbox\n"_s);
    Layout layout = Layout::read(*m_vault).value();
    layout.setEmoji(u"Inbox"_s, u"\U0001F4E5"_s);
    QString error;
    QVERIFY(layout.write(*m_vault, &error));
    QCOMPARE(read(Layout::kPath), u"# Shared with the team.\n"
                                  "folders:\n"
                                  "  Inbox:\n"
                                  "    color: pink\n"
                                  "    emoji: \U0001F4E5\n"
                                  "    badges: [new, hot]\n"
                                  "    sort:\n"
                                  "      by: title\n"
                                  "      reverse: true\n"
                                  "views:\n"
                                  "  # Kanban, later.\n"
                                  "  board:\n"
                                  "    - Inbox\n"_s);

    // A notebook moved takes what Jot keeps for it as written, too.
    QVERIFY(m_vault->moveFolder(u"Inbox"_s, u"Later"_s, &error));
    QVERIFY(Layout::follow(*m_vault, u"Inbox"_s, u"Later"_s, &error));
    QVERIFY(read(Layout::kPath).contains(u"  Later:\n    color: pink\n"_s));
    QVERIFY(read(Layout::kPath).contains(u"      reverse: true\n"_s));
}

void TestLayout::leavesAnUnreadableFileAlone()
{
    const QByteArray bytes("folders:\n  Inbox:\n    emoji: \xff\xfe\n");
    QFile file(m_vault->absolute(Layout::kPath));
    QVERIFY(QDir().mkpath(QFileInfo(file).path()));
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(bytes);
    file.close();

    QString error;
    QVERIFY(!Layout::read(*m_vault, &error));
    QVERIFY(error.contains(Layout::kPath));
    QVERIFY(!Layout::follow(*m_vault, u"Inbox"_s, u"Later"_s, &error));
    QVERIFY(file.open(QIODevice::ReadOnly));
    QCOMPARE(file.readAll(), bytes);
}

void TestLayout::keepsPinnedNotes()
{
    Layout layout;
    layout.setPinned(u"Ashfall/Chapters/Ch 2.md"_s, true);
    layout.setPinned(u"Ashfall/Chapters/Prologue.md"_s, true);
    layout.setPinned(u"Ashfall/Chapters/Prologue.md"_s, true);
    layout.setPinned(u"Gone.md"_s, true);
    QString error;
    QVERIFY(layout.write(*m_vault, &error));
    // In the order pinned, without what is gone.
    QCOMPARE(read(Layout::kPath), u"pinned:\n"
                                  "  - Ashfall/Chapters/Ch 2.md\n"
                                  "  - Ashfall/Chapters/Prologue.md\n"_s);
    QCOMPARE(Layout::read(*m_vault).value().pinned(),
             (QStringList{u"Ashfall/Chapters/Ch 2.md"_s, u"Ashfall/Chapters/Prologue.md"_s}));

    // A pin goes with its note, and with a notebook moved around it.
    QVERIFY(m_vault->moveNote(u"Ashfall/Chapters/Ch 2.md"_s, u"Inbox/Ch 2.md"_s, &error));
    QVERIFY(Layout::follow(*m_vault, u"Ashfall/Chapters/Ch 2.md"_s, u"Inbox/Ch 2.md"_s, &error));
    QVERIFY(m_vault->moveFolder(u"Ashfall"_s, u"Saga"_s, &error));
    QVERIFY(Layout::follow(*m_vault, u"Ashfall"_s, u"Saga"_s, &error));
    QCOMPARE(Layout::read(*m_vault).value().pinned(),
             (QStringList{u"Inbox/Ch 2.md"_s, u"Saga/Chapters/Prologue.md"_s}));

    layout = Layout::read(*m_vault).value();
    layout.setPinned(u"Inbox/Ch 2.md"_s, false);
    QCOMPARE(layout.pinned(), QStringList{u"Saga/Chapters/Prologue.md"_s});
}

QTEST_GUILESS_MAIN(TestLayout)
#include "tst_layout.moc"
