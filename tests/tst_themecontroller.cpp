#include "ThemeController.h"

#include "greylock/Color.h"
#include "greylock/ThemeLoader.h"

#include <QDir>
#include <QFile>
#include <QFontDatabase>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

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
    void startsWithDefaultTheme();
    void loadsAFileAndReloadsOnEdit();
    void brokenEditKeepsLastGoodTheme();
    void builtInIdIgnoresSameNamedFile();
    void everyBuiltInThemeReads();
    void bundledFontsAreAvailable();
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
    }
    QVERIFY(theme->load(QStringLiteral("callie")).isEmpty());
}

void TestThemeController::bundledFontsAreAvailable()
{
    ThemeController::instance();
    const QStringList families = QFontDatabase::families();
    QVERIFY(families.contains(QStringLiteral("Nunito")));
    QVERIFY(families.contains(QStringLiteral("Fraunces")));
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

QTEST_MAIN(TestThemeController)
#include "tst_themecontroller.moc"
