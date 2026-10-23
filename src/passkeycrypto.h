#pragma once
#include <QByteArray>
#include <QCborMap>
#include <QString>

namespace PasskeyCrypto {
// No private material from this module is exposed to the renderer or QML.
void wipe(QByteArray &bytes);
QByteArray random(int size);
QByteArray derive(const QByteArray &passphrase, const QByteArray &salt);
QByteArray seal(const QByteArray &plain, const QByteArray &key, const QByteArray &context);
QByteArray open(const QByteArray &sealed, const QByteArray &key, const QByteArray &context);
struct Key { QByteArray der; QByteArray cose; };
Key generate();
QByteArray sign(const QByteArray &der, const QByteArray &message);
QByteArray authenticatorData(const QString &rp, bool registration);
}
