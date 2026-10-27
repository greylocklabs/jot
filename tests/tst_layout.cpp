#include "jot/Layout.h"

#include <QDir>
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
    const Layout layout = Layout::read(*m_vault);
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
    QCOMPARE(Layout::read(*m_vault).emojis(), layout.emojis());

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
    QVERIFY(Layout::read(*m_vault).write(*m_vault, &error));
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
    QCOMPARE(Layout::read(*m_vault).emoji(odd), u"\U0001F947"_s);
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
    const Layout moved = Layout::read(*m_vault);
    QCOMPARE(moved.emoji(u"Inbox/Ashfall"_s), u"a"_s);
    QCOMPARE(moved.emoji(u"Inbox/Ashfall/Chapters"_s), u"b"_s);
    QCOMPARE(moved.emoji(u"Inbox"_s), u"c"_s);
    QCOMPARE(moved.emoji(u"Ashfall"_s), QString());
}

QTEST_GUILESS_MAIN(TestLayout)
#include "tst_layout.moc"
