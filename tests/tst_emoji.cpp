#include "EmojiList.h"

#include <QTest>

using namespace jot;
using namespace Qt::StringLiterals;

class TestEmoji : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void findsByTheStartsOfWords();
};

void TestEmoji::findsByTheStartsOfWords()
{
    const auto emoji = [](const QString &query) {
        QStringList found;
        for (const QVariant &each : EmojiList::matching(query))
            found << each.toMap().value(u"emoji"_s).toString();
        return found;
    };
    // Faces come first when nothing is typed.
    QCOMPARE(emoji({}).first(), u"😀"_s);
    QVERIFY(emoji({}).size() > 1000);
    QVERIFY(emoji(u"fox"_s).contains(u"🦊"_s));
    // Every word must start one in the name, in any order and case.
    QVERIFY(emoji(u"Face FOX"_s).contains(u"🦊"_s));
    QVERIFY(!emoji(u"ox"_s).contains(u"🦊"_s));
    QVERIFY(emoji(u"zzzz"_s).isEmpty());
    // One drawn as text by default comes as an emoji.
    QVERIFY(emoji(u"heavy heart"_s).contains(u"❤️"_s));
}

QTEST_GUILESS_MAIN(TestEmoji)
#include "tst_emoji.moc"
