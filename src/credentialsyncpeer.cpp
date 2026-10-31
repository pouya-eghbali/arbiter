#include "credentialsyncpeer.h"
#include "credentialsyncstate.h"
#include <QCryptographicHash>
#include <QHostAddress>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QSslCertificate>
#include <QSslConfiguration>
#include <QUrlQuery>
#include <memory>

namespace ArbiterSync {
namespace {
QByteArray decode(const QString &s) { return QByteArray::fromBase64(s.toLatin1(), QByteArray::Base64UrlEncoding | QByteArray::AbortOnBase64DecodingErrors); }
}
bool localEndpoint(const QUrl &url) {
    if (url.scheme() != "https" || !url.userInfo().isEmpty() || url.hasQuery() || url.hasFragment() ||
        (!url.path().isEmpty() && url.path() != "/")) return false;
    const auto host = url.host().toLower();
    if (host == "localhost" || host.endsWith(".local")) return true;
    const QHostAddress a(host);
    return a.isLoopback() || a.isInSubnet(QHostAddress("10.0.0.0"), 8) ||
        a.isInSubnet(QHostAddress("172.16.0.0"), 12) || a.isInSubnet(QHostAddress("192.168.0.0"), 16) ||
        a.isInSubnet(QHostAddress("fc00::"), 7) || a.isInSubnet(QHostAddress("fe80::"), 10);
}
QJsonObject pairingInfo(const QString &invitation) {
    if (invitation.size() > 16000) return {};
    const QUrl url(invitation.trimmed());
    if (url.scheme() != "arbiter-pair" || url.host() != "v1") return {};
    const auto p = QJsonDocument::fromJson(decode(QUrlQuery(url).queryItemValue("data"))).object();
    if (p.value("v") != 1 || !localEndpoint(QUrl(p.value("endpoint").toString())) ||
        decode(p.value("key").toString()).size() != 32 || decode(p.value("code").toString()).size() != 32 ||
        p.value("name").toString().isEmpty() || p.value("name").toString().size() > 80 ||
        !QRegularExpression("^[0-9a-f]{64}$").match(p.value("pin").toString()).hasMatch() ||
        !QRegularExpression("^[a-zA-Z0-9-]{16,80}$").match(p.value("vaultId").toString()).hasMatch()) return {};
    return p;
}
void exchange(QObject *owner, const QString &path, const QJsonObject &peer, const QByteArray &token,
              const QJsonObject &payload, Seal seal, Open open,
              std::function<void(QString, QJsonObject)> done) {
    QUrl endpoint(peer.value("endpoint").toString());
    auto key = decode(peer.value("key").toString());
    const auto pin = peer.value("pin").toString().toLatin1();
    if (!localEndpoint(endpoint) || key.size() != 32 || token.size() != 32 ||
        !QRegularExpression("^[0-9a-f]{64}$").match(QString::fromLatin1(pin)).hasMatch()) {
        done(QObject::tr("Invalid sync connection."), {}); return;
    }
    const auto context = "ArbiterSync/v1/" + peer.value("vaultId").toString().toUtf8();
    auto clear = QJsonDocument(payload).toJson(QJsonDocument::Compact);
    if (clear.size() > MaximumBytes) { clear.fill(0); done(QObject::tr("Sync data is too large."), {}); return; }
    auto body = seal(clear, key, context); clear.fill(0);
    if (body.isEmpty()) { done(QObject::tr("Could not encrypt sync data."), {}); return; }
    endpoint.setPath(path);
    QNetworkRequest request(endpoint);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    request.setTransferTimeout(10000);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/octet-stream");
    request.setRawHeader("Authorization", "Bearer " + token.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));
    auto manager = new QNetworkAccessManager(owner);
    auto reply = manager->post(request, body); body.fill(0);
    auto pinned = std::make_shared<bool>(false);
    auto received = std::make_shared<QByteArray>();
    QObject::connect(reply, &QNetworkReply::sslErrors, reply, [reply, pin, pinned] {
        const auto cert = reply->sslConfiguration().peerCertificate();
        if (!cert.isNull() && cert.digest(QCryptographicHash::Sha256).toHex() == pin) {
            *pinned = true; reply->ignoreSslErrors();
        } else reply->abort();
    });
    QObject::connect(reply, &QNetworkReply::encrypted, reply, [reply, pin, pinned] {
        if (reply->sslConfiguration().peerCertificate().digest(QCryptographicHash::Sha256).toHex() != pin) reply->abort();
        else *pinned = true;
    });
    QObject::connect(reply, &QIODevice::readyRead, reply, [reply, received] {
        received->append(reply->readAll()); if (received->size() > MaximumBytes + 28) reply->abort();
    });
    QObject::connect(reply, &QNetworkReply::finished, owner,
        [reply, manager, received, pinned, key, context, open, done = std::move(done)]() mutable {
            received->append(reply->readAll()); bool ok = false;
            const auto status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            auto clear = *pinned && received->size() <= MaximumBytes + 28 && status == 200 && reply->error() == QNetworkReply::NoError
                ? open(*received, key, context, &ok) : QByteArray();
            key.fill(0); received->fill(0);
            const auto result = QJsonDocument::fromJson(clear).object(); clear.fill(0);
            manager->deleteLater();
            done(ok && !result.isEmpty() ? QString() : status == 401 ? QObject::tr("Device access was revoked. Pair again to reconnect.")
                : QObject::tr("Could not sync. Keep the other device open and unlocked on the same Wi-Fi."), result);
        });
}
}
