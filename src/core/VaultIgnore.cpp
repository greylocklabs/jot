#include "jot/VaultIgnore.h"

#include "jot/FrontMatter.h"
#include "jot/Vault.h"

#include <QFile>

using namespace Qt::StringLiterals;

namespace jot {

namespace {

QString join(const QString &folder, const QString &name)
{
    return folder.isEmpty() ? name : folder + u'/' + name;
}

/// A gitignore glob as a regular expression for the paths it matches, without anchors.
QString globToRegex(QStringView glob)
{
    QString out;
    const qsizetype size = glob.size();
    for (qsizetype i = 0; i < size; ++i) {
        const QChar c = glob[i];
        const bool atStart = i == 0 || glob[i - 1] == u'/';
        if (c == u'*' && i + 1 < size && glob[i + 1] == u'*' && atStart) {
            if (i + 2 == size) {
                out += u".*"_s;
                i += 1;
                continue;
            }
            if (glob[i + 2] == u'/') {
                out += u"(?:.*/)?"_s;
                i += 2;
                continue;
            }
        }
        if (c == u'*') {
            out += u"[^/]*"_s;
        } else if (c == u'?') {
            out += u"[^/]"_s;
        } else if (c == u'\\' && i + 1 < size) {
            out += QRegularExpression::escape(glob[++i]);
        } else if (c == u'[') {
            qsizetype end = i + 1;
            if (end < size && (glob[end] == u'!' || glob[end] == u'^'))
                ++end;
            if (end < size && glob[end] == u']')
                ++end;
            while (end < size && glob[end] != u']')
                ++end;
            if (end >= size) {
                out += u"\\["_s;
                continue;
            }
            QString set = glob.sliced(i + 1, end - i - 1).toString();
            if (set.startsWith(u'!'))
                set[0] = u'^';
            set.replace(u"\\"_s, u"\\\\"_s);
            out += u'[' + set + u']';
            i = end;
        } else {
            out += QRegularExpression::escape(c);
        }
    }
    return out;
}

bool isJot(const QString &path)
{
    return path == u".jot" || path.startsWith(u".jot/"_s);
}

} // namespace

VaultIgnore::VaultIgnore(const Vault &vault)
    : VaultIgnore(vault.root(), patternsOf(vault), respectsGitignore(vault))
{}

QStringList VaultIgnore::patternsOf(const Vault &vault)
{
    QString error;
    const std::optional<QString> text =
        notefile::read(vault.absolute(u".jot/vault.yaml"_s), &error);
    if (!text)
        return {};
    const QVariant patterns = readFrontMatter(*text).value(u"ignore"_s);
    if (patterns.typeId() == QMetaType::QStringList)
        return patterns.toStringList();
    const QString one = patterns.toString().trimmed();
    return one.isEmpty() ? QStringList() : QStringList{one};
}

bool VaultIgnore::respectsGitignore(const Vault &vault)
{
    const QString said = vault.setting(u"gitignore"_s).toLower();
    return !(said == u"false" || said == u"no" || said == u"off");
}

VaultIgnore::VaultIgnore(QString root, const QStringList &patterns, bool gitignore)
    : m_root(std::move(root)), m_own(parse(patterns)), m_gitignore(gitignore)
{}

bool VaultIgnore::empty() const
{
    return m_own.isEmpty() && !m_gitignore;
}

QList<VaultIgnore::Rule> VaultIgnore::parse(const QStringList &lines)
{
    QList<Rule> rules;
    for (QString line : lines) {
        if (line.endsWith(u'\r'))
            line.chop(1);
        // Trailing spaces go unless a backslash keeps the last one.
        while (line.endsWith(u' ') && !line.endsWith(u"\\ "_s))
            line.chop(1);
        if (line.isEmpty() || line.startsWith(u'#'))
            continue;
        Rule rule;
        if (line.startsWith(u'!')) {
            rule.negated = true;
            line.remove(0, 1);
        } else if (line.startsWith(u"\\!"_s) || line.startsWith(u"\\#"_s)) {
            line.remove(0, 1);
        }
        if (line.endsWith(u'/')) {
            rule.foldersOnly = true;
            line.chop(1);
        }
        if (line.isEmpty())
            continue;
        // A slash anywhere but the end ties the pattern to its own folder; otherwise it matches a
        // name at any depth.
        const bool anchored = line.contains(u'/');
        if (line.startsWith(u'/'))
            line.remove(0, 1);
        rule.pattern.setPattern((anchored ? u"^"_s : u"^(?:.*/)?"_s) + globToRegex(line) + u'$');
        if (rule.pattern.isValid())
            rules.append(rule);
    }
    return rules;
}

const QList<VaultIgnore::Rule> &VaultIgnore::gitignoreIn(const QString &folder) const
{
    auto it = m_read.constFind(folder);
    if (it != m_read.cend())
        return *it;
    QList<Rule> rules;
    QFile file(m_root + u'/' + join(folder, u".gitignore"_s));
    if (file.open(QIODevice::ReadOnly))
        rules = parse(QString::fromUtf8(file.readAll()).split(u'\n'));
    return *m_read.insert(folder, rules);
}

bool VaultIgnore::ignoredIn(const QString &parent, const QString &name, bool folder) const
{
    const QString path = join(parent, name);
    if (empty() || isJot(path))
        return false;
    bool ignored = false;
    const auto apply = [&](const QList<Rule> &rules, const QString &base) {
        const QString relative = base.isEmpty() ? path : path.mid(base.size() + 1);
        for (const Rule &rule : rules) {
            if ((!rule.foldersOnly || folder) && rule.pattern.match(relative).hasMatch())
                ignored = !rule.negated;
        }
    };
    apply(m_own, {});
    if (m_gitignore) {
        // Deeper .gitignore files come later, so their rules win, as in git.
        apply(gitignoreIn({}), {});
        QString base;
        for (const QString &part : parent.split(u'/', Qt::SkipEmptyParts)) {
            base = join(base, part);
            apply(gitignoreIn(base), base);
        }
    }
    return ignored;
}

bool VaultIgnore::ignored(const QString &path, bool folder) const
{
    if (empty() || isJot(path))
        return false;
    const QStringList parts = path.split(u'/', Qt::SkipEmptyParts);
    if (parts.isEmpty())
        return false;
    // Nothing inside an ignored folder comes back, as git cannot re-include it either.
    QString parent;
    for (qsizetype i = 0; i + 1 < parts.size(); ++i) {
        const QString here = join(parent, parts.at(i));
        auto it = m_folders.constFind(here);
        if (it == m_folders.cend())
            it = m_folders.insert(here, ignoredIn(parent, parts.at(i), true));
        if (*it)
            return true;
        parent = here;
    }
    return ignoredIn(parent, parts.last(), folder);
}

} // namespace jot
