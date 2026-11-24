#include "DesktopTextScale.h"
#include "ThemeController.h"

#include "greylock/Color.h"
#include "greylock/ThemeLoader.h"

#include <QDir>
#include <QFile>
#include <QFont>
#include <QFontDatabase>
#include <QFontInfo>
#include <QFontMetricsF>
#include <QSet>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <algorithm>
#include <cmath>

using namespace jot;
using namespace Qt::StringLiterals;

namespace {

void writeFile(const QString &path, const QByteArray &contents)
{
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write(contents);
}

} // namespace

class TestThemeController : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void noteFontsFallBackToGenericFamilies();
    void scalesItsTextAsTheDesktopAndSettingsSay();
    void readsTheDesktopsTextSize();
    void startsWithDefaultTheme();
    void loadsAFileAndReloadsOnEdit();
    void brokenEditKeepsLastGoodTheme();
    void builtInIdIgnoresSameNamedFile();
    void everyBuiltInThemeReads();
    void bundledFontsAreAvailable();
    void measuresWhatALineCannotBreak();
};

void TestThemeController::startsWithDefaultTheme()
{
    QCOMPARE(ThemeController::instance()->name(), greylock::ThemeLoader::defaultTheme().name);
}

void TestThemeController::loadsAFileAndReloadsOnEdit()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("mine.toml"));
    writeFile(path, "[theme]\nname = \"Mine\"\n[colors]\naccent = \"#5b8def\"\n");

    ThemeController *theme = ThemeController::instance();
    QVERIFY(theme->load(path).isEmpty());
    QCOMPARE(theme->name(), QStringLiteral("Mine"));
    QCOMPARE(theme->accent(), QColor("#5b8def"));

    QSignalSpy changed(theme, &ThemeController::changed);
    writeFile(path, "[theme]\nname = \"Mine\"\n[colors]\naccent = \"#2fa98c\"\n");
    QVERIFY(changed.wait(3000));
    QCOMPARE(theme->accent(), QColor("#2fa98c"));
}

void TestThemeController::brokenEditKeepsLastGoodTheme()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("mine.toml"));
    writeFile(path, "[colors]\naccent = \"#5b8def\"\n");
    ThemeController *theme = ThemeController::instance();
    QVERIFY(theme->load(path).isEmpty());

    QSignalSpy changed(theme, &ThemeController::changed);
    writeFile(path, "[colors]\naccent = \n");
    QTest::qWait(500);
    QCOMPARE(changed.size(), 0);
    QCOMPARE(theme->accent(), QColor("#5b8def"));
}

void TestThemeController::builtInIdIgnoresSameNamedFile()
{
    // A file called "callie" in the working directory must not be mistaken for
    // the built-in theme of that name and watched in its place.
    QTemporaryDir dir;
    const QString previous = QDir::currentPath();
    QDir::setCurrent(dir.path());
    const QString impostor = dir.filePath(QStringLiteral("callie"));
    writeFile(impostor, "not a theme");

    ThemeController *theme = ThemeController::instance();
    QVERIFY(theme->load(QStringLiteral("callie")).isEmpty());
    QSignalSpy changed(theme, &ThemeController::changed);
    writeFile(impostor, "[theme]\nname = \"Impostor\"\n");
    QTest::qWait(500);
    QCOMPARE(changed.size(), 0);
    QCOMPARE(theme->name(), QStringLiteral("Callie"));

    QDir::setCurrent(previous);
}

void TestThemeController::everyBuiltInThemeReads()
{
    ThemeController *theme = ThemeController::instance();
    for (const QString &id : greylock::ThemeLoader::builtInIds()) {
        QVERIFY2(theme->load(id).isEmpty(), qPrintable(id));
        // Text, accents and sticker ink all meet the contrast Jot asks of any theme.
        QVERIFY2(theme->warnings().isEmpty(),
                 qPrintable(id + u": "_s + theme->warnings().join(u"; "_s)));
        // A filled red button's label reads as body text does.
        QVERIFY2(greylock::color::contrastRatio(theme->dangerText(), theme->danger()) >= 4.5,
                 qPrintable(id));
        QCOMPARE(theme->dangerText() == theme->text() || theme->dangerText() == theme->bg(), true);
        // Vi's mode labels read on their stickers.
        QVERIFY2(greylock::color::contrastRatio(theme->insertModeText(), theme->insertMode()) >=
                     4.5,
                 qPrintable(id));
        QVERIFY2(greylock::color::contrastRatio(theme->visualModeText(), theme->visualMode()) >=
                     4.5,
                 qPrintable(id));
        // Diagram sections get eight colors of their own, each with text that reads on it.
        const jot::DiagramStyle diagram = theme->diagramStyle({}, 15);
        QSet<QString> distinct;
        for (const QColor &fill : diagram.series)
            distinct.insert(fill.name());
        QCOMPARE(distinct.size(), 8);
        for (const QColor &fill : diagram.series) {
            const double reads = std::max(greylock::color::contrastRatio(diagram.onLight, fill),
                                          greylock::color::contrastRatio(diagram.onDark, fill));
            QVERIFY2(reads >= 4.5, qPrintable(id + u": "_s + fill.name()));
        }
    }
    QVERIFY(theme->load(QStringLiteral("callie")).isEmpty());
}

void TestThemeController::bundledFontsAreAvailable()
{
    ThemeController::instance();
    // The bundled fonts, or the same typefaces installed on the system, which Qt then lists by
    // foundry, as "Nunito [NeWT]", rather than a stand-in.
    for (const QString &family : {QStringLiteral("Nunito"), QStringLiteral("Fraunces")}) {
        const QString found = QFontInfo(QFont(family)).family();
        QVERIFY2(found == family || found.startsWith(family + QStringLiteral(" [")),
                 qPrintable(found));
    }
}

void TestThemeController::noteFontsFallBackToGenericFamilies()
{
    ThemeController &theme = *ThemeController::instance();
    QVERIFY(theme.load(u"minimal"_s).isEmpty());
    // Minimal names no mono or display family of its own.
    QCOMPARE(theme.noteFontFamily(u"mono"_s), u"monospace"_s);
    QCOMPARE(theme.noteFontFamily(u"serif"_s), u"serif"_s);
    QCOMPARE(theme.noteFontFamily({}), theme.editorFontFamily());
    QVERIFY(theme.load(u"callie"_s).isEmpty());
    QCOMPARE(theme.noteFontFamily(u"serif"_s), theme.displayFontFamily());
}

void TestThemeController::scalesItsTextAsTheDesktopAndSettingsSay()
{
    ThemeController *theme = ThemeController::instance();
    QCOMPARE(theme->textLg(), 15);
    QSignalSpy changed(theme, &ThemeController::textScaleChanged);
    theme->setDesktopTextScale(1.25);
    theme->setUserTextScale(1.2);
    QCOMPARE(changed.size(), 2);
    QCOMPARE(theme->textScale(), 1.5);
    QCOMPARE(theme->textLg(), 23);
    QCOMPARE(theme->textSm(), 18);
    // Spacing stays as it is.
    QCOMPARE(theme->space4(), 12);
    theme->setUserTextScale(1.2);
    QCOMPARE(changed.size(), 2);
    theme->setDesktopTextScale(1);
    theme->setUserTextScale(1);
    QCOMPARE(theme->textLg(), 15);
}

void TestThemeController::readsTheDesktopsTextSize()
{
    QCOMPARE(DesktopTextScale::scaleFrom(std::nullopt, QString()), 1.0);
    // GNOME's Large Text, KDE's font a size up, and both.
    QCOMPARE(DesktopTextScale::scaleFrom(1.25, QString()), 1.25);
    QCOMPARE(DesktopTextScale::scaleFrom(1.0, u"Noto Sans,12,-1,5,50,0,0,0,0,0"_s), 1.2);
    QCOMPARE(DesktopTextScale::scaleFrom(1.25, u"Noto Sans,12"_s), 1.5);
    // What makes no sense is the usual size, and the scale stays in bounds.
    QCOMPARE(DesktopTextScale::scaleFrom(0.0, u"Noto Sans"_s), 1.0);
    QCOMPARE(DesktopTextScale::scaleFrom(5.0, QString()), 3.0);
}

QTEST_MAIN(TestThemeController)
void TestThemeController::measuresWhatALineCannotBreak()
{
    const ThemeController &theme = *ThemeController::instance();
    QFont font;
    font.setPixelSize(28);
    const QFontMetricsF metrics(font);
    // The widest word, without the space after it.
    QCOMPARE(theme.widestUnbreakable(QStringLiteral("Ferry Crossings at dawn"), font),
             metrics.horizontalAdvance(QStringLiteral("Crossings")));
    // Text written without spaces breaks between its characters, so no stretch is the whole.
    const QString japanese = QStringLiteral("\u6E21\u3057\u8239\u306E\u671D");
    QVERIFY(theme.widestUnbreakable(japanese, font) < metrics.horizontalAdvance(japanese) / 2);

    // Its own size when the widest word fits; smaller, but not below the smallest, when not.
    const qreal word = metrics.horizontalAdvance(QStringLiteral("Crossings"));
    QCOMPARE(theme.fittingPixelSize(QStringLiteral("Crossings"), font, word + 1, 20), 28);
    const int fitted = theme.fittingPixelSize(QStringLiteral("Crossings"), font, word * 0.8, 20);
    QVERIFY(fitted < 28 && fitted >= 20);
    QCOMPARE(theme.fittingPixelSize(QStringLiteral("Crossings"), font, word * 0.1, 20), 20);
    QCOMPARE(theme.fittingPixelSize(japanese, font, metrics.horizontalAdvance(japanese) / 2, 20),
             28);
}

#include "tst_themecontroller.moc"
