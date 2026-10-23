#include "secretstore.h"
#include <keychain.h>

namespace {
void configure(QKeychain::Job *job, const QString &key)
{
    // QtKeychain's Windows Credential Store backend uses the key as the global
    // target name, so callers supply a fully namespaced key on every platform.
    job->setKey(key);
    job->setInsecureFallback(false);
}
}

void KeychainSecretStore::read(const QString &key, Callback callback)
{
    auto *job = new QKeychain::ReadPasswordJob(m_service, this);
    configure(job, key);
    connect(job, &QKeychain::Job::finished, this, [job, callback](QKeychain::Job *) {
        callback(job->error() ? tr("Could not read the credential from the system credential store. Unlock your keyring and try again.") : QString(),
                 job->error() ? QByteArray() : job->binaryData());
    });
    job->start();
}

void KeychainSecretStore::write(const QString &key, const QByteArray &data, Callback callback)
{
    auto *job = new QKeychain::WritePasswordJob(m_service, this);
    configure(job, key);
    job->setBinaryData(data);
    connect(job, &QKeychain::Job::finished, this, [job, callback](QKeychain::Job *) {
        callback(job->error() ? tr("Could not save the credential securely. Unlock your system keyring, or enable a supported credential store, and try again.") : QString(), {});
    });
    job->start();
}

void KeychainSecretStore::remove(const QString &key, Callback callback)
{
    auto *job = new QKeychain::DeletePasswordJob(m_service, this);
    configure(job, key);
    connect(job, &QKeychain::Job::finished, this, [job, callback](QKeychain::Job *) {
        callback(job->error() && job->error() != QKeychain::EntryNotFound
                     ? tr("Could not delete the credential from the system credential store. Try again after unlocking your keyring.") : QString(), {});
    });
    job->start();
}
