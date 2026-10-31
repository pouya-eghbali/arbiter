#pragma once
#include <QJsonObject>
#include <QObject>
#include <QUrl>
#include <functional>

namespace ArbiterSync {
bool localEndpoint(const QUrl &url);
QJsonObject pairingInfo(const QString &invitation);
using Seal = std::function<QByteArray(const QByteArray &, const QByteArray &, const QByteArray &)>;
using Open = std::function<QByteArray(const QByteArray &, const QByteArray &, const QByteArray &, bool *)>;
// Certificate pinning, redirect refusal and bounded responses are shared by
// desktop and mobile. Never expose this transport to website JavaScript.
void exchange(QObject *owner, const QString &path, const QJsonObject &peer, const QByteArray &token,
              const QJsonObject &payload, Seal seal, Open open,
              std::function<void(QString, QJsonObject)> done);
}
