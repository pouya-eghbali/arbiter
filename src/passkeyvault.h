#pragma once
#include "secretstore.h"
#include "biometricauth.h"
#include "lockedkey.h"
#include <QCborMap>
#include <QLockFile>
#include <QPointer>
#include <QVariantList>
#include <memory>

class PasskeyVault : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool configured READ configured NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
public:
    using Result = std::function<void(int status, const QByteArray &response)>;
    explicit PasskeyVault(SecretStore *store, const QString &directory, QObject *parent = nullptr, BiometricAuth *biometrics = nullptr);
    ~PasskeyVault() override;
    bool configured() const { return m_index.value(2).toBool(); }
    bool busy() const { return m_busy; }
    QString error() const { return m_error; }
    Q_INVOKABLE void setup(const QString &passphrase, const QString &confirmation);
    bool biometricEnabled() const { return m_index.value(4).toBool(); }
    bool biometricAvailable() const { return m_biometrics && m_biometrics->available(); }
    bool biometricReady() const;
    QString biometricName() const { return m_biometrics ? m_biometrics->name() : tr("Biometrics"); }
    QString biometricStatus() const { return m_biometrics ? m_biometrics->status() : tr("Biometric support is unavailable."); }
    void setBiometricEnabled(bool enabled, const QString &passphrase);
    void lockBiometrics();
    void cancelBiometric();
    QVariantList credentials(const QString &rp) const;
    void perform(bool registration, const QByteArray &request, const QByteArray &credentialId,
                 const QString &passphrase, Result callback, std::function<bool()> current = {}, bool biometric = false);
signals:
    void changed();
private:
    bool load();
    bool save();
    bool begin();
    void finish(const QString &error = {});
    QString itemKey(const QByteArray &id) const;
    void unlock(const QString &passphrase, std::function<void(QByteArray)> callback);
    void unlockBiometric(std::function<void(QByteArray)> callback);
    bool cacheKey(const QByteArray &key);
    QPointer<SecretStore> m_store;
    QString m_directory;
    QCborMap m_index;
    std::unique_ptr<QLockFile> m_lock;
    bool m_busy = false;
    QString m_error;
    int m_failures = 0;
    qint64 m_retryAfter = 0;
    QPointer<BiometricAuth> m_biometrics;
    LockedKey m_cachedKey;
    QByteArray m_cachedVaultId;
    QTimer m_cacheExpiry;
    quint64 m_biometricEpoch = 0;
};
