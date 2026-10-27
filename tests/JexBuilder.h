#pragma once

#include <QByteArray>
#include <QList>
#include <QString>

#include <algorithm>
#include <utility>

// Joplin JEX exports made in memory, for tests, so no binary fixtures sit in the repo.

using namespace Qt::StringLiterals;

/// One tar entry: a 512-byte header, then `data` padded to whole blocks. `type` is '0' for a
/// file, 'x' for a pax header and 'L' for a GNU long name; `prefix` is ustar's path prefix.
inline QByteArray tarEntry(const QByteArray &name, const QByteArray &data, char type = '0',
                           const QByteArray &prefix = {})
{
    // Each field has a fixed width; the value is written at its start, padded with NULs.
    const auto put = [](QByteArray &header, qsizetype at, const QByteArray &value) {
        std::copy(value.cbegin(), value.cend(), header.begin() + at);
    };
    QByteArray header(512, '\0');
    put(header, 0, name);
    put(header, 100, "0000644");
    put(header, 124, QByteArray::number(data.size(), 8).rightJustified(11, '0'));
    put(header, 148, QByteArray(8, ' '));
    header[156] = type;
    put(header, 257,
        QByteArray("ustar\0"
                   "00",
                   8));
    put(header, 345, prefix);
    int sum = 0;
    for (const char c : std::as_const(header))
        sum += static_cast<unsigned char>(c);
    put(header, 148, QByteArray::number(sum, 8).rightJustified(6, '0') + '\0');
    return header + data + QByteArray((512 - data.size() % 512) % 512, '\0');
}

/// A ustar archive of `files`, by path, as Joplin writes a JEX.
inline QByteArray tar(const QList<std::pair<QString, QByteArray>> &files)
{
    QByteArray out;
    for (const auto &[path, data] : files)
        out += tarEntry(path.toUtf8(), data);
    return out + QByteArray(1024, '\0');
}

/// A raw item as Joplin exports it: the title and body, if any, then its properties.
inline QByteArray item(const QString &title, const QString &body, const QString &props)
{
    QString text;
    if (!title.isEmpty())
        text = title + u"\n\n"_s + (body.isEmpty() ? QString() : body + u"\n\n"_s);
    return (text + props).toUtf8();
}
