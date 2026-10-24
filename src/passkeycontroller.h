#pragma once
#include "passkeyvault.h"
#include <QWebEngineWebAuthUxRequest>
#include <QUrl>

class PasskeyController;
// Engine capability is available before a vault exists; the UI provisions the
// vault inside a pending registration. No private keys are exposed here.
class PasskeyProviderBridge final : public QObject {
    Q_OBJECT
    Q_PROPERTY(int abi READ abi CONSTANT)
    Q_PROPERTY(bool configured READ ready CONSTANT)
public:
    explicit PasskeyProviderBridge(PasskeyController *controller);
    int abi() const { return 1; }
    bool ready() const { return true; }
    Q_INVOKABLE QVariantList credentials(const QString &rp) const;
private:
    PasskeyController *m_controller;
};

class PasskeyController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool biometricAvailable READ biometricAvailable NOTIFY changed)
    Q_PROPERTY(bool biometricEnabled READ biometricEnabled NOTIFY changed)
    Q_PROPERTY(bool biometricReady READ biometricReady NOTIFY changed)
    Q_PROPERTY(QString biometricName READ biometricName NOTIFY changed)
    Q_PROPERTY(QString biometricStatus READ biometricStatus NOTIFY changed)
    Q_PROPERTY(int abi READ abi CONSTANT)
    Q_PROPERTY(bool enabled READ enabled CONSTANT)
    Q_PROPERTY(bool configured READ configured NOTIFY changed)
    Q_PROPERTY(bool active READ active NOTIFY changed)
    Q_PROPERTY(bool localAvailable READ localAvailable NOTIFY changed)
    Q_PROPERTY(bool phoneAvailable READ phoneAvailable NOTIFY changed)
    Q_PROPERTY(bool systemAvailable READ systemAvailable NOTIFY changed)
    Q_PROPERTY(bool verifying READ verifying NOTIFY changed)
    Q_PROPERTY(bool registration READ registration NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(int bluetooth READ bluetooth NOTIFY changed)
    Q_PROPERTY(QVariantList accounts READ accounts NOTIFY changed)
    Q_PROPERTY(QString origin READ origin NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(QUrl qrImage READ qrImage NOTIFY changed)
public:
    explicit PasskeyController(QObject *parent = nullptr);
    PasskeyController(SecretStore *store, const QString &directory, BiometricAuth *biometrics, QObject *parent = nullptr);
    ~PasskeyController() override;
    bool biometricAvailable() const { return m_vault.biometricAvailable(); }
    bool biometricEnabled() const { return m_vault.biometricEnabled(); }
    bool biometricReady() const { return m_vault.biometricReady(); }
    QString biometricName() const { return m_vault.biometricName(); }
    QString biometricStatus() const { return m_vault.biometricStatus(); }
    Q_INVOKABLE void refreshBiometrics() { m_biometrics->refresh(); }
    Q_INVOKABLE void setBiometricEnabled(bool enabled, const QString &phrase) { m_vault.setBiometricEnabled(enabled,phrase); }
    Q_INVOKABLE void cancelBiometric() { m_vault.cancelBiometric(); }
    Q_INVOKABLE void lockBiometrics() { m_vault.lockBiometrics(); }
    Q_INVOKABLE void verifyBiometric(int accountIndex);
    int abi() const { return 1; }
    bool enabled() const;
    bool configured() const { return m_vault.configured(); }
    bool active() const { return m_bridge; }
    bool localAvailable() const;
    bool phoneAvailable() const;
    bool systemAvailable() const;
    bool verifying() const;
    bool registration() const;
    bool busy() const { return m_vault.busy(); }
    int bluetooth() const;
    QVariantList accounts() const;
    QString origin() const;
    QString error() const { return m_vault.error(); }
    QUrl qrImage() const { return m_qr; }
    Q_INVOKABLE QVariantList credentials(const QString &rp) const { return m_vault.credentials(rp); }
    Q_INVOKABLE void handle(QWebEngineWebAuthUxRequest *request);
    Q_INVOKABLE void clear();
    Q_INVOKABLE void setup(const QString &passphrase, const QString &confirmation);
    Q_INVOKABLE void setupForRequest(const QString &passphrase,const QString &confirmation);
    Q_INVOKABLE void useLocal();
    Q_INVOKABLE void usePhone();
    Q_INVOKABLE void useSystem();
    Q_INVOKABLE void powerOnBluetooth();
    Q_INVOKABLE void requestBluetoothPermission();
    Q_INVOKABLE void verify(int accountIndex, const QString &passphrase);
signals:
    void changed();
    void verificationSubmitted();
private slots:
    void update();
private:
    void verifyLocal(int accountIndex, const QString &phrase, bool biometric);
    PasskeyProviderBridge m_provider;
    QString m_pendingSetupPhrase;
    BiometricAuth *m_biometrics;
    KeychainSecretStore m_store;
    PasskeyVault m_vault;
    QPointer<QObject> m_bridge;
    QPointer<QWebEngineWebAuthUxRequest> m_request;
    QUrl m_qr;
    quint64 m_operationEpoch = 0;
};
