#pragma once

#include <QString>
#include <QStringList>
#include <QStringView>
#include <QVariant>
#include <QVariantMap>

namespace jot {

/// A note split into its YAML front matter and its Markdown body. Both views point into
/// the text that was split, which must outlive them.
struct NoteParts
{
    /// The YAML between the fences, without them. Empty when the note has none.
    QStringView frontMatter;
    /// Everything after the closing fence, or the whole note without front matter.
    QStringView body;
    bool hasFrontMatter = false;
};

/// Front matter is a `---` line at the very start of the note, closed by a `---` or `...`
/// line. Anything else, including an unclosed fence, is all body, so no text is lost.
NoteParts splitFrontMatter(QStringView note);

/// The top-level keys of front matter: a scalar as a QString and a list of scalars as a
/// QStringList. Anything richer, such as a nested map, is kept as its raw text. This reads
/// the YAML that notes use, not every YAML document.
[[nodiscard]] QVariantMap readFrontMatter(QStringView yaml);

/// `note` with `key` set to `value`, a QString or QStringList, or removed when `value` is
/// null. Only that key's lines change, so the rest of the front matter keeps its order,
/// comments and quoting. A note without front matter gets some.
[[nodiscard]] QString setFrontMatterKey(const QString &note, const QString &key,
                                        const QVariant &value);

/// A YAML scalar as written after a key or a list dash, with its quotes and comment removed.
[[nodiscard]] QString readYamlScalar(QStringView text);

/// `text` as a YAML scalar, quoted only when it would otherwise read as something else.
[[nodiscard]] QString yamlScalar(const QString &text);

} // namespace jot
