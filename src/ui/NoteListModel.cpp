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

void NoteListModel::reload(const VaultIndex *index, const QStringList &favorites)
{
    m_index = index;
    m_favorites = favorites;
    refill();
}

void NoteListModel::reloadNote(const VaultIndex *source, const QString &path)
{
    // A search, a tag or the favorites can take a note in or out as its words change, and an
    // order by when notes were made can move it; only a notebook by last change or title is
    // reshaped by nothing but the note's own row.
    if (source != m_index || !m_query.trimmed().isEmpty() || !m_tag.isEmpty() || m_showFavorites ||
        m_order == u"created") {
        reload(source, m_favorites);
        return;
    }
    const qsizetype at = std::ranges::find(m_rows, path, &Row::path) - m_rows.begin();
    if (at == m_rows.size())
        return;
    const std::optional<NoteInfo> note = m_index ? m_index->note(path) : std::nullopt;
    if (!note) {
        reload(source, m_favorites);
        return;
    }
    Row row = m_rows.at(at);
    row.title = note->title;
    row.modified = note->modified;
    row.type = note->type;
    // Where it goes now, among the others as they are.
    const auto before = [this, &row](const Row &other) {
        if (m_order == u"title")
            return naturalCompare(other.title, row.title) < 0;
        return other.modified != row.modified ? other.modified > row.modified
                                              : other.path < row.path;
    };
    qsizetype to = 0;
    for (qsizetype i = 0; i < m_rows.size(); ++i) {
        if (i != at && before(m_rows.at(i)))
            ++to;
    }
    if (to != at) {
        beginMoveRows({}, int(at), int(at), {}, int(to > at ? to + 1 : to));
        m_rows.move(at, to);
        endMoveRows();
    }
    m_rows[to] = row;
    Q_EMIT dataChanged(index(int(to), 0), index(int(to), 0));
}

void NoteListModel::setNotebook(const QString &notebook)
{
    if (m_notebook == notebook && m_tag.isEmpty() && !m_showFavorites)
        return;
    m_notebook = notebook;
    m_tag.clear();
    m_showFavorites = false;
    Q_EMIT scopeChanged();
    refill();
}

void NoteListModel::setTag(const QString &tag)
{
    if (m_tag == tag)
        return;
    m_tag = tag;
    if (!tag.isEmpty()) {
        m_notebook.clear();
        m_showFavorites = false;
    }
    Q_EMIT scopeChanged();
    refill();
}

void NoteListModel::setFavorites(bool shown)
{
    if (m_showFavorites == shown)
        return;
    m_showFavorites = shown;
    if (shown) {
        m_notebook.clear();
        m_tag.clear();
    }
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
            for (const SearchHit &hit : m_index->search(scopedQuery(m_query, m_notebook, m_tag))) {
                if (!m_showFavorites || m_favorites.contains(hit.path))
                    m_rows.append({hit.path, hit.title, hit.modified, hit.snippet,
                                   m_index->note(hit.path).value_or(NoteInfo()).type});
            }
        } else if (m_showFavorites) {
            // In the order given, of those still there.
            for (const QString &path : std::as_const(m_favorites)) {
                if (const std::optional<NoteInfo> note = m_index->note(path))
                    m_rows.append({note->path, note->title, note->modified, {}, note->type});
            }
        } else {
            QList<NoteInfo> notes;
            if (!m_tag.isEmpty()) {
                const QStringList tagged = m_index->tagged(m_tag);
                const QSet<QString> wanted(tagged.cbegin(), tagged.cend());
                for (const NoteInfo &note : m_index->shownIn({})) {
                    if (wanted.contains(note.path))
                        notes.append(note);
                }
            } else {
                notes = m_index->listedIn(m_notebook);
            }
            sortNotes(notes, m_order);
            for (const NoteInfo &note : std::as_const(notes))
                m_rows.append({note.path, note.title, note.modified, {}, note.type});
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
    case TypeRole: return row.type;
    }
    return {};
}

QHash<int, QByteArray> NoteListModel::roleNames() const
{
    return {{PathRole, "path"},         {TitleRole, "title"},     {NotebookRole, "notebook"},
            {ModifiedRole, "modified"}, {SnippetRole, "snippet"}, {TypeRole, "type"}};
}

} // namespace jot
