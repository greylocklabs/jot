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
    void keepsFavorites();
    void keepsTheViewsEmojis();
    void keepsNotebookTargets();

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
    layout.setSort(u"Ashfall/Chapters"_s, u"title"_s);
    QString error;
    QVERIFY(layout.write(*m_vault, &error));
    QCOMPARE(Layout::read(*m_vault)->sort(u"Ashfall/Chapters"_s), u"title"_s);

    QVERIFY(m_vault->moveFolder(u"Ashfall"_s, u"Inbox/Ashfall"_s, &error));
    QVERIFY2(Layout::follow(*m_vault, u"Ashfall"_s, u"Inbox/Ashfall"_s, &error), qPrintable(error));
    const Layout moved = Layout::read(*m_vault).value();
    QCOMPARE(moved.emoji(u"Inbox/Ashfall"_s), u"a"_s);
    QCOMPARE(moved.emoji(u"Inbox/Ashfall/Chapters"_s), u"b"_s);
    QCOMPARE(moved.emoji(u"Inbox"_s), u"c"_s);
    QCOMPARE(moved.emoji(u"Ashfall"_s), QString());
    // Its sort goes with it, and is its own, not its parent's.
    QCOMPARE(moved.sort(u"Inbox/Ashfall/Chapters"_s), u"title"_s);
    QCOMPARE(moved.sort(u"Inbox/Ashfall"_s), QString());
    QCOMPARE(moved.sorts().size(), 1);
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

void TestLayout::keepsFavorites()
{
    Layout layout;
    layout.setFavorite(u"Ashfall/Chapters/Ch 2.md"_s, true);
    layout.setFavorite(u"Ashfall/Chapters/Prologue.md"_s, true);
    layout.setFavorite(u"Ashfall/Chapters/Prologue.md"_s, true);
    layout.setFavorite(u"Gone.md"_s, true);
    QString error;
    QVERIFY(layout.write(*m_vault, &error));
    // In the order given, without what is gone.
    QCOMPARE(read(Layout::kPath), u"favorites:\n"
                                  "  notes:\n"
                                  "    - Ashfall/Chapters/Ch 2.md\n"
                                  "    - Ashfall/Chapters/Prologue.md\n"_s);
    // With an emoji of its own beside them, both are read.
    Layout starred = Layout::read(*m_vault).value();
    starred.setViewEmoji(u"favorites"_s, u"\U0001F496"_s);
    QVERIFY(starred.write(*m_vault, &error));
    QCOMPARE(Layout::read(*m_vault).value().viewEmoji(u"favorites"_s), u"\U0001F496"_s);
    QCOMPARE(Layout::read(*m_vault).value().favorites().size(), 2);
    QCOMPARE(Layout::read(*m_vault).value().favorites(),
             (QStringList{u"Ashfall/Chapters/Ch 2.md"_s, u"Ashfall/Chapters/Prologue.md"_s}));
    // One put first moves there.
    Layout moved = Layout::read(*m_vault).value();
    moved.setFavorite(u"Ashfall/Chapters/Prologue.md"_s, true, 0);
    QCOMPARE(moved.favorites(),
             (QStringList{u"Ashfall/Chapters/Prologue.md"_s, u"Ashfall/Chapters/Ch 2.md"_s}));

    // A favorite goes with its note, and with a notebook moved around it.
    QVERIFY(m_vault->moveNote(u"Ashfall/Chapters/Ch 2.md"_s, u"Inbox/Ch 2.md"_s, &error));
    QVERIFY(Layout::follow(*m_vault, u"Ashfall/Chapters/Ch 2.md"_s, u"Inbox/Ch 2.md"_s, &error));
    QVERIFY(m_vault->moveFolder(u"Ashfall"_s, u"Saga"_s, &error));
    QVERIFY(Layout::follow(*m_vault, u"Ashfall"_s, u"Saga"_s, &error));
    QCOMPARE(Layout::read(*m_vault).value().favorites(),
             (QStringList{u"Inbox/Ch 2.md"_s, u"Saga/Chapters/Prologue.md"_s}));

    layout = Layout::read(*m_vault).value();
    layout.setFavorite(u"Inbox/Ch 2.md"_s, false);
    QCOMPARE(layout.favorites(), QStringList{u"Saga/Chapters/Prologue.md"_s});
    QCOMPARE(Layout::defaultEmoji(u"favorites"_s), u"\u2B50"_s);

    // Written on one line, they are read, and a new one is kept beside them.
    write(Layout::kPath, u"favorites: [Saga/Chapters/Prologue.md]\n"_s);
    layout = Layout::read(*m_vault).value();
    QCOMPARE(layout.favorites(), QStringList{u"Saga/Chapters/Prologue.md"_s});
    layout.setFavorite(u"Inbox/Ch 2.md"_s, true);
    QVERIFY(layout.write(*m_vault, &error));
    QCOMPARE(Layout::read(*m_vault).value().favorites().size(), 2);
}

void TestLayout::keepsTheViewsEmojis()
{
    // Trash has its emoji without the file saying so, and the file says nothing until it changes.
    Layout layout;
    QCOMPARE(layout.viewEmoji(u"trash"_s), Layout::defaultEmoji(u"trash"_s));
    QCOMPARE(layout.viewEmoji(u"tasks"_s), u"\u2705"_s);
    layout.setViewEmoji(u"trash"_s, Layout::defaultEmoji(u"trash"_s));
    QCOMPARE(layout.text(), QString());

    layout.setViewEmoji(u"trash"_s, u"\U0001F9FA"_s);
    QString error;
    QVERIFY2(layout.write(*m_vault, &error), qPrintable(error));
    QCOMPARE(read(Layout::kPath), u"trash:\n  emoji: \U0001F9FA\n"_s);
    QCOMPARE(Layout::read(*m_vault).value().viewEmoji(u"trash"_s), u"\U0001F9FA"_s);

    // Taken away, it stays away.
    layout.setViewEmoji(u"trash"_s, {});
    QVERIFY(layout.write(*m_vault, &error));
    QCOMPARE(read(Layout::kPath), u"trash:\n  emoji: \"\"\n"_s);
    QCOMPARE(Layout::read(*m_vault).value().viewEmoji(u"trash"_s), QString());

    // Tasks' too, each in its own section, and what Jot does not use under one is kept.
    write(Layout::kPath, u"tasks:\n  emoji: y\ntrash:\n  emoji: x\n  color: red\n"_s);
    const Layout both = Layout::read(*m_vault).value();
    QCOMPARE(both.viewEmoji(u"tasks"_s), u"y"_s);
    QCOMPARE(both.text(), u"tasks:\n  emoji: y\ntrash:\n  emoji: x\n  color: red\n"_s);

    // Indented its own way, it is written back with what is under it still under it.
    write(Layout::kPath, u"trash:\n    emoji: x\n    color: red\n    nested:\n      a: 1\n"_s);
    QCOMPARE(Layout::read(*m_vault).value().viewEmoji(u"trash"_s), u"x"_s);
    QCOMPARE(Layout::read(*m_vault).value().text(),
             u"trash:\n  emoji: x\n  color: red\n  nested:\n    a: 1\n"_s);

    // A comment before the first key does not set how far the keys are indented.
    write(Layout::kPath, u"trash:\n# why\n  emoji: x\n"_s);
    Layout commented = Layout::read(*m_vault).value();
    QCOMPARE(commented.viewEmoji(u"trash"_s), u"x"_s);
    commented.setViewEmoji(u"trash"_s, u"z"_s);
    QCOMPARE(commented.text(), u"trash:\n  emoji: z\n  # why\n"_s);

    // A value on the section's own line is kept as written, until Jot sets one itself.
    write(Layout::kPath, u"trash: {emoji: x}\npinned:\n  - Inbox/A.md\n"_s);
    Layout inline_ = Layout::read(*m_vault).value();
    QVERIFY(inline_.text().contains(u"trash: {emoji: x}\n"_s));
    QVERIFY(inline_.text().contains(u"  - Inbox/A.md\n"_s));
    inline_.setViewEmoji(u"trash"_s, u"z"_s);
    QVERIFY(!inline_.text().contains(u"{emoji: x}"_s));
    QVERIFY(inline_.text().contains(u"trash:\n  emoji: z\n"_s));
}

QTEST_GUILESS_MAIN(TestLayout)
#include "tst_layout.moc"

void TestLayout::keepsNotebookTargets()
{
    QVERIFY(QDir(m_dir->path()).mkpath(u"Novel"_s) && QDir(m_dir->path()).mkpath(u"Essays"_s));
    Layout layout;
    layout.setEmoji(u"Novel"_s, u"\U0001F4DA"_s);
    layout.setTarget(u"Novel"_s, {90000, QDate(2027, 1, 30)});
    layout.setTarget(u"Essays"_s, {5000, {}});
    QString error;
    QVERIFY2(layout.write(*m_vault, &error), qPrintable(error));
    QCOMPARE(read(Layout::kPath), u"folders:\n"
                                  "  Essays:\n"
                                  "    target: 5000\n"
                                  "  Novel:\n"
                                  "    deadline: 2027-01-30\n"
                                  "    emoji: \U0001F4DA\n"
                                  "    target: 90000\n"_s);
    const Layout kept = Layout::read(*m_vault).value();
    QCOMPARE(kept.target(u"Novel"_s).words, 90000);
    QCOMPARE(kept.target(u"Novel"_s).deadline, QDate(2027, 1, 30));
    QCOMPARE(kept.target(u"Essays"_s).words, 5000);
    QVERIFY(!kept.target(u"Essays"_s).deadline.isValid());
    QCOMPARE(kept.target(u"Other"_s).words, 0);
    // A target that is no number of words, or fewer than none, is none.
    QFile odd(m_vault->absolute(Layout::kPath));
    QVERIFY(odd.open(QIODevice::WriteOnly));
    odd.write("folders:\n  Novel:\n    target: lots\n  Essays:\n    target: -5\n"
              "    deadline: 2027-01-30\n");
    odd.close();
    const Layout oddly = Layout::read(*m_vault).value();
    QCOMPARE(oddly.target(u"Novel"_s).words, 0);
    QCOMPARE(oddly.target(u"Essays"_s).words, 0);
    QVERIFY(!oddly.target(u"Essays"_s).deadline.isValid());

    // Taken away, the deadline goes with it and the emoji stays.
    layout.setTarget(u"Novel"_s, {});
    layout.setTarget(u"Essays"_s, {});
    QVERIFY(layout.write(*m_vault, &error));
    QCOMPARE(read(Layout::kPath), u"folders:\n"
                                  "  Novel:\n"
                                  "    emoji: \U0001F4DA\n"_s);
}
