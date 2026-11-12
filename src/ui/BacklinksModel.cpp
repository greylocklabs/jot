#include "BacklinksModel.h"

#include "jot/Vault.h"

#include <QRegularExpression>
#include <QSet>

using namespace Qt::StringLiterals;

namespace jot {

namespace {

/// A line as it reads: links by their text, without brackets or targets.
QString readable(QString line)
{
    static const QRegularExpression link(uR"(!?\[([^\]]*)\]\([^)]*\))"_s);
    static const QRegularExpression list(uR"(^\s*(?:[-*+]|\d+[.)])\s+(?:\[[ xX]\]\s+)?)"_s);
    line.replace(list, QString());
    line.replace(link, u"\\1"_s);
    return line.trimmed();
}

} // namespace

BacklinksModel::BacklinksModel(QObject *parent) : QAbstractListModel(parent) {}

void BacklinksModel::reload(const Vault *vault, const VaultIndex *index, const QString &path)
{
    beginResetModel();
    m_rows.clear();
    m_linked = 0;
    if (vault && index && !path.isEmpty()) {
        QHash<QString, QStringList> lines;
        QSet<QString> sources;
        for (const NoteLink &link : index->backlinks(path)) {
            if (!lines.contains(link.source)) {
                QString error;
                lines.insert(link.source, notefile::read(vault->absolute(link.source), &error)
                                              .value_or(QString())
                                              .split(u'\n'));
            }
            const QString line = readable(lines.value(link.source).value(link.line - 1));
            // One row a line, though a line can link twice.
            if (!m_rows.isEmpty() && m_rows.last().path == link.source &&
                m_rows.last().line == link.line)
                continue;
            const std::optional<NoteInfo> source = index->note(link.source);
            m_rows.append({link.source, source ? source->title : Vault::titleFromPath(link.source),
                           line, link.line, false});
            sources.insert(link.source);
        }
        m_linked = int(m_rows.size());

        // Mentions: the title or an alias, as a phrase, in notes that do not link here.
        if (const std::optional<NoteInfo> info = index->note(path)) {
            QSet<QString> seen = sources;
            seen.insert(path);
            for (const QString &name : QStringList{info->title} + info->aliases) {
                if (name.trimmed().size() < 3)
                    continue;
                for (const SearchHit &hit : index->search(u'"' + name + u'"', kMentionLimit)) {
                    if (seen.contains(hit.path) || m_rows.size() - m_linked >= kMentionLimit)
                        continue;
                    seen.insert(hit.path);
                    QString text = hit.snippet;
                    text.remove(u"<mark>"_s).remove(u"</mark>"_s);
                    m_rows.append({hit.path, hit.title, text.simplified(), 0, true});
                }
            }
        }
    }
    endResetModel();
    Q_EMIT countsChanged();
}

int BacklinksModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_rows.size());
}

QVariant BacklinksModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_rows.size())
        return {};
    const Row &row = m_rows.at(index.row());
    switch (role) {
    case PathRole: return row.path;
    case TitleRole: return row.title;
    case TextRole: return row.text;
    case LineRole: return row.line;
    case MentionRole: return row.mention;
    }
    return {};
}

QHash<int, QByteArray> BacklinksModel::roleNames() const
{
    return {{PathRole, "path"},
            {TitleRole, "title"},
            {TextRole, "excerpt"},
            {LineRole, "line"},
            {MentionRole, "mention"}};
}

} // namespace jot
