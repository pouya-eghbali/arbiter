#pragma once
#include "passkeyvault.h"
#include <QWebEngineWebAuthUxRequest>
#include <QUrl>

class PasskeyController final : public QObject
{
    Q_OBJECT
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
    ~PasskeyController() override;
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
    KeychainSecretStore m_store;
    PasskeyVault m_vault;
    QPointer<QObject> m_bridge;
    QPointer<QWebEngineWebAuthUxRequest> m_request;
    QUrl m_qr;
};
