#pragma once
#include "secretstore.h"
#include "biometricauth.h"
#include "lockedkey.h"
#include <QCborMap>
#include <QLockFile>
#include <QPointer>
#include <QVariantList>
#include <QJsonArray>
#include <QJsonObject>
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
    bool biometricPersistenceSupported() const { return m_biometrics && m_biometrics->persistentKeysSupported(); }
    bool biometricPersistent() const { return m_index.value(5).toByteArray().size() == 16; }
    bool biometricNeedsEnrollment() const { return biometricEnabled() && biometricPersistenceSupported() && !biometricPersistent(); }
    QString biometricName() const { return m_biometrics ? m_biometrics->name() : tr("Biometrics"); }
    QString biometricStatus() const { return m_biometrics ? m_biometrics->status() : tr("Biometric support is unavailable."); }
    void setBiometricEnabled(bool enabled, const QString &passphrase);
    void lockBiometrics();
    void cancelBiometric();
    QVariantList credentials(const QString &rp) const;
    void perform(bool registration, const QByteArray &request, const QByteArray &credentialId,
                 const QString &passphrase, Result callback, std::function<bool()> current = {}, bool biometric = false);
    // Native-only transfer session, separately bounded from website verification.
    using SyncResult=std::function<void(const QString &,const QJsonArray &)>;
    using SyncWrite=std::function<void(const QString &)>;
    void unlockForSync(const QString &passphrase,bool biometric,SyncResult callback);
    void exportForSync(SyncResult callback);
    void upsertFromSync(const QJsonObject &data,SyncWrite callback);
    void removeFromSync(const QByteArray &credentialId,SyncWrite callback);
    bool syncReady() const { return m_syncKey.present() && (m_syncStayUnlocked || m_syncExpiry.isActive()); }
    void setSyncStayUnlocked(bool enabled);
    void lockSync();
signals:
    void changed();
    void syncLocked();
private:
    bool load();
    bool save();
    bool begin();
    void finish(const QString &error = {});
    QString itemKey(const QByteArray &id) const;
    void unlock(const QString &passphrase, std::function<void(QByteArray)> callback);
    QString biometricKeyId(const QByteArray &reference) const { return itemKey({}) + QStringLiteral("biometric/") + QString::fromLatin1(reference.toHex()); }
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
    quint64 m_syncEpoch=0;
    bool m_syncStayUnlocked=false;
    LockedKey m_syncKey;
    QTimer m_syncExpiry;
    LockedKey m_cachedKey;
    QByteArray m_cachedVaultId;
    QTimer m_cacheExpiry;
    quint64 m_biometricEpoch = 0;
};
