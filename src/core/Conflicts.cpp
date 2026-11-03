#include "jot/Conflicts.h"

#include <QFileInfo>
#include <QRegularExpression>

#include <algorithm>

using namespace Qt::StringLiterals;

namespace jot {

std::optional<ConflictCopy> readConflictCopy(const QString &path)
{
    const QFileInfo info(path);
    const QString folder = path.contains(u'/') ? path.section(u'/', 0, -2) + u'/' : QString();
    const QString suffix = info.suffix().isEmpty() ? QString() : u'.' + info.suffix();
    const QString stem = info.suffix().isEmpty() ? info.fileName() : info.completeBaseName();
    // Each name: the original's stem, who it came from, when, and how to read when.
    struct Form
    {
        QRegularExpression pattern;
        QString time;
    };
    static const QList<Form> forms = {
        {QRegularExpression(
             uR"(^(.+) \(conflict (?:(\S+) )?(\d{4}-\d{2}-\d{2} \d{4})\)(?: \d+)?$)"_s),
         u"yyyy-MM-dd HHmm"_s},
        {QRegularExpression(uR"(^(.+)\.sync-conflict-(\d{8}-\d{6})-([A-Z0-9]{7})()$)"_s),
         u"yyyyMMdd-HHmmss"_s},
        {QRegularExpression(
             uR"(^(.+) \(conflicted copy (?:(.+) )?(\d{4}-\d{2}-\d{2} \d{6})\)(?: \(\d+\))?$)"_s),
         u"yyyy-MM-dd HHmmss"_s},
        {QRegularExpression(uR"(^(.+)_conflict-(\d{8}-\d{6})()$)"_s), u"yyyyMMdd-HHmmss"_s},
        {QRegularExpression(
             uR"(^(.+) \((.+)'s conflicted copy (\d{4}-\d{2}-\d{2})\)(?: \(\d+\))?$)"_s),
         u"yyyy-MM-dd"_s},
    };
    for (const Form &form : forms) {
        const QRegularExpressionMatch m = form.pattern.match(stem);
        if (!m.hasMatch())
            continue;
        // Syncthing and ownCloud put the time before who; the rest after.
        const bool timeFirst = form.time.startsWith(u"yyyyMMdd"_s);
        const QString when = timeFirst ? m.captured(2) : m.captured(3);
        const QString from = timeFirst ? m.captured(3) : m.captured(2);
        return ConflictCopy{folder + m.captured(1) + suffix, from,
                            QDateTime::fromString(when, form.time)};
    }
    return std::nullopt;
}

QString conflictCopyPath(const QString &root, const QString &path, const QString &device,
                         const QDateTime &when)
{
    const QFileInfo info(path);
    const QString folder = path.contains(u'/') ? path.section(u'/', 0, -2) + u'/' : QString();
    const QString suffix = info.suffix().isEmpty() ? QString() : u'.' + info.suffix();
    const QString stem = info.suffix().isEmpty() ? info.fileName() : info.completeBaseName();
    const QString label =
        u"%1 (conflict %2 %3)"_s.arg(stem, device, when.toString(u"yyyy-MM-dd HHmm"_s));
    QString candidate = folder + label + suffix;
    for (int n = 2; QFileInfo::exists(root + u'/' + candidate); ++n)
        candidate = folder + label + u' ' + QString::number(n) + suffix;
    return candidate;
}

QStringList conflictCopiesOf(const QString &path, const QStringList &paths)
{
    QList<std::pair<QDateTime, QString>> found;
    for (const QString &each : paths) {
        const std::optional<ConflictCopy> copy = readConflictCopy(each);
        if (copy && copy->original == path)
            found.append({copy->when, each});
    }
    std::ranges::sort(found);
    QStringList copies;
    for (const auto &[when, each] : std::as_const(found))
        copies.append(each);
    return copies;
}

} // namespace jot
