#include "NoteListModel.h"

#include "jot/Settings.h"
#include "jot/Vault.h"

#include <QSet>

#include <algorithm>

using namespace Qt::StringLiterals;

namespace jot {

namespace {

QString notebookOf(const QString &path)
{
    const qsizetype slash = path.lastIndexOf(u'/');
    return slash < 0 ? QString() : path.left(slash);
}

/// The search words, with the notebook or tag shown as a filter. The filter goes first,
/// so a quote the user leaves open cannot swallow it.
QString scopedQuery(const QString &query, const QString &notebook, const QString &tag)
{
    QString scope;
    if (!tag.isEmpty())
        scope = u"tag:\"%1\" "_s.arg(tag);
    else if (!notebook.isEmpty())
        scope = u"in:\"%1\" "_s.arg(notebook);
    return scope + query;
}

} // namespace

NoteListModel::NoteListModel(QObject *parent) : QAbstractListModel(parent) {}

void NoteListModel::reload(const VaultIndex *index, const QStringList &pinned)
{
    m_index = index;
    m_pinned = pinned;
    refill();
}

void NoteListModel::setNotebook(const QString &notebook)
{
    if (m_notebook == notebook && m_tag.isEmpty())
        return;
    m_notebook = notebook;
    m_tag.clear();
    Q_EMIT scopeChanged();
    refill();
}

void NoteListModel::setTag(const QString &tag)
{
    if (m_tag == tag)
        return;
    m_tag = tag;
    if (!tag.isEmpty())
        m_notebook.clear();
    Q_EMIT scopeChanged();
    refill();
}

void NoteListModel::setQuery(const QString &query)
{
    if (m_query == query)
        return;
    m_query = query;
    Q_EMIT queryChanged();
    refill();
}

void NoteListModel::setOrder(const QString &order)
{
    if (m_order == order || !Settings::noteSorts().contains(order))
        return;
    m_order = order;
    Q_EMIT orderChanged();
    refill();
}

void NoteListModel::refill()
{
    const int before = int(m_rows.size());
    beginResetModel();
    m_rows.clear();
    if (m_index) {
        if (!m_query.trimmed().isEmpty()) {
            for (const SearchHit &hit : m_index->search(scopedQuery(m_query, m_notebook, m_tag)))
                m_rows.append(
                    {hit.path, hit.title, hit.modified, hit.snippet, m_pinned.contains(hit.path)});
        } else {
            QList<NoteInfo> notes;
            if (!m_tag.isEmpty()) {
                const QStringList tagged = m_index->tagged(m_tag);
                const QSet<QString> wanted(tagged.cbegin(), tagged.cend());
                for (const NoteInfo &note : m_index->notesIn({})) {
                    if (wanted.contains(note.path))
                        notes.append(note);
                }
            } else {
                notes = m_index->notesIn(m_notebook);
            }
            sortNotes(notes, m_order, m_pinned);
            for (const NoteInfo &note : std::as_const(notes))
                m_rows.append(
                    {note.path, note.title, note.modified, {}, m_pinned.contains(note.path)});
        }
    }
    endResetModel();
    if (before != m_rows.size())
        Q_EMIT countChanged();
}

int NoteListModel::rowOf(const QString &path) const
{
    for (qsizetype i = 0; i < m_rows.size(); ++i) {
        if (m_rows.at(i).path == path)
            return int(i);
    }
    return -1;
}

QString NoteListModel::pathAt(int row) const
{
    return row >= 0 && row < m_rows.size() ? m_rows.at(row).path : QString();
}

int NoteListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_rows.size());
}

QVariant NoteListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_rows.size())
        return {};
    const Row &row = m_rows.at(index.row());
    switch (role) {
    case PathRole: return row.path;
    case TitleRole: return row.title;
    case NotebookRole: return notebookOf(row.path);
    case ModifiedRole: return QDateTime::fromMSecsSinceEpoch(row.modified / 1000000);
    case SnippetRole: return row.snippet;
    case PinnedRole: return row.pinned;
    }
    return {};
}

QHash<int, QByteArray> NoteListModel::roleNames() const
{
    return {{PathRole, "path"},         {TitleRole, "title"},     {NotebookRole, "notebook"},
            {ModifiedRole, "modified"}, {SnippetRole, "snippet"}, {PinnedRole, "pinned"}};
}

} // namespace jot
