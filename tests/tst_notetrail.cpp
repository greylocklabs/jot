#include "jot/NoteTrail.h"

#include <QSet>
#include <QTest>

using namespace Qt::StringLiterals;
using jot::NoteTrail;

class TestNoteTrail : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void goesBackAndForwardToWhereTheCursorWas();
    void dropsWhatWasAheadOnANewVisit();
    void skipsNotesGoneSince();
    void followsMoves();
    void keepsTheLatest();
};

namespace {

const auto everything = [](const QString &) { return true; };

} // namespace

void TestNoteTrail::goesBackAndForwardToWhereTheCursorWas()
{
    NoteTrail trail;
    QVERIFY(!trail.canGoBack());
    trail.visit(u"a.md"_s, 0);
    trail.visit(u"b.md"_s, 12);
    // Opening the note already shown is no visit.
    trail.visit(u"b.md"_s, 0);
    trail.visit(u"c.md"_s, 30);
    QVERIFY(trail.canGoBack());
    QVERIFY(!trail.canGoForward());

    QCOMPARE(trail.back(5, everything), std::optional(NoteTrail::Visit{u"b.md"_s, 30}));
    QCOMPARE(trail.back(31, everything), std::optional(NoteTrail::Visit{u"a.md"_s, 12}));
    QVERIFY(!trail.back(0, everything));
    QVERIFY(trail.canGoForward());
    QCOMPARE(trail.forward(2, everything), std::optional(NoteTrail::Visit{u"b.md"_s, 31}));
    QCOMPARE(trail.forward(0, everything), std::optional(NoteTrail::Visit{u"c.md"_s, 5}));
    QVERIFY(!trail.forward(0, everything));
}

void TestNoteTrail::dropsWhatWasAheadOnANewVisit()
{
    NoteTrail trail;
    trail.visit(u"a.md"_s, 0);
    trail.visit(u"b.md"_s, 0);
    trail.visit(u"c.md"_s, 0);
    QVERIFY(trail.back(0, everything));
    trail.visit(u"d.md"_s, 0);
    QVERIFY(!trail.canGoForward());
    QCOMPARE(trail.back(0, everything)->path, u"b.md"_s);
    QCOMPARE(trail.back(0, everything)->path, u"a.md"_s);
}

void TestNoteTrail::skipsNotesGoneSince()
{
    NoteTrail trail;
    for (const QString &path : {u"a.md"_s, u"b.md"_s, u"a.md"_s, u"c.md"_s, u"d.md"_s})
        trail.visit(path, 0);
    QSet<QString> gone{u"c.md"_s};
    const auto exists = [&gone](const QString &path) { return !gone.contains(path); };
    QCOMPARE(trail.back(0, exists)->path, u"a.md"_s);
    // With b gone too, all that is behind is the note shown.
    gone.insert(u"b.md"_s);
    QVERIFY(!trail.back(0, exists));
    QCOMPARE(trail.visits().size(), 2);
    QCOMPARE(trail.forward(0, exists)->path, u"d.md"_s);
    // Nothing left that exists, nothing to go to.
    gone.insert(u"a.md"_s);
    QVERIFY(!trail.back(0, exists));
    QVERIFY(!trail.canGoBack());
}

void TestNoteTrail::followsMoves()
{
    NoteTrail trail;
    trail.visit(u"Ashfall/Mara.md"_s, 0);
    trail.visit(u"Ashfall/Cast/Soren.md"_s, 0);
    trail.visit(u"Ashfallen.md"_s, 0);
    trail.moved(u"Ashfall"_s, u"Novels/Ashfall"_s);
    trail.moved(u"Ashfallen.md"_s, u"Inbox/Ashfallen.md"_s);
    QCOMPARE(trail.visits().at(0).path, u"Novels/Ashfall/Mara.md"_s);
    QCOMPARE(trail.visits().at(1).path, u"Novels/Ashfall/Cast/Soren.md"_s);
    QCOMPARE(trail.visits().at(2).path, u"Inbox/Ashfallen.md"_s);
}

void TestNoteTrail::keepsTheLatest()
{
    NoteTrail trail;
    for (int each = 0; each < NoteTrail::kLimit + 10; ++each)
        trail.visit(u"%1.md"_s.arg(each), 0);
    QCOMPARE(trail.visits().size(), NoteTrail::kLimit);
    QCOMPARE(trail.visits().first().path, u"10.md"_s);
    QVERIFY(trail.canGoBack());
}

QTEST_GUILESS_MAIN(TestNoteTrail)
#include "tst_notetrail.moc"
