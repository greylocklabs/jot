#include "jot/Translations.h"

#include <QCoreApplication>
#include <QFile>
#include <QTest>
#include <QTranslator>

using namespace Qt::StringLiterals;

class TestTranslations : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void installsTheEnglishWording();
};

void TestTranslations::installsTheEnglishWording()
{
    // The wording compiled from translations/jot_en.ts travels inside the app.
    QVERIFY(QFile::exists(u":/i18n/jot_en.qm"_s));
    jot::installTranslations(*QCoreApplication::instance());
    const QList<QTranslator *> installed =
        QCoreApplication::instance()->findChildren<QTranslator *>();
    QCOMPARE(installed.size(), 1);
    QVERIFY(installed.first()->filePath().endsWith(u".qm"_s));
}

QTEST_GUILESS_MAIN(TestTranslations)
#include "tst_translations.moc"
