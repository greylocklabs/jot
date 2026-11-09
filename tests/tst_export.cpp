#include "jot/Export.h"

#include <QTest>

using namespace jot;
using namespace Qt::StringLiterals;

class TestExport : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void writesTheNoteForPandoc();
    void asksPandocForTheLook();
};

void TestExport::writesTheNoteForPandoc()
{
    const auto embed = [](const QString &target) {
        return target == u"map.png"_s ? u"/v/map.png"_s : QString();
    };
    bool toc = true;
    QCOMPARE(exportMarkdown(u"---\ncreated: 2026-11-09T10:00:00Z\n---\n# Ferry\n\nSee [[Mara]] and "
                            "[[Cast/Soren|her brother]].\n"_s,
                            embed, &toc),
             u"# Ferry\n\nSee Mara and her brother.\n"_s);
    QVERIFY(!toc);
    // Embeds become images, edits are made and comments go; the contents marker asks for one.
    QCOMPARE(exportMarkdown(u"${toc}\n![[map.png]] ![[gone.png]]\nA {++new++}{--old--} "
                            "{~~cat~>dog~~} {==bright==}{>>check<<}\n"_s,
                            embed, &toc),
             u"![map.png](</v/map.png>) gone.png\nA new dog bright\n"_s);
    QVERIFY(toc);
    // Code is left as written.
    const QString code = u"`[[not]] {++x++}`\n```\n[[Mara]] ${toc}\n${toc}\n```\n"_s;
    QCOMPARE(exportMarkdown(code, embed, &toc), code);
    QVERIFY(!toc);
}

void TestExport::asksPandocForTheLook()
{
    const auto args = [](const QString &format, const QString &preset, bool toc = false) {
        return pandocArguments(format, preset, u"/w/note.md"_s, u"/out/Ferry."_s + format,
                               u"Ferry"_s, toc, u"/v/Places"_s, u"/w"_s);
    };
    const QStringList docx = args(u"docx"_s, u"standard"_s);
    QCOMPARE(docx.first(), u"--from=markdown"_s);
    QVERIFY(docx.contains(u"--to=docx"_s));
    QVERIFY(docx.contains(u"--output=/out/Ferry.docx"_s));
    QVERIFY(docx.contains(u"--resource-path=/v/Places"_s));
    QVERIFY(docx.contains(u"--metadata=title:Ferry"_s));
    QCOMPARE(docx.last(), u"/w/note.md"_s);
    QVERIFY(!docx.join(u' ').contains(u"reference-doc"_s));
    QVERIFY(args(u"html"_s, u"standard"_s).contains(u"--embed-resources"_s));
    QVERIFY(args(u"epub"_s, u"standard"_s, true).contains(u"--toc"_s));

    QVERIFY(args(u"docx"_s, u"manuscript"_s).contains(u"--reference-doc=/w/manuscript.docx"_s));
    QVERIFY(args(u"odt"_s, u"manuscript"_s).contains(u"--reference-doc=/w/manuscript.odt"_s));
    QVERIFY(args(u"pdf"_s, u"manuscript"_s).contains(u"--variable=fontfamily:courier"_s));
    QVERIFY(args(u"epub"_s, u"manuscript"_s).contains(u"--css=/w/manuscript.css"_s));
}

QTEST_GUILESS_MAIN(TestExport)
#include "tst_export.moc"
