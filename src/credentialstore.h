#pragma once
#include "secretstore.h"
#include "lockedkey.h"
#include <QQueue>
#include <QPointer>
#include <QCborMap>
#include <QLockFile>
#include <memory>

// One encrypted store per browser profile. Only the random master key lives in
// the OS keyring. Website verification is performed by the credential managers;
// enabling sync authorizes unattended access for the lifetime of the profile.
class CredentialStore final : public SecretStore {
    Q_OBJECT
public:
    CredentialStore(const QString &directory, SecretStore *keys, QObject *parent=nullptr);
    bool automaticProtection() const override { return true; }
    void read(const QString &key, Callback callback) override;
    void readOptional(const QString &key, Callback callback) override;
    void readSilently(const QString &key, Callback callback) override;
    void readOptionalSilently(const QString &key, Callback callback) override;
    void write(const QString &key, const QByteArray &data, Callback callback) override;
    void remove(const QString &key, Callback callback) override;
private:
    struct Job { QString key; QByteArray data; int operation; bool interactive,optional; Callback callback; };
    void enqueue(Job job);
    void next();
    void opened(const QString &error,const QByteArray &key);
    void execute();
    void complete(const QString &error,const QByteArray &data={});
    bool save(const QCborMap &records);
    QString m_directory;
    QPointer<SecretStore> m_keys;
    LockedKey m_key;
    QByteArray m_id;
    QCborMap m_records;
    QQueue<Job> m_jobs;
    bool m_running=false;
    std::unique_ptr<class QLockFile> m_lock;
};
