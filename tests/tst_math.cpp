#include "MathRenderer.h"

#include <QGuiApplication>
#include <QImage>
#include <QTemporaryDir>
#include <QTest>

using namespace jot;
using namespace Qt::StringLiterals;

class TestMath : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void drawsFormulas();
    void saysWhatIsWrong();
    void leavesDefinitionsAsWritten();

private:
    QTemporaryDir m_dir;
};

void TestMath::initTestCase()
{
    // The fonts and drawings go to a cache of the test's own.
    qputenv("XDG_CACHE_HOME", m_dir.path().toUtf8());
}

void TestMath::drawsFormulas()
{
    MathRenderer &math = MathRenderer::instance();
    const MathRenderer::Drawn fraction =
        math.draw(u"\\frac{a}{b} + \\sqrt{x^2}"_s, false, 16, Qt::black);
    QVERIFY2(fraction.ok(), qPrintable(fraction.error));
    QVERIFY(fraction.path.endsWith(u"@2x.png"_s));
    const QImage image(fraction.path);
    QVERIFY(std::abs(image.width() - fraction.size.width() * 2) <= 1);
    QVERIFY(fraction.size.width() > 30);
    QVERIFY(fraction.size.height() > 16);
    QVERIFY(fraction.baseline > 0 && fraction.baseline < fraction.size.height());
    // Something is drawn, not just room.
    int inked = 0;
    for (int y = 0; y < image.height(); ++y)
        for (int x = 0; x < image.width(); ++x)
            inked += qAlpha(image.pixel(x, y)) > 128;
    QVERIFY(inked > 50);

    // Set apart, the same is drawn larger; the same asked again comes from the cache.
    const MathRenderer::Drawn display =
        math.draw(u"\\frac{a}{b} + \\sqrt{x^2}"_s, true, 16, Qt::black);
    QVERIFY(display.size.height() > fraction.size.height());
    QCOMPARE(math.draw(u"\\frac{a}{b} + \\sqrt{x^2}"_s, false, 16, Qt::black).path, fraction.path);
}

void TestMath::saysWhatIsWrong()
{
    // MicroTeX draws what it can of most mistakes, but not a brace out of place.
    const MathRenderer::Drawn wrong = MathRenderer::instance().draw(u"x}"_s, false, 16, Qt::black);
    QVERIFY(!wrong.ok());
    QVERIFY(wrong.error.contains(u"'}'"_s));
}

void TestMath::leavesDefinitionsAsWritten()
{
    // A command that calls itself would never finish, and one defined would last for every
    // formula after, so neither is drawn.
    MathRenderer &math = MathRenderer::instance();
    QVERIFY(!math.draw(u"\\newcommand{\\a}{\\a\\a}\\a"_s, false, 16, Qt::black).ok());
    QVERIFY(!math.draw(u"\\def\\b{x}\\b"_s, true, 16, Qt::black).ok());
    // A command defined by declaring an operator is one too.
    QVERIFY(!math.draw(u"\\DeclareMathOperator{\\c}{\\c}\\c"_s, false, 16, Qt::black).ok());
    // A command that only starts the same way is drawn.
    QVERIFY(math.draw(u"\\delta"_s, false, 16, Qt::black).ok());
}

QTEST_MAIN(TestMath)
#include "tst_math.moc"
