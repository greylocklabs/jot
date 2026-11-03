#include "jot/GitSync.h"

#include "jot/Conflicts.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QSaveFile>
#include <QStringDecoder>

using namespace Qt::StringLiterals;

namespace jot {

namespace {

/// What stays on this computer: hidden files but for Jot's own .jot/, and what syncs delete.
const QByteArray kExclude = "# Written by Jot: what stays on this computer.\n"
                            ".*\n"
                            "!/.jot/\n"
                            "!/.gitattributes\n"
                            "/.jot/trash/\n";

const QByteArray kLfsStart = "# Jot: large files go to Git LFS.\n";

std::optional<QString> utf8(const QByteArray &data)
{
    QStringDecoder decode(QStringDecoder::Utf8, QStringDecoder::Flag::Stateless |
                                                    QStringDecoder::Flag::ConvertInitialBom);
    QString text = decode(data);
    if (decode.hasError())
        return std::nullopt;
    return text;
}

} // namespace

const QStringList GitSync::kLargeFiles = {
    u"png"_s, u"jpg"_s, u"jpeg"_s, u"gif"_s,  u"webp"_s, u"heic"_s, u"pdf"_s,  u"mp3"_s,
    u"m4a"_s, u"wav"_s, u"ogg"_s,  u"flac"_s, u"mp4"_s,  u"mov"_s,  u"webm"_s, u"zip"_s};

GitSync::GitSync(QString root, QString url, QString branch, QString gitDir, QString device)
    : m_root(QDir(root).absolutePath()), m_url(std::move(url)),
      m_branch(branch.isEmpty() ? u"main"_s : std::move(branch)), m_gitDir(std::move(gitDir)),
      m_device(std::move(device))
{
    // A vault that is a git repository already keeps using its own.
    if (QFileInfo(m_root + u"/.git"_s).isDir())
        m_gitDir = m_root + u"/.git"_s;
}

QString GitSync::remoteBranch() const
{
    return u"refs/remotes/origin/"_s + m_branch;
}

GitSync::Run GitSync::command(const QStringList &arguments, const QString &directory, int timeoutMs)
{
    QProcess process;
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    // Never wait on a question nobody sees: a password prompt fails instead, and SSH uses the
    // agent and keys it has.
    environment.insert(u"GIT_TERMINAL_PROMPT"_s, u"0"_s);
    if (!environment.contains(u"GIT_SSH_COMMAND"_s))
        environment.insert(u"GIT_SSH_COMMAND"_s, u"ssh -o BatchMode=yes"_s);
    environment.insert(u"LC_ALL"_s, u"C"_s);
    process.setProcessEnvironment(environment);
    if (!directory.isEmpty())
        process.setWorkingDirectory(directory);
    process.start(u"git"_s, arguments);
    Run run;
    if (!process.waitForStarted(10000)) {
        run.err = "git could not be started";
        return run;
    }
    process.closeWriteChannel();
    if (!process.waitForFinished(timeoutMs)) {
        process.kill();
        process.waitForFinished(1000);
        run.err = "git took too long";
        return run;
    }
    run.code = process.exitStatus() == QProcess::NormalExit ? process.exitCode() : -1;
    run.out = process.readAllStandardOutput();
    run.err = process.readAllStandardError();
    return run;
}

GitSync::Run GitSync::git(const QStringList &arguments, int timeoutMs) const
{
    return command(QStringList{u"--git-dir="_s + m_gitDir, u"--work-tree="_s + m_root, u"-c"_s,
                               u"core.quotepath=off"_s, u"-c"_s, u"core.autocrlf=false"_s} +
                       arguments,
                   m_root, timeoutMs);
}

bool GitSync::available()
{
    return command({u"--version"_s}, {}, 10000).ok();
}

bool GitSync::lfsAvailable()
{
    return command({u"lfs"_s, u"version"_s}, {}, 10000).ok();
}

bool GitSync::reachable(const QString &url, QString *error)
{
    if (!available()) {
        *error = QObject::tr("git is not installed");
        return false;
    }
    const Run run = command({u"ls-remote"_s, u"--heads"_s, url}, {}, kNetworkMs);
    if (!run.ok())
        *error = QString::fromUtf8(run.err).trimmed();
    return run.ok();
}

bool GitSync::writeFile(const QString &path, const QByteArray &data, QString *error) const
{
    const QString target = m_root + u'/' + path;
    QDir().mkpath(QFileInfo(target).path());
    QSaveFile file(target);
    if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit()) {
        *error = QObject::tr("could not write %1: %2").arg(path, file.errorString());
        return false;
    }
    return true;
}

bool GitSync::prepare(QString *error)
{
    const auto fail = [error](const Run &run) {
        *error = QString::fromUtf8(run.err).trimmed();
        return false;
    };
    if (!QFileInfo(m_gitDir).isDir()) {
        QDir().mkpath(m_gitDir);
        if (const Run run = git({u"init"_s, u"-q"_s, u"-b"_s, m_branch}); !run.ok())
            return fail(run);
    }
    const Run remote = git({u"remote"_s, u"get-url"_s, u"origin"_s});
    if (!remote.ok()) {
        if (const Run run = git({u"remote"_s, u"add"_s, u"origin"_s, m_url}); !run.ok())
            return fail(run);
    } else if (QString::fromUtf8(remote.out).trimmed() != m_url) {
        if (const Run run = git({u"remote"_s, u"set-url"_s, u"origin"_s, m_url}); !run.ok())
            return fail(run);
    }
    // Commits need a name: the person's own where git has one, else this computer's.
    if (git({u"config"_s, u"user.email"_s}).out.trimmed().isEmpty()) {
        git({u"config"_s, u"user.name"_s, u"Jot on "_s + m_device});
        git({u"config"_s, u"user.email"_s, u"jot@"_s + m_device + u".local"_s});
    }
    QDir().mkpath(m_gitDir + u"/info"_s);
    QFile exclude(m_gitDir + u"/info/exclude"_s);
    if (exclude.open(QIODevice::ReadOnly) && exclude.readAll().contains(kExclude)) {
        exclude.close();
    } else {
        exclude.close();
        if (exclude.open(QIODevice::Append))
            exclude.write('\n' + kExclude);
    }
    if (lfsAvailable()) {
        if (const Run run = git({u"lfs"_s, u"install"_s, u"--local"_s}); !run.ok())
            return fail(run);
        // Kept in the vault, so a plain clone of the repository reads the large files too.
        QFile attributes(m_root + u"/.gitattributes"_s);
        QByteArray held;
        if (attributes.open(QIODevice::ReadOnly))
            held = attributes.readAll();
        attributes.close();
        if (!held.contains(kLfsStart)) {
            QByteArray lines = kLfsStart;
            for (const QString &extension : kLargeFiles)
                lines += "*." + extension.toLatin1() + " filter=lfs diff=lfs merge=lfs -text\n";
            if (!held.isEmpty() && !held.endsWith('\n'))
                held += '\n';
            if (!writeFile(u".gitattributes"_s, held + lines, error))
                return false;
        }
    }
    return true;
}

std::optional<QStringList> GitSync::commit(QString *error)
{
    if (const Run run = git({u"add"_s, u"-A"_s}); !run.ok()) {
        *error = QString::fromUtf8(run.err).trimmed();
        return std::nullopt;
    }
    const Run staged = git({u"diff"_s, u"--cached"_s, u"--name-only"_s, u"-z"_s});
    QStringList paths;
    for (const QByteArray &path : staged.out.split('\0')) {
        if (!path.isEmpty())
            paths << QString::fromUtf8(path);
    }
    if (paths.isEmpty())
        return paths;
    const QString message = u"Jot on %1, %2"_s.arg(m_device, m_now.toString(u"yyyy-MM-dd HH:mm"_s));
    if (const Run run = git({u"commit"_s, u"-q"_s, u"--no-verify"_s, u"-m"_s, message});
        !run.ok()) {
        *error = QString::fromUtf8(run.err + run.out).trimmed();
        return std::nullopt;
    }
    return paths;
}

bool GitSync::resolve(const QString &path, SyncReport &report, QString *error)
{
    // What each side holds: 1 the base, 2 this computer's, 3 the other's.
    QHash<int, QByteArray> stages;
    const Run listed = git({u"ls-files"_s, u"-u"_s, u"-z"_s, u"--"_s, path});
    for (const QByteArray &entry : listed.out.split('\0')) {
        const QList<QByteArray> fields = entry.left(entry.indexOf('\t')).split(' ');
        if (fields.size() < 3)
            continue;
        const int stage = fields.at(2).toInt();
        const Run shown = git({u"show"_s, u":%1:%2"_s.arg(stage).arg(path)});
        if (shown.ok())
            stages.insert(stage, shown.out);
    }
    QStringList add = {path};
    if (stages.contains(2) && stages.contains(3)) {
        const std::optional<QString> base =
            stages.contains(1) ? utf8(stages.value(1)) : std::optional<QString>(QString());
        const std::optional<QString> mine = utf8(stages.value(2));
        const std::optional<QString> theirs = utf8(stages.value(3));
        std::optional<QString> merged;
        if (SyncEngine::mergeable(path) && base && mine && theirs)
            merged = SyncEngine::mergeNote(*base, *mine, *theirs);
        if (merged) {
            if (!writeFile(path, merged->toUtf8(), error))
                return false;
            report.merged << path;
        } else {
            // This computer's stays at the path; the other's is kept beside it.
            const QString copy = conflictCopyPath(m_root, path, m_device, m_now);
            if (!writeFile(path, stages.value(2), error) ||
                !writeFile(copy, stages.value(3), error))
                return false;
            add << copy;
            report.conflicts << copy;
        }
    } else if (stages.contains(2) || stages.contains(3)) {
        // Deleted on one side and changed on the other: the changed one stays.
        if (!writeFile(path, stages.contains(2) ? stages.value(2) : stages.value(3), error))
            return false;
        report.merged << path;
    } else {
        // Deleted on both sides.
        git({u"rm"_s, u"-q"_s, u"--cached"_s, u"--"_s, path});
        return true;
    }
    if (const Run run = git(QStringList{u"add"_s, u"--"_s} + add); !run.ok()) {
        *error = QString::fromUtf8(run.err).trimmed();
        return false;
    }
    return true;
}

bool GitSync::merge(SyncReport &report, QString *error)
{
    if (!git({u"rev-parse"_s, u"-q"_s, u"--verify"_s, remoteBranch()}).ok())
        return true;
    const Run head = git({u"rev-parse"_s, u"-q"_s, u"--verify"_s, u"HEAD"_s});
    if (head.ok() && git({u"merge-base"_s, u"--is-ancestor"_s, remoteBranch(), u"HEAD"_s}).ok())
        return true;
    const QString before = QString::fromUtf8(head.out).trimmed();
    const Run run = git({u"merge"_s, u"-q"_s, u"--no-edit"_s, u"--no-verify"_s,
                         u"--allow-unrelated-histories"_s, remoteBranch()});
    if (!run.ok()) {
        const Run unmerged = git({u"diff"_s, u"--name-only"_s, u"--diff-filter=U"_s, u"-z"_s});
        QStringList conflicted;
        for (const QByteArray &path : unmerged.out.split('\0')) {
            if (!path.isEmpty())
                conflicted << QString::fromUtf8(path);
        }
        // A merge git could not start, or one this cannot finish, is undone as a whole.
        if (conflicted.isEmpty()) {
            *error = QString::fromUtf8(run.err + run.out).trimmed();
            git({u"merge"_s, u"--abort"_s});
            return false;
        }
        for (const QString &path : std::as_const(conflicted)) {
            if (!resolve(path, report, error)) {
                git({u"merge"_s, u"--abort"_s});
                return false;
            }
        }
        if (const Run done = git({u"commit"_s, u"-q"_s, u"--no-edit"_s, u"--no-verify"_s});
            !done.ok()) {
            *error = QString::fromUtf8(done.err + done.out).trimmed();
            git({u"merge"_s, u"--abort"_s});
            return false;
        }
    }
    const Run changed = before.isEmpty()
                            ? git({u"ls-files"_s, u"-z"_s})
                            : git({u"diff"_s, u"--name-only"_s, u"-z"_s, before, u"HEAD"_s});
    for (const QByteArray &path : changed.out.split('\0')) {
        const QString each = QString::fromUtf8(path);
        if (!path.isEmpty() && !report.merged.contains(each) && !report.conflicts.contains(each))
            report.pulled << each;
    }
    return true;
}

SyncReport GitSync::sync(const QDateTime &now)
{
    m_now = now;
    SyncReport report;
    QString error;
    if (!available()) {
        report.failure = QObject::tr("git is not installed");
        return report;
    }
    if (!prepare(&error)) {
        report.failure = error;
        return report;
    }
    const std::optional<QStringList> committed = commit(&error);
    if (!committed) {
        report.failure = error;
        return report;
    }
    // Someone else pushing first sends this round back to fetch and merge their work.
    for (int attempt = 0; attempt < 3; ++attempt) {
        const Run fetched = git({u"fetch"_s, u"-q"_s, u"origin"_s}, kNetworkMs);
        if (!fetched.ok()) {
            report.failure = QString::fromUtf8(fetched.err).trimmed();
            return report;
        }
        if (!merge(report, &error)) {
            report.failure = error;
            return report;
        }
        if (!git({u"rev-parse"_s, u"-q"_s, u"--verify"_s, u"HEAD"_s}).ok())
            return report;
        if (git({u"rev-parse"_s, u"-q"_s, u"--verify"_s, remoteBranch()}).out ==
            git({u"rev-parse"_s, u"HEAD"_s}).out)
            break;
        const Run pushed =
            git({u"push"_s, u"-q"_s, u"origin"_s, u"HEAD:refs/heads/"_s + m_branch}, kNetworkMs);
        if (pushed.ok()) {
            report.pushed = *committed;
            return report;
        }
        const QString said = QString::fromUtf8(pushed.err);
        if (!said.contains(u"rejected"_s) && !said.contains(u"fetch first"_s)) {
            report.failure = said.trimmed();
            return report;
        }
    }
    report.pushed = *committed;
    return report;
}

} // namespace jot
