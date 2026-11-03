#include "jot/Secrets.h"

#include "greylock/Identity.h"

#include <QEventLoop>

#include <qt6keychain/keychain.h>

namespace jot::secrets {

namespace {

/// Runs `job` to its end and says what went wrong, if anything.
QKeychain::Error run(QKeychain::Job &job, const QString &key, QString *error)
{
    job.setAutoDelete(false);
    job.setInsecureFallback(false);
    job.setKey(key);
    QEventLoop loop;
    QObject::connect(&job, &QKeychain::Job::finished, &loop, &QEventLoop::quit);
    job.start();
    loop.exec();
    if (job.error() != QKeychain::NoError)
        *error = job.errorString();
    return job.error();
}

} // namespace

std::optional<QString> read(const QString &key, QString *error)
{
    QKeychain::ReadPasswordJob job(greylock::identity::appId());
    if (run(job, key, error) != QKeychain::NoError)
        return std::nullopt;
    return job.textData();
}

bool write(const QString &key, const QString &secret, QString *error)
{
    QKeychain::WritePasswordJob job(greylock::identity::appId());
    job.setTextData(secret);
    return run(job, key, error) == QKeychain::NoError;
}

bool remove(const QString &key, QString *error)
{
    QKeychain::DeletePasswordJob job(greylock::identity::appId());
    const QKeychain::Error result = run(job, key, error);
    if (result == QKeychain::EntryNotFound)
        error->clear();
    return result == QKeychain::NoError || result == QKeychain::EntryNotFound;
}

} // namespace jot::secrets
