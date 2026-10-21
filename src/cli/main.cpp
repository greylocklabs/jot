#include "Completion.h"
#include "Paint.h"
#include "Palette.h"
#include "UpdateHint.h"
#include "Upgrade.h"
#include "VaultCommands.h"

#include "greylock/Identity.h"
#include "jot/Diagnostics.h"
#include "jot/LogFile.h"
#include "jot/Releases.h"
#include "jot/Settings.h"
#include "jot/UpdateCheck.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QStandardPaths>
#include <QTextStream>

#include <optional>

#include <unistd.h>

using namespace jot;
using namespace Qt::StringLiterals;

namespace {

QTextStream out(stdout);
QTextStream err(stderr);

/// Colors from $JOT_THEME, or the default theme, when the output is a terminal.
const cli::Paint &paintFor(int fd)
{
    static const cli::Paint paint = [fd] {
        const cli::Paint::Depth depth = cli::Paint::detect(fd);
        if (depth == cli::Paint::Depth::None)
            return cli::Paint();
        return cli::Paint(depth, cli::Palette::forTheme(qEnvironmentVariable("JOT_THEME")));
    }();
    return paint;
}

/// The commands, for --help and for completion.
const QList<std::pair<QString, QString>> kCommands = {
    {u"status"_s, u"The vault, how many notes it holds, and what is left to do"_s},
    {u"ls"_s, u"List notes, newest first, in a notebook or all: jot ls Recipes"_s},
    {u"search"_s, u"Search notes: jot search ferry tag:ashfall in:Notes"_s},
    {u"new"_s, u"Make a note: jot new \"Idea\"; its body is read from a pipe"_s},
    {u"show"_s, u"Print a note, by its title, an alias or its path"_s},
    {u"open"_s, u"Open a note in the app"_s},
    {u"tasks"_s, u"Task notes and unticked boxes"_s},
    {u"logs"_s, u"Show, follow (-f) or open (--open) the log files"_s},
    {u"doctor"_s, u"Details for a bug report; --report opens a new issue"_s},
    {u"donate"_s, u"Support Jot on Ko-fi: opens the page, or --print prints its link"_s},
    {u"upgrade"_s, u"Install the latest release, or the one given: jot upgrade 0.1.0"_s},
    {u"downgrade"_s, u"Go back to the release before this one, or the one given"_s},
    {u"completions"_s, u"Print the shell completion script: bash, zsh or fish"_s},
    {u"gui"_s, u"Open the desktop app"_s},
};

/// After a command on a terminal, says once that a newer release is out. GitHub
/// is asked at most once a day, by a copy of jot left running on its own, so no
/// command waits for it.
class HintAfterwards
{
public:
    explicit HintAfterwards(QString command) : m_command(std::move(command)) {}
    HintAfterwards(const HintAfterwards &) = delete;
    HintAfterwards &operator=(const HintAfterwards &) = delete;

    ~HintAfterwards()
    {
        UpdateCheck check;
        const cli::UpdateHint hint = cli::UpdateHint::after(
            m_command, false, ::isatty(STDERR_FILENO), true, check,
            QVersionNumber::fromString(QCoreApplication::applicationVersion()));
        if (!hint.line.isEmpty())
            err << paintFor(STDERR_FILENO).dim(hint.line) << "\n" << Qt::flush;
        if (hint.ask) {
            check.markAsked();
            QProcess asker;
            asker.setProgram(QCoreApplication::applicationFilePath());
            asker.setArguments({u"update-check"_s});
            asker.setStandardOutputFile(QProcess::nullDevice());
            asker.setStandardErrorFile(QProcess::nullDevice());
            asker.startDetached();
        }
    }

private:
    QString m_command;
};

int runDoctor(bool report)
{
    const QString text = diagnostics::collect();
    out << text << "\n" << Qt::flush;
    if (report) {
        const QString url = diagnostics::issueUrl(text).toString(QUrl::FullyEncoded);
        // Over SSH or without a desktop nothing opens, so the link is always shown.
        if (QProcess::startDetached(u"xdg-open"_s, {url}))
            err << QObject::tr("Opening a new issue in your browser. If it does not open, visit:")
                << "\n";
        else
            err << QObject::tr("Could not open a browser. To report the bug, visit:") << "\n";
        err << url << "\n";
    }
    return 0;
}

/// Prints where to support Jot, and opens it unless `print`.
int runDonate(bool print)
{
    const QString url = releases::supportPage().toString();
    // Over SSH or without a desktop nothing opens, so the link is always shown.
    if (!print)
        err << (QProcess::startDetached(u"xdg-open"_s, {url})
                    ? QObject::tr("Opening Ko-fi in your browser. Thank you!")
                    : QObject::tr("Could not open a browser. To support Jot, visit:"))
            << "\n";
    out << url << "\n";
    return 0;
}

/// The log files, one path per line, newest first; or follows or opens them.
int runLogs(bool follow, bool open)
{
    const QString directory = logfile::defaultDirectory();
    if (open)
        return QProcess::execute(u"xdg-open"_s, {directory}) == 0 ? 0 : 1;

    QDir dir(directory);
    const QStringList names =
        dir.entryList({u"*.log"_s, u"*.log.[0-9]"_s}, QDir::Files, QDir::Time);
    if (names.isEmpty()) {
        err << QObject::tr("No logs yet in %1").arg(directory) << "\n";
        return 0;
    }
    QStringList paths;
    for (const QString &name : names)
        paths.append(dir.filePath(name));
    if (follow) {
        out.flush();
        return QProcess::execute(u"tail"_s, QStringList{u"-F"_s} + paths);
    }
    for (const QString &path : std::as_const(paths))
        out << path << "\n";
    return 0;
}

/// The vault given with --vault, else the one the app opened last, made ready to read.
struct OpenVault
{
    std::unique_ptr<Vault> vault;
    std::unique_ptr<VaultIndex> index;
};

/// For completion: no messages, since they would land in the user's shell, and no full
/// look over the vault on every Tab.
enum class Quiet { No, Yes };

std::optional<OpenVault> openVault(const QString &given, Quiet quiet = Quiet::No)
{
    const QString path = given.isEmpty() ? Settings(Settings::defaultPath()).vaultPath() : given;
    if (path.isEmpty() || !QFileInfo(path).isDir()) {
        if (quiet == Quiet::No)
            err << (path.isEmpty() ? QObject::tr("jot: no vault yet; open one in the app, or give "
                                                 "--vault <folder>")
                                   : QObject::tr("jot: %1 is not a folder").arg(path))
                << "\n";
        return std::nullopt;
    }
    OpenVault open;
    open.vault = std::make_unique<Vault>(QDir(path).absolutePath());
    open.index = std::make_unique<VaultIndex>(*open.vault);
    QString error;
    if (!open.index->open(&error)) {
        if (quiet == Quiet::No)
            err << QObject::tr("jot: could not open the index: %1").arg(error) << "\n";
        return std::nullopt;
    }
    // Completion trusts the index, but a vault never read yet has to be read once.
    if (quiet == Quiet::No || open.index->noteCount() == 0)
        open.index->refresh();
    return open;
}

/// jot-gui beside this jot, as in a build, else on the PATH.
QString guiProgram()
{
    const QString here = QCoreApplication::applicationDirPath();
    for (const QString &candidate : {here + u"/jot-gui"_s, here + u"/../gui/jot-gui"_s}) {
        if (QFileInfo(candidate).isExecutable())
            return QFileInfo(candidate).absoluteFilePath();
    }
    return QStandardPaths::findExecutable(u"jot-gui"_s);
}

int runCompletions(const QString &shell)
{
    const QHash<QString, QString> files = {
        {u"bash"_s, u"jot.bash"_s}, {u"zsh"_s, u"_jot"_s}, {u"fish"_s, u"jot.fish"_s}};
    QFile script(u":/completions/"_s + files.value(shell, u"none"_s));
    if (!script.open(QIODevice::ReadOnly)) {
        err << QObject::tr("usage: jot completions bash|zsh|fish") << "\n";
        return 2;
    }
    out << script.readAll();
    return 0;
}

} // namespace

/// Quiet on success. Each failure is one line on stderr, and any failure exits 1.
int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    app.setApplicationName(greylock::identity::dirName());
    app.setApplicationVersion(QStringLiteral(JOT_VERSION));

    QCommandLineParser parser;
    QString description = u"Jot: plain-file notes for Linux.\n\nCommands:"_s;
    for (const auto &[name, about] : kCommands)
        description += u"\n  "_s + name.leftJustified(12) + about;
    parser.setApplicationDescription(description);
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument(u"command"_s, u"Command to run."_s);
    // Kept for completion too, since the parser does not list them.
    QList<QCommandLineOption> options;
    const auto addOption = [&parser, &options](const QCommandLineOption &option) {
        parser.addOption(option);
        options.append(option);
    };
    const QCommandLineOption followOption({u"f"_s, u"follow"_s},
                                          u"With logs: keep printing new lines."_s);
    addOption(followOption);
    const QCommandLineOption openOption(u"open"_s, u"With logs: open the folder."_s);
    addOption(openOption);
    const QCommandLineOption verboseOption(u"verbose"_s, u"Print progress as well as warnings."_s);
    addOption(verboseOption);
    const QCommandLineOption reportOption(u"report"_s, u"With doctor: open a new GitHub issue."_s);
    addOption(reportOption);
    const QCommandLineOption printOption(u"print"_s,
                                         u"With donate: print the link, without opening it."_s);
    addOption(printOption);
    const QCommandLineOption forceOption(
        u"force"_s, u"With upgrade: install the release even when it is the one installed."_s);
    addOption(forceOption);
    const QCommandLineOption vaultOption(u"vault"_s, u"The vault to use, rather than the app's."_s,
                                         u"folder"_s);
    addOption(vaultOption);
    const QCommandLineOption notebookOption(u"notebook"_s,
                                            u"With new: the notebook to put it in."_s, u"name"_s);
    addOption(notebookOption);
    const QCommandLineOption orderOption(u"order"_s, u"With ls: updated (the default) or title."_s,
                                         u"order"_s);
    addOption(orderOption);
    const QCommandLineOption jsonOption(
        u"json"_s, u"With status, ls, search, new and tasks: print JSON for scripts."_s);
    addOption(jsonOption);
    const QCommandLineOption dryRunOption(
        u"dry-run"_s, u"With upgrade and downgrade: say what would happen, and change nothing."_s);
    addOption(dryRunOption);

    // Run by the shell completion scripts with the words typed so far, which
    // are not parsed as options since they are still being typed.
    if (app.arguments().value(1) == u"__complete") {
        // The shells hand over the line as typed, up to the cursor.
        const QStringList words = app.arguments().value(2) == u"--line"
                                      ? cli::Completion::wordsOf(app.arguments().value(3))
                                      : app.arguments().mid(2);
        QList<cli::Candidate> commands;
        for (const auto &[name, about] : kCommands)
            commands.append({name, about});
        QList<cli::Completion::Option> named;
        for (const QCommandLineOption &option : std::as_const(options)) {
            for (const QString &name : option.names())
                named.append({name, option.description(), !option.valueName().isEmpty()});
        }
        // Notebooks and notes come from the vault, read only when a command wants them.
        std::optional<OpenVault> vault;
        bool tried = false;
        const auto opened = [&]() -> OpenVault * {
            if (!tried) {
                tried = true;
                // --vault folder, or --vault=folder, as the parser would read them.
                QString given;
                for (qsizetype i = 0; i < words.size(); ++i) {
                    if (words.at(i) == u"--vault")
                        given = words.value(i + 1);
                    else if (words.at(i).startsWith(u"--vault="_s))
                        given = words.at(i).mid(8);
                }
                vault = openVault(given, Quiet::Yes);
            }
            return vault ? &*vault : nullptr;
        };
        const auto notebooks = [&opened] {
            QList<cli::Candidate> found;
            if (OpenVault *open = opened()) {
                for (const VaultEntry &entry : open->vault->entries()) {
                    if (entry.folder)
                        found.append({entry.path, {}});
                }
            }
            return found;
        };
        const auto titles = [&opened] {
            QList<cli::Candidate> found;
            if (OpenVault *open = opened()) {
                for (const NoteInfo &note : open->index->notesIn({}))
                    found.append({note.title, note.path});
            }
            return found;
        };
        cli::Completion::Sources sources;
        sources.arguments = [&](const QStringList &positional, const QString &) {
            const QString command = positional.first();
            if (positional.size() == 1 && command == u"completions")
                return cli::Completion::plain({u"bash"_s, u"zsh"_s, u"fish"_s});
            if (positional.size() == 1 && command == u"ls")
                return notebooks();
            if (positional.size() == 1 && (command == u"show" || command == u"open"))
                return titles();
            return QList<cli::Candidate>();
        };
        sources.values = [&](const QString &option) {
            if (option == u"notebook")
                return notebooks();
            if (option == u"order")
                return cli::Completion::plain({u"updated"_s, u"title"_s});
            return QList<cli::Candidate>();
        };
        const cli::Completion completion(commands, named, sources);
        for (const cli::Candidate &candidate : completion.complete(words))
            out << candidate.value
                << (candidate.about.isEmpty() ? QString() : u'\t' + candidate.about) << "\n";
        return 0;
    }

    parser.process(app);
    logfile::install(u"jot"_s);
    logfile::setVerboseTerminal(parser.isSet(verboseOption) ||
                                !qEnvironmentVariableIsEmpty("QT_LOGGING_RULES"));

    const QStringList args = parser.positionalArguments();
    if (args.isEmpty())
        parser.showHelp(0);
    const QString command = args.first();

    // Run by HintAfterwards, on its own, to learn the latest release.
    if (command == u"update-check" && args.size() == 1) {
        UpdateCheck check;
        if (!check.wanted(true))
            return 0;
        QObject::connect(&check, &UpdateCheck::checked, &app, &QCoreApplication::quit);
        check.check();
        return app.exec();
    }

    const HintAfterwards hint(command);

    if (command == u"completions" && args.size() == 2)
        return runCompletions(args.at(1));

    static const QStringList kVaultCommands = {u"status"_s, u"ls"_s,   u"search"_s, u"new"_s,
                                               u"show"_s,   u"open"_s, u"tasks"_s};
    if (kVaultCommands.contains(command)) {
        std::optional<OpenVault> open = openVault(parser.value(vaultOption));
        if (!open)
            return 1;
        cli::VaultCommands commands(*open->vault, *open->index, out, err);
        commands.setPaint(paintFor(STDOUT_FILENO));
        const bool json = parser.isSet(jsonOption);
        const QString rest = args.mid(1).join(u' ');
        if (command == u"status")
            return commands.status(VaultIndex::defaultPath(*open->vault), json);
        if (command == u"ls")
            return commands.list(rest, parser.value(orderOption), json);
        if (command == u"search")
            return commands.search(rest, json);
        if (command == u"show")
            return commands.show(rest);
        if (command == u"tasks")
            return commands.listTasks(json);
        if (command == u"new") {
            // Piped in, the text becomes the note's body.
            QString body;
            if (!::isatty(STDIN_FILENO)) {
                QTextStream in(stdin);
                body = in.readAll();
            }
            return commands.create(rest, parser.value(notebookOption), body, json);
        }
        return commands.open(rest, guiProgram(),
                             [](const QString &program, const QStringList &arguments) {
                                 return QProcess::startDetached(program, arguments);
                             });
    }
    if (command == u"doctor" && args.size() == 1)
        return runDoctor(parser.isSet(reportOption));
    if (command == u"donate" && args.size() == 1)
        return runDonate(parser.isSet(printOption));
    if (command == u"logs" && args.size() == 1)
        return runLogs(parser.isSet(followOption), parser.isSet(openOption));
    if ((command == u"upgrade" || command == u"downgrade") && args.size() <= 2) {
        cli::Upgrade upgrade(out, err);
        cli::Upgrade::Options upgradeOptions;
        upgradeOptions.downgrade = command == u"downgrade";
        if (args.size() == 2)
            upgradeOptions.version = args.at(1);
        upgradeOptions.force = parser.isSet(forceOption);
        upgradeOptions.dryRun = parser.isSet(dryRunOption);
        std::optional<int> result;
        upgrade.run(upgradeOptions, [&result](int code) {
            result = code;
            QCoreApplication::exit(code);
        });
        return result ? *result : app.exec();
    }
    if (command == u"gui" && args.size() == 1)
        return QProcess::startDetached(u"jot-gui"_s, {}) ? 0 : 1;

    err << QObject::tr("jot: unknown command '%1'").arg(args.join(u' ')) << "\n";
    parser.showHelp(1);
}
