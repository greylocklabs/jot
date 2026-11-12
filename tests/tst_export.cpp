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
    QCOMPARE(
        exportMarkdown(u"---\ncreated: 2026-11-09T10:00:00Z\n---\n# Ferry\n\nSee [Mara](Mara.md) "
                       "and [[Soren]].\n"_s,
                       embed, &toc),
        u"# Ferry\n\nSee [Mara](Mara.md) and [[Soren]].\n"_s);
    QVERIFY(!toc);
    // Pictures are found in the vault, edits are made and comments go; the contents marker asks
    // for one.
    QCOMPARE(
        exportMarkdown(u"${toc}\n![map.png](map.png) ![gone.png](gone.png)\nA {++new++}{--old--} "
                       "{~~cat~>dog~~} {==bright==}{>>check<<}\n"_s,
                       embed, &toc),
        u"![map.png](</v/map.png>) gone.png\nA new dog bright\n"_s);
    QVERIFY(toc);
    // Only pictures in the vault reach Pandoc; others, on this computer or the web, are their
    // words, and a reference to a file outside the vault loses its path.
    QCOMPARE(exportMarkdown(
                 u"![Map](map.png) ![key](/home/me/.ssh/id_rsa) ![web](https://x.example/p.png)\n"
                 "[k]: /etc/hostname\n[m]: map.png \"Map\"\n[w]: https://x.example\n"_s,
                 embed, &toc),
             u"![Map](</v/map.png>) key web\n[k]: #\n[m]: </v/map.png> \"Map\"\n"
             "[w]: https://x.example\n"_s);
    // Every way of writing a picture's title is read, and one that cannot be read is a link.
    QCOMPARE(
        exportMarkdown(u"![a](/etc/a 'x') ![b](/etc/b (x)) ![c](/etc/c \"x\") ![d](map.png 'x')\n"
                       "![e](/etc/e \"unclosed)\n"_s,
                       embed, &toc),
        u"a b c ![d](</v/map.png>)\n[e](/etc/e \"unclosed)\n"_s);
    // A footnote is no reference: its words stay.
    QCOMPARE(exportMarkdown(u"See[^1].\n\n[^1]: Some words here.\n"_s, embed, &toc),
             u"See[^1].\n\n[^1]: Some words here.\n"_s);
    // Code is left as written.
    const QString code = u"`![x](not.png) {++x++}`\n```\n![x](map.png) ${toc}\n~~~\n${toc}\n```\n"
                         "    ![x](indented.png)\n"_s;
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
    // Raw TeX and HTML are never read, as they could reach the computer's files.
    QCOMPARE(docx.first(), u"--from=markdown-raw_tex-raw_html-raw_attribute"_s);
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

    // TeX, making a PDF, reads no file a note's math names outside its own folder.
    QVERIFY(pandocEnvironment().contains(u"openin_any=p"_s));
    QVERIFY(pandocEnvironment().contains(u"openout_any=p"_s));
}

QTEST_GUILESS_MAIN(TestExport)
#include "tst_export.moc"
