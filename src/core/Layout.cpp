#include "jot/Layout.h"

#include "jot/FrontMatter.h"

#include <QDir>
#include <QFileInfo>

#include <optional>

using namespace Qt::StringLiterals;

namespace jot {

namespace {

/// A line's key and what follows its colon, the key unquoted; nothing for a line that is
/// not `key:` or `key: value`.
std::optional<std::pair<QString, QString>> splitKey(QStringView line)
{
    qsizetype end = -1;
    if (line.startsWith(u'"') || line.startsWith(u'\'')) {
        for (qsizetype i = 1; i < line.size(); ++i) {
            if (line.front() == u'"' && line[i] == u'\\') {
                ++i;
            } else if (line[i] == line.front()) {
                end = i + 1;
                break;
            }
        }
        if (end < 0 || end >= line.size() || line[end] != u':')
            return std::nullopt;
    } else {
        end = line.indexOf(u": "_s);
        if (end < 0 && line.endsWith(u':'))
            end = line.size() - 1;
        if (end <= 0)
            return std::nullopt;
    }
    return std::pair{readYamlScalar(line.first(end)), line.sliced(end + 1).trimmed().toString()};
}

bool under(const QString &path, const QString &folder)
{
    return path == folder || path.startsWith(folder + u'/');
}

} // namespace

Layout Layout::read(const Vault &vault)
{
    Layout layout;
    QString ignored;
    const std::optional<QString> text = notefile::read(vault.absolute(kPath), &ignored);
    if (!text)
        return layout;
    QString section;
    QString entry;
    qsizetype entryIndent = -1;
    for (const QString &raw : text->split(u'\n')) {
        const QString line = raw.trimmed();
        if (line.isEmpty() || line.startsWith(u'#'))
            continue;
        // The line starts where its text does, after the spaces that indent it.
        const qsizetype at = raw.indexOf(line);
        if (at == 0) {
            section = splitKey(line).value_or(std::pair<QString, QString>()).first;
            entry.clear();
            entryIndent = -1;
            continue;
        }
        if (line == u"-"_s || line.startsWith(u"- "_s)) {
            if (section == u"order"_s && !entry.isEmpty())
                layout.m_order[entry].append(readYamlScalar(QStringView(line).sliced(1)));
            continue;
        }
        const auto key = splitKey(line);
        if (!key)
            continue;
        if (entryIndent < 0 || at <= entryIndent) {
            entry = key->first;
            entryIndent = at;
            if (section == u"order"_s)
                layout.m_order[entry];
        } else if (section == u"folders"_s && !key->second.isEmpty()) {
            layout.m_folders[entry][key->first] = readYamlScalar(key->second);
        }
    }
    return layout;
}

QString Layout::text() const
{
    QString out;
    if (!m_order.isEmpty()) {
        out += u"order:\n"_s;
        for (auto it = m_order.cbegin(); it != m_order.cend(); ++it) {
            out += u"  %1:\n"_s.arg(yamlScalar(it.key()));
            for (const QString &name : it.value())
                out += u"    - %1\n"_s.arg(yamlScalar(name));
        }
    }
    if (!m_folders.isEmpty()) {
        out += u"folders:\n"_s;
        for (auto it = m_folders.cbegin(); it != m_folders.cend(); ++it) {
            out += u"  %1:\n"_s.arg(yamlScalar(it.key()));
            for (auto key = it->cbegin(); key != it->cend(); ++key)
                out += u"    %1: %2\n"_s.arg(yamlScalar(key.key()), yamlScalar(key.value()));
        }
    }
    return out;
}

bool Layout::write(const Vault &vault, QString *error) const
{
    const auto isFolder = [&](const QString &path) {
        return QFileInfo(vault.absolute(path)).isDir();
    };
    Layout kept;
    for (auto it = m_order.cbegin(); it != m_order.cend(); ++it) {
        if (!isFolder(it.key()))
            continue;
        QStringList names;
        for (const QString &name : it.value()) {
            if (QFileInfo::exists(vault.absolute(it.key() + u'/' + name)))
                names.append(name);
        }
        if (!names.isEmpty())
            kept.m_order.insert(it.key(), names);
    }
    for (auto it = m_folders.cbegin(); it != m_folders.cend(); ++it) {
        if (isFolder(it.key()) && !it->isEmpty())
            kept.m_folders.insert(it.key(), it.value());
    }

    const QString path = vault.absolute(kPath);
    QString ignored;
    const std::optional<QString> before = notefile::read(path, &ignored);
    const QString text = kept.text();
    if (before ? *before == text : text.isEmpty())
        return true;
    if (!QFileInfo(path).dir().exists() && !QDir().mkpath(QFileInfo(path).path())) {
        *error = QObject::tr("could not make %1").arg(QFileInfo(path).path());
        return false;
    }
    return notefile::write(path, text, error);
}

bool Layout::follow(const Vault &vault, const QString &from, const QString &to, QString *error)
{
    Layout layout = read(vault);
    const QString before = layout.text();
    layout.move(from, to);
    if (layout.text() == before)
        return true;
    return layout.write(vault, error);
}

void Layout::move(const QString &from, const QString &to)
{
    const auto moved = [&](const QString &path) { return to + path.sliced(from.size()); };
    for (const QString &key : m_order.keys()) {
        if (under(key, from))
            m_order.insert(moved(key), m_order.take(key));
    }
    for (const QString &key : m_folders.keys()) {
        if (under(key, from))
            m_folders.insert(moved(key), m_folders.take(key));
    }
}

QString Layout::emoji(const QString &folder) const
{
    return m_folders.value(folder).value(u"emoji"_s);
}

void Layout::setEmoji(const QString &folder, const QString &emoji)
{
    if (emoji.trimmed().isEmpty()) {
        if (m_folders.contains(folder)) {
            m_folders[folder].remove(u"emoji"_s);
            if (m_folders[folder].isEmpty())
                m_folders.remove(folder);
        }
        return;
    }
    m_folders[folder][u"emoji"_s] = emoji.trimmed();
}

QHash<QString, QString> Layout::emojis() const
{
    QHash<QString, QString> found;
    for (auto it = m_folders.cbegin(); it != m_folders.cend(); ++it) {
        if (const QString emoji = it->value(u"emoji"_s); !emoji.isEmpty())
            found.insert(it.key(), emoji);
    }
    return found;
}

} // namespace jot
