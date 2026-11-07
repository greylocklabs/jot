#include "jot/Diagram.h"

#include <QFile>
#include <QImage>
#include <QImageReader>
#include <QTest>

using namespace Qt::StringLiterals;
using jot::Diagram;
using jot::DiagramStyle;

class TestDiagram : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void drawsEachKind_data();
    void drawsEachKind();
    void writesOutWhatQtCannotRead();
    void drawsInTheThemeColors();
    void fillsSectionsInTheThemeColors();
    void keepsEmojiWhole();
    void keepsLabelsAsWritten();
    void saysWhatIsWrong();
    void drawsTheSameTextOnce();
    void namesDiagramLanguages();
};

namespace {

DiagramStyle style()
{
    DiagramStyle style;
    style.background = QColor(u"#fdf6f0"_s);
    style.text = QColor(u"#3a2530"_s);
    style.muted = QColor(u"#8a6d78"_s);
    style.surface = QColor(u"#f6e4ea"_s);
    style.surfaceAlt = QColor(u"#eccfd9"_s);
    style.border = QColor(u"#c48a9f"_s);
    style.accent = QColor(u"#d4537e"_s);
    style.pixelSize = 15;
    return style;
}

QString svgOf(const Diagram &diagram)
{
    QFile file(diagram.path);
    return file.open(QIODevice::ReadOnly) ? QString::fromUtf8(file.readAll()) : QString();
}

QImage imageOf(const Diagram &diagram)
{
    QImageReader reader(diagram.path);
    return reader.read();
}

} // namespace

void TestDiagram::drawsEachKind_data()
{
    QTest::addColumn<QString>("source");
    QTest::newRow("flowchart") << u"flowchart LR\n  A[Write] --> B{Done?}\n  B -- yes --> C"_s;
    QTest::newRow("sequence") << u"sequenceDiagram\n  Jot->>Vault: save\n  Vault-->>Jot: ok"_s;
    QTest::newRow("class") << u"classDiagram\n  class Note { +title: string }"_s;
    QTest::newRow("state") << u"stateDiagram-v2\n  [*] --> Draft\n  Draft --> [*]"_s;
    QTest::newRow("er") << u"erDiagram\n  NOTEBOOK ||--o{ NOTE : holds"_s;
    QTest::newRow("gantt") << u"gantt\n  dateFormat YYYY-MM-DD\n  Tables :a, 2026-11-01, 4d"_s;
    QTest::newRow("pie") << u"pie title Notes\n  \"Work\" : 40\n  \"Home\" : 60"_s;
    QTest::newRow("mindmap") << u"mindmap\n  root((Jot))\n    Notes\n    Tasks"_s;
    QTest::newRow("timeline") << u"timeline\n  2026-10 : 0.1.0\n  2026-11 : 0.2.0"_s;
    QTest::newRow("git") << u"gitGraph\n  commit\n  branch feat\n  commit"_s;
    QTest::newRow("xychart") << u"xychart-beta\n  x-axis [a, b]\n  bar [3, 5]"_s;
}

void TestDiagram::drawsEachKind()
{
    QFETCH(QString, source);
    const Diagram diagram = jot::drawDiagram(source, style());
    QVERIFY2(diagram.ok(), qPrintable(diagram.error));
    QVERIFY(diagram.size.width() > 0 && diagram.size.height() > 0);
    const QImage image = imageOf(diagram);
    QVERIFY(!image.isNull());
    QCOMPARE(image.size(), diagram.size);
}

void TestDiagram::writesOutWhatQtCannotRead()
{
    const QString svg = svgOf(jot::drawDiagram(u"flowchart LR\n  A(Round) --> B"_s, style()));
    QVERIFY(!svg.isEmpty());
    // Labels here say no var(, so any left would be a style Qt cannot read.
    QVERIFY(!svg.contains(u"var("_s));
    QVERIFY(!svg.contains(u"style=\"rx:"_s));
    QVERIFY(svg.contains(u"font-size=\"15\""_s));
    QVERIFY(svg.contains(u"font-family=\""_s));
}

void TestDiagram::drawsInTheThemeColors()
{
    const Diagram diagram = jot::drawDiagram(u"flowchart LR\n  A[Box] --> B[Box]"_s, style());
    const QString svg = svgOf(diagram);
    QVERIFY(svg.contains(u"#f6e4ea"_s));
    QVERIFY(svg.contains(u"#3a2530"_s));
    // A box is filled in the surface color, not left black as unread CSS would leave it.
    const QImage image = imageOf(diagram).convertToFormat(QImage::Format_ARGB32);
    int surface = 0;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x)
            surface += image.pixelColor(x, y).name() == u"#f6e4ea"_s;
    }
    QVERIFY2(surface > image.width() * image.height() / 10, qPrintable(QString::number(surface)));
}

void TestDiagram::fillsSectionsInTheThemeColors()
{
    DiagramStyle themed = style();
    themed.series = {QColor(u"#f2b8c9"_s), QColor(u"#b8e0c9"_s), QColor(u"#2b2450"_s)};
    themed.onLight = QColor(u"#101010"_s);
    themed.onDark = QColor(u"#fafafa"_s);
    for (const QString &source :
         {u"timeline\n  title Ashfall\n  Act one : Mara\n  Act two : Soren\n  Act three : Debt"_s,
          u"pie title Notes\n  \"Work\" : 40\n  \"Home\" : 35\n  \"Play\" : 25"_s}) {
        const QString svg = svgOf(jot::drawDiagram(source, themed));
        for (const QColor &fill : themed.series)
            QVERIFY2(svg.contains(fill.name()), qPrintable(source + u": "_s + fill.name()));
        // None of Pele's own palette is left.
        for (const QString &own : {u"#4c78a8"_s, u"#f58518"_s, u"#54a24b"_s})
            QVERIFY2(!svg.contains(own), qPrintable(source + u": "_s + own));
    }
    // Section names are dark on light fills and light on the dark one.
    const QString timeline =
        svgOf(jot::drawDiagram(u"timeline\n  One : a\n  Two : b\n  Three : c"_s, themed));
    QVERIFY(timeline.contains(u"fill=\"#101010\""_s));
    QVERIFY(timeline.contains(u"fill=\"#fafafa\""_s));
}

void TestDiagram::keepsEmojiWhole()
{
    const QString svg =
        svgOf(jot::drawDiagram(u"flowchart LR\n  A[Practice \U0001F3B7 daily]"_s, style()));
    QVERIFY(svg.contains(u"Practice \U0001F3B7 daily"_s));
    const QString mind = svgOf(jot::drawDiagram(u"mindmap\n  root\n    Zoë \U0001F3B7"_s, style()));
    QVERIFY(mind.contains(u"Zoë \U0001F3B7"_s));
}

void TestDiagram::keepsLabelsAsWritten()
{
    // Only the drawing's own styles are filled in, never what a label says. Short labels, so
    // no font wraps them.
    const QString svg = svgOf(jot::drawDiagram(
        u"flowchart LR\n  A[\"var(x)\"] --> B[\"var(--_fg)\"] --> C[\"f(var(--pele-bg))\"]"_s,
        style()));
    QVERIFY2(svg.contains(u">var(x)<"_s), qPrintable(svg));
    QVERIFY(svg.contains(u">var(--_fg)<"_s));
    QVERIFY(svg.contains(u">f(var(--pele-bg))<"_s));
}

void TestDiagram::saysWhatIsWrong()
{
    const Diagram diagram = jot::drawDiagram(u"flowchart LR\n  A[open --> B"_s, style());
    QVERIFY(!diagram.ok());
    QVERIFY(!diagram.error.isEmpty());
    QVERIFY(!jot::drawDiagram(u"not a diagram"_s, style()).ok());
}

void TestDiagram::drawsTheSameTextOnce()
{
    const QString source = u"flowchart LR\n  A --> B"_s;
    const Diagram first = jot::drawDiagram(source, style());
    QCOMPARE(jot::drawDiagram(source, style()).path, first.path);
    DiagramStyle dark = style();
    dark.surface = QColor(u"#2a1d24"_s);
    const Diagram other = jot::drawDiagram(source, dark);
    QVERIFY(other.ok());
    QVERIFY(other.path != first.path);
    QVERIFY(svgOf(other).contains(u"#2a1d24"_s));
}

void TestDiagram::namesDiagramLanguages()
{
    QVERIFY(jot::isDiagramLanguage(u"mermaid"));
    QVERIFY(jot::isDiagramLanguage(u"Mermaid"));
    QVERIFY(!jot::isDiagramLanguage(u"json"));
    QVERIFY(!jot::isDiagramLanguage(u""));
}

QTEST_MAIN(TestDiagram)
#include "tst_diagram.moc"
