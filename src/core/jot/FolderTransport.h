#pragma once

#include "jot/Sync.h"

namespace jot {

/// A plain folder as a sync remote, such as a NAS mount, a USB drive or a network share. It
/// holds the vault's files as they are, readable without Jot; what a sync deletes there goes to
/// its own `.jot/trash/`.
class FolderTransport : public Transport
{
public:
    explicit FolderTransport(QString root);

    std::optional<QList<RemoteFile>> list(QString *error) override;
    std::optional<QByteArray> get(const QString &path, QString *rev, QString *error) override;
    std::optional<QString> put(const QString &path, const QByteArray &data, const QString &expected,
                               bool *stale, QString *error) override;
    bool remove(const QString &path, const QString &expected, bool *stale, QString *error) override;

private:
    /// The file's version as its size and time, or empty when there is none.
    [[nodiscard]] QString revOf(const QString &path) const;

    QString m_root;
};

} // namespace jot
