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
    bool configured() const { return true; } // Automatically created; no setup UI.
    bool busy() const { return m_busy; }
    QString error() const { return m_error; }
    bool biometricAvailable() const { return m_biometrics && m_biometrics->available(); }
    QString biometricName() const { return m_biometrics ? m_biometrics->name() : tr("Biometrics"); }
    QString biometricStatus() const { return m_biometrics ? m_biometrics->status() : tr("Biometric support is unavailable."); }
    void lockBiometrics();
    void cancelBiometric();
    QVariantList credentials(const QString &rp) const;
    void perform(bool registration, const QByteArray &request, const QByteArray &credentialId,
                 Result callback, std::function<bool()> current = {});
    // Native-only transfer session, separately bounded from website verification.
    using SyncResult=std::function<void(const QString &,const QJsonArray &)>;
    using SyncWrite=std::function<void(const QString &)>;
    void unlockForSync(SyncResult callback);
    void exportForSync(SyncResult callback);
    void upsertFromSync(const QJsonObject &data,SyncWrite callback);
    void removeFromSync(const QByteArray &credentialId,SyncWrite callback);
    bool syncReady() const { return m_syncKey.present(); }
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
    void automaticKey(std::function<void(QByteArray)> callback);
    void saveRootKey(const QByteArray &key, std::function<void(QByteArray)> callback);
    QPointer<SecretStore> m_store;
    QString m_directory;
    QCborMap m_index;
    std::unique_ptr<QLockFile> m_lock;
    bool m_busy = false;
    QString m_error;
    QPointer<BiometricAuth> m_biometrics;
    quint64 m_syncEpoch=0;
    LockedKey m_syncKey;
    quint64 m_biometricEpoch = 0;
};
