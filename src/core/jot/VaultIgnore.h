#pragma once

#include <QHash>
#include <QList>
#include <QRegularExpression>
#include <QString>
#include <QStringList>

namespace jot {

class Vault;

/// The vault paths Jot leaves out besides hidden names: those the `ignore:` patterns in
/// `.jot/vault.yaml` name and, unless it says `gitignore: false`, those the vault's `.gitignore`
/// files ignore. Both are read as git reads a `.gitignore`, the vault's own patterns as though
/// they were one at the top of the vault. Jot's own `.jot` folder is never ignored.
class VaultIgnore
{
public:
    /// The rules `vault` sets, read now.
    explicit VaultIgnore(const Vault &vault);
    /// Rules for the vault at `root` from `patterns`, and its `.gitignore` files when `gitignore`.
    VaultIgnore(QString root, const QStringList &patterns, bool gitignore);

    /// Whether the file or, when `folder`, the folder at vault path `path` is left out, itself
    /// or as inside an ignored folder.
    [[nodiscard]] bool ignored(const QString &path, bool folder) const;
    /// As ignored(), for `name` in the vault folder `parent`, which is known not to be ignored.
    [[nodiscard]] bool ignoredIn(const QString &parent, const QString &name, bool folder) const;
    /// Whether any rule could leave something out.
    [[nodiscard]] bool empty() const;

    /// The `ignore:` patterns in `vault`'s `.jot/vault.yaml`, one string or a list.
    [[nodiscard]] static QStringList patternsOf(const Vault &vault);
    /// Whether `vault` respects its `.gitignore` files: unless it says `gitignore: false`.
    [[nodiscard]] static bool respectsGitignore(const Vault &vault);
    /// A pattern that matches the vault path `path` alone, from the top of the vault, its
    /// characters that patterns read otherwise escaped; a folder's ends with `/`.
    [[nodiscard]] static QString literal(const QString &path, bool folder);

private:
    struct Rule
    {
        QRegularExpression pattern;
        bool negated = false;
        bool foldersOnly = false;
    };
    static QList<Rule> parse(const QStringList &lines);
    /// The rules of the `.gitignore` in vault folder `folder`, read once.
    const QList<Rule> &gitignoreIn(const QString &folder) const;

    QString m_root;
    QList<Rule> m_own;
    bool m_gitignore = true;
    mutable QHash<QString, QList<Rule>> m_read;
    mutable QHash<QString, bool> m_folders;
};

} // namespace jot
