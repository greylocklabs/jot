#pragma once

#include <QString>

#include <optional>

/// Secrets kept in the desktop's keyring (the Secret Service), never in a file. Each call waits
/// for the keyring, which can ask the user to unlock it.
namespace jot::secrets {

/// The secret kept under `key`, or nothing, with `error` saying why, when there is none or no
/// keyring to ask.
[[nodiscard]] std::optional<QString> read(const QString &key, QString *error);
bool write(const QString &key, const QString &secret, QString *error);
/// Removing one that is not there counts as done.
bool remove(const QString &key, QString *error);

} // namespace jot::secrets
