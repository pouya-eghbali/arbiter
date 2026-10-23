// Arbiter's private, versioned browser-process WebAuthn bridge.
// SPDX-License-Identifier: BSD-3-Clause
#ifndef ARBITER_WEBAUTHN_H
#define ARBITER_WEBAUTHN_H
#include <QObject>
#include <QByteArray>
#include <QPointer>
#include <QString>
#include <functional>
#include "device/fido/fido_discovery_base.h"

namespace QtWebEngineCore {
class ArbiterWebAuthnRequest final : public QObject {
    Q_OBJECT
    Q_PROPERTY(int abi READ abi CONSTANT)
    Q_PROPERTY(QString origin MEMBER origin CONSTANT)
    Q_PROPERTY(QString rp MEMBER rp CONSTANT)
    Q_PROPERTY(bool registration MEMBER registration CONSTANT)
    Q_PROPERTY(bool localAvailable MEMBER localAvailable NOTIFY changed)
    Q_PROPERTY(bool phoneAvailable MEMBER phoneAvailable NOTIFY changed)
    Q_PROPERTY(bool systemAvailable MEMBER systemAvailable NOTIFY changed)
    Q_PROPERTY(int bluetooth MEMBER bluetooth NOTIFY changed)
    Q_PROPERTY(QByteArray payload MEMBER payload NOTIFY changed)
    Q_PROPERTY(QString qrPayload MEMBER qrPayload NOTIFY changed)
public:
    int abi() const { return 1; }
    QString origin, rp, qrPayload;
    QByteArray payload;
    bool registration = false, localAvailable = false, phoneAvailable = false, systemAvailable = false;
    int bluetooth = 1;
    std::function<void()> dispatchLocal, dispatchSystem, powerBluetooth, permitBluetooth;
    Q_INVOKABLE void useLocal() { if (dispatchLocal) dispatchLocal(); }
    Q_INVOKABLE void useSystem() { if (dispatchSystem) dispatchSystem(); }
    Q_INVOKABLE void powerOnBluetooth() { if (powerBluetooth) powerBluetooth(); }
    Q_INVOKABLE void requestBluetoothPermission() { if (permitBluetooth) permitBluetooth(); }
    Q_INVOKABLE void complete(const QByteArray &response, int status) { Q_EMIT completed(response, status); }
Q_SIGNALS:
    void changed();
    void completed(const QByteArray &response, int status);
};
QObject *arbiterPasskeyProvider();
std::unique_ptr<device::FidoDiscoveryBase> createArbiterDiscovery(ArbiterWebAuthnRequest *request);
}
#endif
