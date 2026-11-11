#include "jot/FrontMatter.h"

#include <QTest>

using namespace Qt::StringLiterals;

class TestFrontMatter : public QObject
{
    Q_OBJECT

private slots:
    void splits_data();
    void splits();
    void byteOrderMarkStaysOutOfTheYaml();

    void readsScalarsAndLists();
    void keepsRicherValuesAsText();

    void setsAKeyInPlace();
    void addsAMissingKey();
    void removesAKeyAndItsLines();
    void keepsABlockListABlockList();
    void givesFrontMatterToANoteWithout();
    void keepsLineEndingsAndTheByteOrderMark();
    void quotesOnlyWhatNeedsIt_data();
    void quotesOnlyWhatNeedsIt();
    void valuesSurviveARoundTrip();
};

void TestFrontMatter::splits_data()
{
    QTest::addColumn<QString>("note");
    QTest::addColumn<bool>("has");
    QTest::addColumn<QString>("yaml");
    QTest::addColumn<QString>("body");

    QTest::newRow("none") << u"# Title\nText\n"_s << false << QString() << u"# Title\nText\n"_s;
    QTest::newRow("simple") << u"---\ntags: [a]\n---\nBody\n"_s << true << u"tags: [a]\n"_s
                            << u"Body\n"_s;
    QTest::newRow("dots close") << u"---\na: 1\n...\nBody"_s << true << u"a: 1\n"_s << u"Body"_s;
    QTest::newRow("crlf") << u"---\r\na: 1\r\n---\r\nBody"_s << true << u"a: 1\r\n"_s << u"Body"_s;
    QTest::newRow("empty") << u"---\n---\nBody"_s << true << QString() << u"Body"_s;
    QTest::newRow("at end") << u"---\na: 1\n---"_s << true << u"a: 1\n"_s << QString();
    QTest::newRow("unclosed") << u"---\na: 1\nBody"_s << false << QString() << u"---\na: 1\nBody"_s;
    QTest::newRow("not first") << u"\n---\na: 1\n---\n"_s << false << QString()
                               << u"\n---\na: 1\n---\n"_s;
    QTest::newRow("dots do not open")
        << u"...\na: 1\n...\n"_s << false << QString() << u"...\na: 1\n...\n"_s;
    QTest::newRow("rule in body") << u"Text\n\n---\n\nMore"_s << false << QString()
                                  << u"Text\n\n---\n\nMore"_s;
}

void TestFrontMatter::splits()
{
    QFETCH(QString, note);
    QFETCH(bool, has);
    QFETCH(QString, yaml);
    QFETCH(QString, body);

    const jot::NoteParts parts = jot::splitFrontMatter(note);
    QCOMPARE(parts.hasFrontMatter, has);
    QCOMPARE(parts.frontMatter.toString(), yaml);
    QCOMPARE(parts.body.toString(), body);
}

void TestFrontMatter::byteOrderMarkStaysOutOfTheYaml()
{
    const QString note = u"﻿---\na: 1\n---\nBody"_s;
    const jot::NoteParts parts = jot::splitFrontMatter(note);
    QVERIFY(parts.hasFrontMatter);
    QCOMPARE(parts.frontMatter.toString(), u"a: 1\n"_s);
    QCOMPARE(jot::splitFrontMatter(u"﻿Body"_s).body.toString(), u"Body"_s);
}

void TestFrontMatter::readsScalarsAndLists()
{
    const QVariantMap values = jot::readFrontMatter(uR"(# a comment
title: "Q&A / Mara: \"the\" ferry"
plain: Mara Vey # trailing comment
single: 'it''s'
tags: [ashfall, "cast/lead", 'a, b']
aliases:
  - Mara
  - "the Ferrywoman"
empty:
created: 2026-10-14T15:30:00Z
)"_s);
    QCOMPARE(values.value(u"title"_s).toString(), uR"(Q&A / Mara: "the" ferry)"_s);
    QCOMPARE(values.value(u"plain"_s).toString(), u"Mara Vey"_s);
    QCOMPARE(values.value(u"single"_s).toString(), u"it's"_s);
    QCOMPARE(values.value(u"tags"_s).toStringList(),
             (QStringList{u"ashfall"_s, u"cast/lead"_s, u"a, b"_s}));
    QCOMPARE(values.value(u"aliases"_s).toStringList(),
             (QStringList{u"Mara"_s, u"the Ferrywoman"_s}));
    QCOMPARE(values.value(u"empty"_s).toString(), QString());
    QCOMPARE(values.value(u"created"_s).toString(), u"2026-10-14T15:30:00Z"_s);
}

void TestFrontMatter::keepsRicherValuesAsText()
{
    const QVariantMap values = jot::readFrontMatter(u"relationships:\n  - to: Soren\n    kind: "
                                                    u"brother\nnext: 1\n"_s);
    QCOMPARE(values.value(u"relationships"_s).toString(), u"  - to: Soren\n    kind: brother"_s);
    QCOMPARE(values.value(u"next"_s).toString(), u"1"_s);
}

void TestFrontMatter::setsAKeyInPlace()
{
    const QString note = u"---\n# mine\ntitle: Old  # keep me? no\ntags: [a]\n---\nBody\n"_s;
    QCOMPARE(jot::setFrontMatterKey(note, u"title"_s, u"New"_s),
             u"---\n# mine\ntitle: New\ntags: [a]\n---\nBody\n"_s);
}

void TestFrontMatter::addsAMissingKey()
{
    const QString note = u"---\ntitle: T\n---\nBody\n"_s;
    QCOMPARE(jot::setFrontMatterKey(note, u"tags"_s, QStringList{u"a"_s, u"b, c"_s}),
             u"---\ntitle: T\ntags: [a, \"b, c\"]\n---\nBody\n"_s);
    // Booleans and numbers are written as they are, not as text.
    QCOMPARE(jot::setFrontMatterKey(note, u"every"_s, 15),
             u"---\ntitle: T\nevery: 15\n---\nBody\n"_s);
    QCOMPARE(jot::setFrontMatterKey(note, u"words"_s, 1234567),
             u"---\ntitle: T\nwords: 1234567\n---\nBody\n"_s);
    QCOMPARE(jot::setFrontMatterKey(note, u"ratio"_s, 0.1),
             u"---\ntitle: T\nratio: 0.1\n---\nBody\n"_s);
    QCOMPARE(jot::setFrontMatterKey(note, u"hooks"_s, false),
             u"---\ntitle: T\nhooks: false\n---\nBody\n"_s);
}

void TestFrontMatter::removesAKeyAndItsLines()
{
    const QString note = u"---\naliases:\n  - A\n  - B\ntitle: T\n---\nBody"_s;
    QCOMPARE(jot::setFrontMatterKey(note, u"aliases"_s, QVariant()), u"---\ntitle: T\n---\nBody"_s);
    QCOMPARE(jot::setFrontMatterKey(note, u"missing"_s, QVariant()), note);
}

void TestFrontMatter::keepsABlockListABlockList()
{
    const QString note = u"---\ntags:\n    - a\n    - b\nx: 1\n---\n"_s;
    QCOMPARE(jot::setFrontMatterKey(note, u"tags"_s, QStringList{u"a"_s, u"c"_s}),
             u"---\ntags:\n    - a\n    - c\nx: 1\n---\n"_s);
}

void TestFrontMatter::givesFrontMatterToANoteWithout()
{
    QCOMPARE(jot::setFrontMatterKey(u"# Hi\n"_s, u"task"_s, u"open"_s),
             u"---\ntask: open\n---\n# Hi\n"_s);
    QCOMPARE(jot::setFrontMatterKey(u"# Hi\n"_s, u"task"_s, QVariant()), u"# Hi\n"_s);
}

void TestFrontMatter::keepsLineEndingsAndTheByteOrderMark()
{
    QCOMPARE(jot::setFrontMatterKey(u"﻿---\r\na: 1\r\n---\r\nBody\r\n"_s, u"b"_s, u"2"_s),
             u"﻿---\r\na: 1\r\nb: 2\r\n---\r\nBody\r\n"_s);
    QCOMPARE(jot::setFrontMatterKey(u"﻿Body\r\n"_s, u"b"_s, u"2"_s),
             u"﻿---\r\nb: 2\r\n---\r\nBody\r\n"_s);
}

void TestFrontMatter::quotesOnlyWhatNeedsIt_data()
{
    QTest::addColumn<QString>("text");
    QTest::addColumn<QString>("yaml");
    QTest::newRow("plain") << u"Mara Vey"_s << u"Mara Vey"_s;
    QTest::newRow("date") << u"2026-10-14"_s << u"2026-10-14"_s;
    QTest::newRow("colon space") << u"Q&A: Mara"_s << u"\"Q&A: Mara\""_s;
    QTest::newRow("boolean") << u"yes"_s << u"\"yes\""_s;
    QTest::newRow("leading dash") << u"- item"_s << u"\"- item\""_s;
    QTest::newRow("empty") << QString() << u"\"\""_s;
    QTest::newRow("newline") << u"a\nb"_s << u"\"a\\nb\""_s;
    QTest::newRow("quote inside") << u"say \"hi\""_s << u"say \"hi\""_s;
    QTest::newRow("hash") << u"#tag"_s << u"\"#tag\""_s;
}

void TestFrontMatter::quotesOnlyWhatNeedsIt()
{
    QFETCH(QString, text);
    QFETCH(QString, yaml);
    QCOMPARE(jot::yamlScalar(text), yaml);
}

void TestFrontMatter::valuesSurviveARoundTrip()
{
    const QStringList awkward{u"plain"_s,    u"a: b"_s,       u"[x]"_s, u"line\nbreak"_s,
                              u"'single'"_s, u"\"double\""_s, u"no"_s,  u"c, d"_s};
    QString note = u"Body\n"_s;
    for (qsizetype i = 0; i < awkward.size(); ++i)
        note = jot::setFrontMatterKey(note, u"k%1"_s.arg(i), awkward.at(i));
    note = jot::setFrontMatterKey(note, u"list"_s, awkward);

    const QVariantMap values = jot::readFrontMatter(jot::splitFrontMatter(note).frontMatter);
    for (qsizetype i = 0; i < awkward.size(); ++i)
        QCOMPARE(values.value(u"k%1"_s.arg(i)).toString(), awkward.at(i));
    QCOMPARE(values.value(u"list"_s).toStringList(), awkward);
    QCOMPARE(jot::splitFrontMatter(note).body.toString(), u"Body\n"_s);
}

QTEST_GUILESS_MAIN(TestFrontMatter)
#include "tst_frontmatter.moc"
