#pragma once
#include "importdata.h"
#include <QVariantMap>
#include <optional>

namespace ChromeVault {
enum class Cipher { MacCbc,LinuxCbc,WindowsGcm };
std::optional<QByteArray> decrypt(const QByteArray &ciphertext,const QByteArray &key,Cipher cipher);
QByteArray deriveCbcKey(const QByteArray &password,int iterations);
// Read-only local import. The OS credential store controls access to its key.
ImportPreview read(const QVariantMap &source);
// Injected key path for synthetic integration fixtures; never exposed to QML.
ImportPreview readWithKey(const QVariantMap &source,const QByteArray &key,Cipher cipher);
}
