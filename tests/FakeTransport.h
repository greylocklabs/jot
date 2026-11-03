#pragma once

#include "jot/Sync.h"

#include <QCryptographicHash>
#include <QMap>

#include <functional>
#include <memory>

/// A remote held in memory, which several devices' engines can share. Each write makes a new
/// rev, as a real store would.
struct FakeRemote
{
    struct File
    {
        QByteArray data;
        QString rev;
    };
    QMap<QString, File> files;
    QMap<QString, QByteArray> trash;
    int revs = 0;
    /// Called before each write, so a test can change the remote under it.
    std::function<void(const QString &path)> beforePut;
    /// Lists fail while set, as an unreachable remote does.
    bool down = false;

    QString nextRev() { return QString::number(++revs); }
};

class FakeTransport : public jot::Transport
{
public:
    explicit FakeTransport(std::shared_ptr<FakeRemote> remote) : m_remote(std::move(remote)) {}

    std::optional<QList<jot::RemoteFile>> list(QString *error) override
    {
        if (m_remote->down) {
            *error = QStringLiteral("unreachable");
            return std::nullopt;
        }
        QList<jot::RemoteFile> files;
        for (auto it = m_remote->files.cbegin(); it != m_remote->files.cend(); ++it)
            files.append({it.key(), it->rev, {}});
        return files;
    }

    std::optional<QByteArray> get(const QString &path, QString *rev, QString *error) override
    {
        const auto found = m_remote->files.constFind(path);
        if (found == m_remote->files.cend()) {
            *error = QStringLiteral("not found");
            return std::nullopt;
        }
        *rev = found->rev;
        return found->data;
    }

    std::optional<QString> put(const QString &path, const QByteArray &data, const QString &expected,
                               bool *stale, QString *) override
    {
        if (m_remote->beforePut)
            m_remote->beforePut(path);
        const auto found = m_remote->files.constFind(path);
        const QString there = found == m_remote->files.cend() ? QString() : found->rev;
        if (there != expected) {
            *stale = true;
            return std::nullopt;
        }
        const QString rev = m_remote->nextRev();
        m_remote->files.insert(path, {data, rev});
        return rev;
    }

    bool remove(const QString &path, const QString &expected, bool *stale, QString *) override
    {
        const auto found = m_remote->files.constFind(path);
        if (found == m_remote->files.cend() || found->rev != expected) {
            *stale = true;
            return false;
        }
        m_remote->trash.insert(path, found->data);
        m_remote->files.remove(path);
        return true;
    }

private:
    std::shared_ptr<FakeRemote> m_remote;
};
