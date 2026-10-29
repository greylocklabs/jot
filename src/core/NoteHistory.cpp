#include "jot/NoteHistory.h"

#include "jot/Vault.h"

#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>

#include <algorithm>

using namespace Qt::StringLiterals;

namespace jot {

namespace {

const QString kStamp = u"yyyy-MM-dd HHmmss"_s;

/// When a version's file name says the note held it, and which of those kept within the same
/// second it is; an invalid time for a file that is not a version.
std::pair<QDateTime, int> writtenFrom(const QString &fileName)
{
    // A second version within the same second is "<stamp> 2.md".
    static const QRegularExpression name(u"^(\\d{4}-\\d\\d-\\d\\d \\d{6})(?: (\\d+))?\\.md$"_s);
    const QRegularExpressionMatch match = name.match(fileName);
    if (!match.hasMatch())
        return {};
    return {QDateTime::fromString(match.captured(1), kStamp),
            match.hasCaptured(2) ? match.captured(2).toInt() : 1};
}

} // namespace

QList<NoteHistory::Version> NoteHistory::versions(const QString &note) const
{
    const QString folder = Vault::historyFolderFor(note);
    QList<std::pair<Version, int>> found;
    const QFileInfoList files =
        QDir(m_vault.absolute(folder)).entryInfoList({u"*.md"_s}, QDir::Files);
    for (const QFileInfo &file : files) {
        const auto [written, order] = writtenFrom(file.fileName());
        if (written.isValid())
            found.append({{written, folder + u'/' + file.fileName()}, order});
    }
    std::ranges::sort(found, [](const auto &a, const auto &b) {
        return a.first.written != b.first.written ? a.first.written > b.first.written
                                                  : a.second > b.second;
    });
    QList<Version> newestFirst;
    for (const auto &[version, order] : found)
        newestFirst.append(version);
    return newestFirst;
}

bool NoteHistory::keep(const QString &note, const QString &text, const QDateTime &written,
                       QString *error) const
{
    if (text.isEmpty())
        return true;
    const QList<Version> kept = versions(note);
    if (!kept.isEmpty()) {
        QString ignored;
        if (notefile::read(m_vault.absolute(kept.first().path), &ignored) == text)
            return true;
    }
    const QString folder = Vault::historyFolderFor(note);
    if (!QDir().mkpath(m_vault.absolute(folder))) {
        *error = QObject::tr("could not create %1").arg(folder);
        return false;
    }
    const QString stamp = written.toLocalTime().toString(kStamp);
    QString path = folder + u'/' + stamp + u".md"_s;
    for (int n = 2; QFileInfo::exists(m_vault.absolute(path)); ++n)
        path = folder + u'/' + u"%1 %2.md"_s.arg(stamp).arg(n);
    return notefile::write(m_vault.absolute(path), text, error);
}

} // namespace jot
