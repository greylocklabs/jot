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
    static const QRegularExpression jot(
        uR"(^(.+) \(conflict (?:(\S+) )?(\d{4}-\d{2}-\d{2} \d{4})\)(?: \d+)?$)"_s);
    const QRegularExpressionMatch m = jot.match(stem);
    if (!m.hasMatch())
        return std::nullopt;
    return ConflictCopy{folder + m.captured(1) + suffix, m.captured(2),
                        QDateTime::fromString(m.captured(3), u"yyyy-MM-dd HHmm"_s)};
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
