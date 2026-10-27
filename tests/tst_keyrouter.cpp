#include "KeyRouter.h"

#include "jot/Settings.h"

#include <QSet>
#include <QSignalSpy>
#include <QTest>

using namespace jot;
using namespace Qt::StringLiterals;

class TestKeyRouter : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void init();
    void singleKeysAct();
    void gStartsATwoKeyCommand();
    void leaderTakesTheNextKey();
    void leaderGivesUpAfterItsTimeout();
    void offLetsKeysThrough();
    void everyKeyMeansOneThing();
    void listsShortcutsAsThePlatformWritesThem();
    void paletteFindsCommandsByName();

private:
    std::unique_ptr<KeyRouter> m_router;
    std::unique_ptr<QSignalSpy> m_triggered;

    QStringList triggered() const
    {
        QStringList ids;
        for (const QList<QVariant> &call : *m_triggered)
            ids << call.first().toString();
        return ids;
    }
};

void TestKeyRouter::init()
{
    m_router = std::make_unique<KeyRouter>();
    m_router->setProperty("viMode", true);
    m_triggered = std::make_unique<QSignalSpy>(m_router.get(), &KeyRouter::triggered);
}

void TestKeyRouter::singleKeysAct()
{
    QVERIFY(m_router->press(u"j"_s));
    QVERIFY(m_router->press(u"G"_s));
    QVERIFY(m_router->press(u"?"_s));
    // The note's own keys are not the window's.
    QVERIFY(!m_router->press(u"x"_s));
    QCOMPARE(triggered(), (QStringList{u"nextNote"_s, u"lastNote"_s, u"help"_s}));
}

void TestKeyRouter::gStartsATwoKeyCommand()
{
    QVERIFY(m_router->press(u"g"_s));
    QCOMPARE(m_router->pending(), u"g"_s);
    QVERIFY(m_router->press(u"g"_s));
    QCOMPARE(m_router->pending(), QString());
    // A g that leads nowhere lets the next key act on its own.
    m_router->press(u"g"_s);
    QVERIFY(m_router->press(u"k"_s));
    QCOMPARE(triggered(), (QStringList{u"firstNote"_s, u"previousNote"_s}));
}

void TestKeyRouter::leaderTakesTheNextKey()
{
    QSignalSpy pending(m_router.get(), &KeyRouter::pendingChanged);
    QVERIFY(m_router->press(u","_s));
    QCOMPARE(m_router->pending(), u"leader"_s);
    QVERIFY(m_router->press(u"t"_s));
    // A key the leader does not know is swallowed, not acted on.
    m_router->press(u","_s);
    QVERIFY(m_router->press(u"j"_s));
    QCOMPARE(triggered(), QStringList{u"tasks"_s});
    QCOMPARE(m_router->pending(), QString());
    QCOMPARE(pending.size(), 4);

    // Another leader, here Space.
    m_router->setProperty("leaderKey", u" "_s);
    QVERIFY(!m_router->press(u","_s));
    m_router->press(u" "_s);
    m_router->press(u"n"_s);
    QCOMPARE(triggered().last(), u"newNote"_s);
}

void TestKeyRouter::leaderGivesUpAfterItsTimeout()
{
    m_router->setProperty("leaderTimeout", 50);
    m_router->press(u","_s);
    QTRY_COMPARE(m_router->pending(), QString());
    // Then the key is an ordinary one again.
    m_router->press(u"j"_s);
    QCOMPARE(triggered(), QStringList{u"nextNote"_s});

    // With no timeout, it waits until told to stop.
    m_router->setProperty("leaderTimeout", 0);
    m_router->press(u","_s);
    QTest::qWait(100);
    QCOMPARE(m_router->pending(), u"leader"_s);
    m_router->cancel();
    QCOMPARE(m_router->pending(), QString());
}

void TestKeyRouter::offLetsKeysThrough()
{
    m_router->press(u","_s);
    // Turning vi mode off drops a command half typed.
    m_router->setProperty("viMode", false);
    QCOMPARE(m_router->pending(), QString());
    QVERIFY(!m_router->press(u"j"_s));
    QVERIFY(triggered().isEmpty());
}

void TestKeyRouter::everyKeyMeansOneThing()
{
    QSet<QString> ids, leader;
    QHash<QString, QSet<QString>> vi;
    for (const KeyAction &action : KeyRouter::actions()) {
        QVERIFY2(!ids.contains(action.id), qPrintable(action.id));
        ids.insert(action.id);
        QVERIFY2(
            QStringList({u"window"_s, u"list"_s, u"note"_s, u"visual"_s}).contains(action.scope),
            qPrintable(action.id));
        // Each scope's vi keys are its own; a key may mean another thing elsewhere.
        if (!action.vi.isEmpty()) {
            QVERIFY2(!vi[action.scope].contains(action.vi), qPrintable(action.vi));
            vi[action.scope].insert(action.vi);
        }
        QVERIFY2(action.vi.isEmpty() || action.leader.isEmpty(), qPrintable(action.id));
        if (!action.leader.isEmpty()) {
            QVERIFY2(!leader.contains(action.leader), qPrintable(action.leader));
            leader.insert(action.leader);
        }
    }
    // No vi key may be a leader, or the leader could never be typed.
    for (const QString &scope : vi.keys()) {
        for (const QString &key : Settings::kLeaderKeys)
            QVERIFY2(!vi[scope].contains(key), qPrintable(scope + u' ' + key));
    }
    bool partial = false;
    QCOMPARE(KeyRouter::viAction(u"note"_s, u"d"_s, &partial), QString());
    QVERIFY(partial);
    QCOMPARE(KeyRouter::viAction(u"note"_s, u"dd"_s, &partial), u"deleteLine"_s);
    QVERIFY(!partial);
}

void TestKeyRouter::listsShortcutsAsThePlatformWritesThem()
{
    QHash<QString, QStringList> keys;
    for (const QVariant &action : KeyRouter::actionList()) {
        const QVariantMap map = action.toMap();
        keys.insert(map.value(u"id"_s).toString(), map.value(u"keys"_s).toStringList());
    }
    QCOMPARE(keys.value(u"goTo"_s), QStringList{u"Ctrl+P"_s});
    QVERIFY(keys.value(u"newNote"_s).contains(u"Ctrl+N"_s));
    // What is not a shortcut is shown as it is written.
    QCOMPARE(keys.value(u"wikilink"_s), QStringList{u"[["_s});
    QCOMPARE(keys.value(u"follow"_s), QStringList{u"Ctrl+Click"_s});
    QCOMPARE(keys.value(u"left"_s), QStringList());
}

void TestKeyRouter::paletteFindsCommandsByName()
{
    const auto ids = [](const QVariantList &commands) {
        QStringList found;
        for (const QVariant &command : commands)
            found << command.toMap().value(u"id"_s).toString();
        return found;
    };
    // With nothing typed, every window command in the table's order, but not the palette.
    const QStringList all = ids(KeyRouter::commands({}, {}));
    QCOMPARE(all.first(), u"newNote"_s);
    QVERIFY(all.contains(u"about"_s));
    QVERIFY(!all.contains(u"commands"_s));
    // Neither the note's keys, which belong to the editor.
    QVERIFY(!all.contains(u"bold"_s));

    const QStringList joplin = ids(KeyRouter::commands(u"joplin"_s, {}));
    QCOMPARE(joplin, QStringList{u"importJoplin"_s});
    // A command that cannot run now is left out.
    QVERIFY(ids(KeyRouter::commands(u"joplin"_s, {u"importJoplin"_s})).isEmpty());
    // The best match comes first, and the keys come along.
    const QVariantList newNote = KeyRouter::commands(u"new note"_s, {});
    QCOMPARE(newNote.first().toMap().value(u"id"_s), u"newNote"_s);
    QVERIFY(newNote.first().toMap().value(u"keys"_s).toStringList().contains(u"Ctrl+N"_s));
}

// Standard shortcuts are the platform theme's, so this needs a GUI application.
QTEST_MAIN(TestKeyRouter)
#include "tst_keyrouter.moc"
