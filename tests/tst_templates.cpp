#include "jot/Templates.h"
#include "jot/Vault.h"

#include <QDir>
#include <QTemporaryDir>
#include <QTest>

using namespace jot;
using namespace Qt::StringLiterals;

class TestTemplates : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void fillsInTheVariables();
    void joinsTheTemplatesFrontMatter();
    void findsItsNotebooksInTheVaultSettings();
};

void TestTemplates::fillsInTheVariables()
{
    const QDateTime now(QDate(2026, 11, 8), QTime(9, 5));
    QCOMPARE(fillTemplate(u"# {{title}}\n{{date}} at {{ time }}\n"_s, u"Plan"_s, now),
             u"# Plan\n2026-11-08 at 09:05\n"_s);
    // Formats as Obsidian writes them; anything else in them is kept as text.
    QCOMPARE(fillTemplate(u"{{date:dddd D MMMM YYYY}} {{time:h:mm a}}"_s, {}, now),
             u"Sunday 8 November 2026 9:05 am"_s);
    QCOMPARE(fillTemplate(u"{{date:[Day] D, [Week's] YY}}"_s, {}, now), u"Day 8, Week's 26"_s);
    // h is the 12-hour clock, with or without AM or PM beside it.
    const QDateTime afternoon(QDate(2026, 11, 8), QTime(15, 4));
    QCOMPARE(fillTemplate(u"{{time:hh:mm}} {{time:h A}} {{time:HH}}"_s, {}, afternoon),
             u"03:04 3 PM 15"_s);
    QCOMPARE(fillTemplate(u"{{time:h}}"_s, {}, QDateTime(QDate(2026, 11, 8), QTime(0, 5))),
             u"12"_s);
    // What it does not know stays as written.
    QCOMPARE(fillTemplate(u"{{weather}} {{title"_s, u"x"_s, now), u"{{weather}} {{title"_s);
}

void TestTemplates::joinsTheTemplatesFrontMatter()
{
    const QString made = u"---\ncreated: 2026-11-08T14:05:00Z\n---\n"_s;
    // The new note's own keys win; the template's others come along, lists and all.
    QCOMPARE(noteFromTemplate(made, u"---\ncreated: never\ntags:\n  - daily\ntask: \"\"\n---\n"
                                    "# Today\n"_s),
             u"---\ncreated: 2026-11-08T14:05:00Z\ntags:\n  - daily\ntask: \"\"\n---\n# Today\n"_s);
    QCOMPARE(noteFromTemplate(made, u"Just text\n"_s), made + u"Just text\n"_s);
}

void TestTemplates::findsItsNotebooksInTheVaultSettings()
{
    QTemporaryDir dir;
    QString error;
    const std::optional<Vault> vault = Vault::create(dir.path(), &error);
    QVERIFY(vault);
    QCOMPARE(templatesFolder(*vault), u"Templates"_s);
    QCOMPARE(dailyNotePath(*vault, QDate(2026, 11, 8)), u"Daily/2026-11-08.md"_s);
    QCOMPARE(dailyTemplatePath(*vault), u"Templates/Daily.md"_s);

    QVERIFY(notefile::write(vault->absolute(u".jot/vault.yaml"_s),
                            u"format: 1\ntemplates: Meta/Patterns/\ndaily: Journal\n"_s, &error));
    QCOMPARE(templatesFolder(*vault), u"Meta/Patterns"_s);
    QCOMPARE(dailyNotePath(*vault, QDate(2026, 11, 8)), u"Journal/2026-11-08.md"_s);

    // Only a notebook in the vault, outside Jot's own folder.
    QVERIFY(notefile::write(vault->absolute(u".jot/vault.yaml"_s),
                            u"templates: ../elsewhere\ndaily: .jot\n"_s, &error));
    QCOMPARE(templatesFolder(*vault), u"Templates"_s);
    QCOMPARE(dailyFolder(*vault), u"Daily"_s);
}

QTEST_GUILESS_MAIN(TestTemplates)
#include "tst_templates.moc"
