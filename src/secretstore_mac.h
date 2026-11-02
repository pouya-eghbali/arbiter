#pragma once
#include "secretstore.h"
void readMacSecret(SecretStore *, const QString &, const QString &, bool, bool, SecretStore::Callback);
void writeMacSecret(SecretStore *, const QString &, const QString &, const QByteArray &, SecretStore::Callback);
void removeMacSecret(SecretStore *, const QString &, const QString &, SecretStore::Callback);
