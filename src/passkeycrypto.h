#pragma once
#include <QByteArray>
#include <QCborMap>
#include <QString>

namespace PasskeyCrypto {
// No private material from this module is exposed to the renderer or QML.
void wipe(QByteArray &bytes);
QByteArray random(int size);
QByteArray derive(const QByteArray &passphrase, const QByteArray &salt);
QByteArray seal(const QByteArray &plain, const QByteArray &key, const QByteArray &context, qsizetype maximumBytes = 8192);
QByteArray open(const QByteArray &sealed, const QByteArray &key, const QByteArray &context, qsizetype maximumBytes = 8192);
struct Key { QByteArray der; QByteArray cose; };
Key generate();
// Normalize transferable P-256 keys to PKCS#8 and verify their public point.
Key portableKey(const QByteArray &der);
QByteArray sign(const QByteArray &der, const QByteArray &message);
QByteArray authenticatorData(const QString &rp, bool registration, bool backupEligible = false, bool backedUp = false);
}
