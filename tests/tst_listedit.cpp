#include "jot/ListEdit.h"

#include <QTest>

using namespace jot;
using namespace Qt::StringLiterals;

class TestListEdit : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void movesTickedToTheEnd();
    void removesTicked();
    void untick();
    void sortsTheListHoldingTheCursor();
    void leavesCodeAlone();
};

void TestListEdit::movesTickedToTheEnd()
{
    // Each list on its own, nested ones too, with what is under an item going along.
    const QString before = u"- [ ] milk\n- [x] eggs\n  - [x] brown\n  - [ ] white\n- [ ] bread\n\n"
                           u"Between.\n\n- [x] one\n- [ ] two\n"_s;
    QCOMPARE(moveTickedToEnd(before),
             u"- [ ] milk\n- [ ] bread\n- [x] eggs\n  - [ ] white\n  - [x] brown\n\n"
             u"Between.\n\n- [ ] two\n- [x] one\n"_s);
    // Numbered lists number again.
    QCOMPARE(moveTickedToEnd(u"1. [x] a\n2. [ ] b\n3. [ ] c"_s), u"1. [ ] b\n2. [ ] c\n3. [x] a"_s);
    // Nothing ticked, nothing changes.
    QCOMPARE(moveTickedToEnd(u"- [ ] a\n- b\n"_s), u"- [ ] a\n- b\n"_s);
}

void TestListEdit::removesTicked()
{
    QCOMPARE(removeTicked(u"- [ ] milk\n- [x] eggs\n  - [ ] under it\n- [ ] bread\n\nEnd.\n"_s),
             u"- [ ] milk\n- [ ] bread\n\nEnd.\n"_s);
    QCOMPARE(removeTicked(u"- [X] gone\n"_s), u""_s);
}

void TestListEdit::untick()
{
    QCOMPARE(untickAll(u"- [x] a\n  - [X] b\n- [ ] c\nNot [x] a to-do.\n"_s),
             u"- [ ] a\n  - [ ] b\n- [ ] c\nNot [x] a to-do.\n"_s);
}

void TestListEdit::sortsTheListHoldingTheCursor()
{
    const QString body = u"Intro.\n\n- pear\n- apple\n  - zest\n  - core\n- Banana\n\nAfter.\n"_s;
    // On the outer list: it sorts, in any case, its items' lines going along.
    QCOMPARE(sortList(body, int(body.indexOf(u"pear"))),
             u"Intro.\n\n- apple\n  - zest\n  - core\n- Banana\n- pear\n\nAfter.\n"_s);
    // On the inner list: only that one.
    QCOMPARE(sortList(body, int(body.indexOf(u"zest"))),
             u"Intro.\n\n- pear\n- apple\n  - core\n  - zest\n- Banana\n\nAfter.\n"_s);
    // Numbers sort as numbers.
    QCOMPARE(sortList(u"- item 10\n- item 9\n"_s, 0), u"- item 9\n- item 10\n"_s);
    // Not in a list, nothing changes.
    QCOMPARE(sortList(body, 2), body);
}

void TestListEdit::leavesCodeAlone()
{
    const QString code = u"```\n- [x] in code\n- [ ] also\n```\n"_s;
    QCOMPARE(moveTickedToEnd(code), code);
    QCOMPARE(removeTicked(code), code);
    QCOMPARE(untickAll(code), code);
}

QTEST_GUILESS_MAIN(TestListEdit)
#include "tst_listedit.moc"
