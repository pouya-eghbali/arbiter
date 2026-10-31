#include "mobilevault.h"
#include "mobileplatform.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMutex>
#include <QMutexLocker>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSslCertificate>
#include <QSslConfiguration>
#include <QSslError>
#include <QSysInfo>
#include <QTimer>
#include <QUrlQuery>
#include <QUuid>
#include <memory>
#ifdef Q_OS_UNIX
#include <fcntl.h>
#include <unistd.h>
#else
#include <QLockFile>
#endif
namespace {
QRecursiveMutex transactionMutex;
QByteArray decode(const QString &s) {
    return QByteArray::fromBase64(s.toLatin1(),
                                  QByteArray::Base64UrlEncoding | QByteArray::AbortOnBase64DecodingErrors);
}
QString encode(const QByteArray &s) {
    return QString::fromLatin1(s.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));
}
QJsonObject object(const QByteArray &bytes) {
    return QJsonDocument::fromJson(bytes).object();
}
} // namespace
MobileVault::MobileVault(QObject *parent) : QObject(parent) {
    m_expiry.setSingleShot(true);
    connect(&m_expiry, &QTimer::timeout, this, &MobileVault::lock);
}
void MobileVault::fail(const QString &error) {
    m_error = error;
    m_busy = false;
    emit changed();
    emit completed(false);
}
bool MobileVault::transaction(const std::function<bool(QJsonObject &, ArbiterSync::State &)> &edit,
                              bool create) {
    QMutexLocker guard(&transactionMutex);
    const auto directory = MobilePlatform::vaultDirectory();
    if (directory.isEmpty())
        return false;
    QDir().mkpath(directory);
    const auto path = directory + "/credentials-v1.enc";
#ifdef Q_OS_UNIX
    const int fd = ::open((path + ".lock").toUtf8().constData(), O_CREAT | O_RDWR, 0600);
    if (fd < 0)
        return false;
    struct flock record {};
    record.l_type = F_WRLCK;
    record.l_whence = SEEK_SET;
    if (fcntl(fd, F_SETLKW, &record) < 0) {
        ::close(fd);
        return false;
    }
    struct Guard {
        int fd;
        ~Guard() {
            ::close(fd);
        }
    } unlock{fd};
#else
    QLockFile record(path + ".lock");
    if (!record.tryLock(5000))
        return false;
#endif
    QByteArray key = MobilePlatform::vaultKey(create);
    if (key.size() != 32)
        return false;
    struct Wipe {
        QByteArray &key;
        ~Wipe() {
            key.fill(0);
        }
    } wipe{key};
    QFile file(path);
    QJsonObject document;
    ArbiterSync::State state;
    QString error;
    if (file.exists()) {
        if (file.size() > ArbiterSync::MaximumBytes + 28 || !file.open(QIODevice::ReadOnly))
            return false;
        bool ok = false;
        auto clear = MobilePlatform::open(file.readAll(), key, "ArbiterMobileVault/v1", &ok);
        document = object(clear);
        clear.fill(0);
        if (!ok || !ArbiterSync::State::parse(document.value("state").toObject(), &state, &error))
            return false;
    } else {
        if (!create)
            return false;
        document = {{"device", QUuid::createUuid().toString(QUuid::WithoutBraces)}, {"state", state.json()}};
    }
    if (!edit(document, state))
        return false;
    document["state"] = state.json();
    auto clear = QJsonDocument(document).toJson(QJsonDocument::Compact);
    auto sealed = MobilePlatform::seal(clear, key, "ArbiterMobileVault/v1");
    clear.fill(0);
    if (sealed.isEmpty())
        return false;
    QSaveFile output(path);
    if (!output.open(QIODevice::WriteOnly) || output.write(sealed) != sealed.size() || !output.commit())
        return false;
    QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    m_document = document;
    m_state = state;
    MobilePlatform::refreshIdentities();
    return true;
}
bool MobileVault::load(bool create) {
    return transaction([](QJsonObject &, ArbiterSync::State &) { return true; }, create);
}
bool MobileVault::persist(bool changePeer) {
    const auto desired = m_state;
    const auto peer = m_document.value("peer");
    return transaction(
        [&](QJsonObject &doc, ArbiterSync::State &state) {
            QString error;
            if (!state.merge(desired, &error))
                return false;
            if (changePeer) {
                if (peer.isObject())
                    doc["peer"] = peer;
                else
                    doc.remove("peer");
            }
            return true;
        },
        true);
}
QVariantList MobileVault::entries() const {
    QVariantList result;
    if (!unlocked())
        return result;
    const auto records = m_state.records();
    for (auto i = records.begin(); i != records.end(); ++i) {
        const auto versions = i.value().toArray();
        if (versions.size() != 1 || versions[0].toObject().value("deleted").toBool())
            continue;
        auto data = versions[0].toObject().value("data").toObject();
        data.remove("password");
        data.remove("privateKey");
        data["id"] = i.key();
        result.append(data.toVariantMap());
    }
    return result;
}
QVariantList MobileVault::conflictChoices() const {
    QVariantList choices;
    if (!unlocked())
        return choices;
    const auto records = m_state.records();
    for (auto i = records.begin(); i != records.end(); ++i) {
        auto versions = i.value().toArray();
        if (versions.size() < 2)
            continue;
        for (const auto &value : versions) {
            auto version = value.toObject();
            auto data = version.value("data").toObject();
            data.remove("password");
            data.remove("privateKey");
            data["id"] = i.key();
            data["deleted"] = version.value("deleted");
            data["device"] = version.value("clock").toObject().keys().join(", ").left(8);
            data["version"] = QString::fromLatin1(
                QCryptographicHash::hash(QJsonDocument(version).toJson(QJsonDocument::Compact),
                                         QCryptographicHash::Sha256)
                    .toHex());
            choices.append(data.toVariantMap());
        }
    }
    return choices;
}
void MobileVault::resolveConflict(const QString &id, const QString &version) {
    const auto shown = m_state.records().value(id).toArray();
    if (shown.size() < 2)
        return;
    authenticated([this, id, version, shown] {
        QString error;
        const bool saved = transaction(
            [&](QJsonObject &doc, ArbiterSync::State &state) {
                const auto latest = state.records().value(id).toArray();
                if (latest != shown)
                    return false;
                for (const auto &value : latest) {
                    const auto selected = value.toObject();
                    if (QString::fromLatin1(
                            QCryptographicHash::hash(QJsonDocument(selected).toJson(QJsonDocument::Compact),
                                                     QCryptographicHash::Sha256)
                                .toHex()) == version)
                        return state.update(id, doc.value("device").toString(),
                                            selected.value("data").toObject(),
                                            selected.value("deleted").toBool(), &error);
                }
                return false;
            },
            false);
        if (!saved)
            fail(tr("Credential changes were updated. Unlock the vault and review them again."));
        else {
            emit changed();
            emit completed(true);
        }
    });
}
void MobileVault::authenticated(std::function<void()> action) {
    if (m_verified.isValid() && m_cooldown > 0 && m_verified.elapsed() < m_cooldown * 1000) {
        action();
        return;
    }
    const auto epoch = m_epoch;
    QPointer<MobileVault> self(this);
    MobilePlatform::verify(tr("Unlock your Arbiter credential vault"),
                           [self, epoch, action = std::move(action)](bool approved) {
                               if (!self || self->m_epoch != epoch)
                                   return;
                               if (!approved) {
                                   self->fail(tr("Authentication cancelled."));
                                   return;
                               }
                               if (!self->load(true)) {
                                   self->fail(tr("Could not unlock the vault."));
                                   return;
                               }
                               self->m_verified.start();
                               self->m_expiry.start((self->m_cooldown > 0 ? self->m_cooldown : 60) * 1000);
                               emit self->changed();
                               action();
                           });
}
void MobileVault::unlock() {
    authenticated([this] {
        m_error.clear();
        emit changed();
    });
}
void MobileVault::lock() {
    m_expiry.stop();
    ++m_epoch;
    m_verified.invalidate();
    m_state = {};
    m_document = {};
    m_pairing = {};
    m_busy = false;
    emit changed();
}
void MobileVault::remove(const QString &id) {
    authenticated([this, id] {
        QString error;
        if (!m_state.update(id, m_document.value("device").toString(), {}, true, &error) || !persist())
            fail(tr("Could not update the vault."));
        else
            emit changed();
    });
}
void MobileVault::readPassword(const QString &id, const QString &origin,
                               std::function<void(QString, QString)> callback) {
    authenticated([this, id, origin, callback = std::move(callback)] {
        auto a = m_state.records().value(id).toArray();
        if (a.size() != 1 || a[0].toObject().value("deleted").toBool())
            return;
        auto d = a[0].toObject().value("data").toObject();
        if (d.value("kind") != "password" || d.value("origin") != origin)
            return;
        callback(d.value("username").toString(), d.value("password").toString());
    });
}
void MobileVault::savePassword(const QString &origin, const QString &username, const QString &password) {
    const QUrl url(origin);
    if (url.scheme() != "https" || !url.userInfo().isEmpty() || url.host().isEmpty() || password.isEmpty() ||
        password.size() > 4096)
        return;
    authenticated([this, origin, username, password] {
        QString error;
        QJsonObject data{
            {"kind", "password"}, {"origin", origin}, {"username", username}, {"password", password}};
        if (!m_state.update(ArbiterSync::passwordId(origin, username), m_document.value("device").toString(),
                            data, false, &error) ||
            !persist())
            fail(tr("Could not save password."));
        else {
            emit changed();
            emit completed(true);
        }
    });
}
bool MobileVault::localEndpoint(const QUrl &url) {
    if (url.scheme() != "https" || !url.userInfo().isEmpty() || url.hasQuery() || url.hasFragment() ||
        (!url.path().isEmpty() && url.path() != "/"))
        return false;
    const auto host = url.host().toLower();
    if (host == "localhost" || host.endsWith(".local"))
        return true;
    const QHostAddress address(host);
    return address.isLoopback() || address.isInSubnet(QHostAddress("10.0.0.0"), 8) ||
           address.isInSubnet(QHostAddress("172.16.0.0"), 12) ||
           address.isInSubnet(QHostAddress("192.168.0.0"), 16) ||
           address.isInSubnet(QHostAddress("fc00::"), 7) || address.isInSubnet(QHostAddress("fe80::"), 10);
}
QJsonObject MobileVault::pairingInfo(const QString &invitation) {
    if (invitation.size() > 16000)
        return {};
    QUrl url(invitation.trimmed());
    if (url.scheme() != "arbiter-pair" || url.host() != "v1")
        return {};
    auto data = object(decode(QUrlQuery(url).queryItemValue("data")));
    if (data.value("v") != 1 || !localEndpoint(QUrl(data.value("endpoint").toString())) ||
        decode(data.value("key").toString()).size() != 32 ||
        decode(data.value("code").toString()).size() != 32 ||
        !QRegularExpression("^[0-9a-f]{64}$").match(data.value("pin").toString()).hasMatch() ||
        !QRegularExpression("^[a-zA-Z0-9-]{16,80}$").match(data.value("vaultId").toString()).hasMatch())
        return {};
    return data;
}
void MobileVault::pair(const QString &invitation) {
    auto p = pairingInfo(invitation);
    if (p.isEmpty()) {
        fail(tr("Invalid pairing code"));
        return;
    }
    authenticated([this, p] {
        m_pairing = p;
        emit pairingReview(p.value("name").toString(), p.value("pin").toString());
    });
}
void MobileVault::confirmPair() {
    if (m_pairing.isEmpty() || !unlocked())
        return;
    const auto p = m_pairing;
    m_pairing = {};
    exchange("/v1/pair", p, decode(p.value("code").toString()),
             {{"deviceId", m_document.value("device")}, {"name", QSysInfo::machineHostName()}},
             [this, p](QJsonObject response) {
                 if (decode(response.value("token").toString()).size() != 32) {
                     fail(tr("Invalid device authorization"));
                     return;
                 }
                 auto peer = p;
                 peer.remove("code");
                 peer["token"] = response.value("token");
                 m_document["peer"] = peer;
                 if (!persist(true)) {
                     fail(tr("Could not update the vault."));
                     return;
                 }
                 sync();
             });
}
void MobileVault::exchange(const QString &path, const QJsonObject &peer, const QByteArray &token,
                           const QJsonObject &payload, std::function<void(QJsonObject)> callback) {
    const QUrl endpoint(peer.value("endpoint").toString());
    auto key = decode(peer.value("key").toString());
    if (!localEndpoint(endpoint) || key.size() != 32) {
        fail(tr("Pair with your desktop first"));
        return;
    }
    const auto context = "ArbiterSync/v1/" + peer.value("vaultId").toString().toUtf8();
    auto bytes = QJsonDocument(payload).toJson(QJsonDocument::Compact);
    auto sealed = MobilePlatform::seal(bytes, key, context);
    bytes.fill(0);
    if (sealed.isEmpty()) {
        fail(tr("Could not encrypt sync data."));
        return;
    }
    auto url = endpoint;
    url.setPath(path);
    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    request.setTransferTimeout(25000);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/octet-stream");
    request.setRawHeader("Authorization", "Bearer " + encode(token).toLatin1());
    // Never send a credential payload until TLS has authenticated the pinned peer.
    auto manager = new QNetworkAccessManager(this);
    auto reply = manager->post(request, sealed);
    connect(reply, &QNetworkReply::finished, manager, &QObject::deleteLater);
    const auto epoch = m_epoch;
    const auto pin = peer.value("pin").toString().toLatin1();
    auto pinned = std::make_shared<bool>(false);
    connect(reply, &QNetworkReply::sslErrors, reply, [reply, pin, pinned](const QList<QSslError> &) {
        auto cert = reply->sslConfiguration().peerCertificate();
        if (!cert.isNull() && cert.digest(QCryptographicHash::Sha256).toHex() == pin) {
            *pinned = true;
            reply->ignoreSslErrors();
        } else
            reply->abort();
    });
    connect(reply, &QNetworkReply::encrypted, reply, [reply, pin, pinned] {
        if (reply->sslConfiguration().peerCertificate().digest(QCryptographicHash::Sha256).toHex() != pin)
            reply->abort();
        else
            *pinned = true;
    });
    auto received = std::make_shared<QByteArray>();
    connect(reply, &QIODevice::readyRead, reply, [reply, received] {
        received->append(reply->readAll());
        if (received->size() > ArbiterSync::MaximumBytes + 28)
            reply->abort();
    });
    m_busy = true;
    m_error.clear();
    emit changed();
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, received, key, context, callback = std::move(callback), epoch, pinned]() mutable {
                reply->deleteLater();
                if (epoch != m_epoch) {
                    key.fill(0);
                    return;
                }
                m_busy = false;
                received->append(reply->readAll());
                bool ok = false;
                auto clear = (*pinned && reply->error() == QNetworkReply::NoError &&
                              reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 200)
                                 ? MobilePlatform::open(*received, key, context, &ok)
                                 : QByteArray();
                key.fill(0);
                auto result = object(clear);
                clear.fill(0);
                if (!ok || result.isEmpty()) {
                    fail(tr("Could not sync. Keep your desktop open and unlocked on the same Wi-Fi."));
                    return;
                }
                callback(result);
            });
}
void MobileVault::cancel() {
    ++m_epoch;
    for (auto reply : findChildren<QNetworkReply *>())
        reply->abort();
    m_busy = false;
    emit completed(false);
}
void MobileVault::sync() {
    if (m_busy)
        return;
    if (m_document.isEmpty() && !load(false))
        return;
    const auto peer = m_document.value("peer").toObject();
    if (peer.isEmpty())
        return;
    exchange("/v1/sync", peer, decode(peer.value("token").toString()), m_state.json(),
             [this, peer](QJsonObject data) {
                 ArbiterSync::State remote;
                 QString error;
                 if (!ArbiterSync::State::parse(data, &remote, &error) ||
                     !transaction(
                         [&](QJsonObject &doc, ArbiterSync::State &state) {
                             if (doc.value("peer").toObject().value("token") != peer.value("token"))
                                 return false;
                             return state.merge(remote, &error);
                         },
                         false)) {
                     fail(tr("Could not merge sync data."));
                     return;
                 }
                 emit changed();
                 emit completed(true);
             });
}
void MobileVault::backgroundSync(std::function<void(bool)> done) {
    auto connection = std::make_shared<QMetaObject::Connection>();
    *connection =
        connect(this, &MobileVault::completed, this, [connection, done = std::move(done)](bool success) {
            QObject::disconnect(*connection);
            done(success);
        });
    if (!load(false) || m_document.value("peer").toObject().isEmpty()) {
        emit completed(true);
        return;
    }
    sync();
}
void MobileVault::disconnectPeer() {
    ++m_epoch;
    m_document.remove("peer");
    if (!persist(true))
        fail(tr("Could not update the vault."));
    else
        emit changed();
}
QString MobileVault::generatePassword(int length, bool symbols) {
    if (length < 16 || length > 64)
        return {};
    QList<QString> groups = {"abcdefghijkmnopqrstuvwxyz", "ABCDEFGHJKLMNPQRSTUVWXYZ", "23456789"};
    if (symbols)
        groups.append("!@#$%&*+-=?");
    QString result, alphabet;
    for (const auto &g : groups) {
        alphabet += g;
        result += g.at(QRandomGenerator::system()->bounded(g.size()));
    }
    while (result.size() < length)
        result += alphabet.at(QRandomGenerator::system()->bounded(alphabet.size()));
    for (int i = result.size() - 1; i > 0; --i) {
        const auto j = QRandomGenerator::system()->bounded(i + 1);
        const auto a = result.at(i);
        result[i] = result.at(j);
        result[j] = a;
    }
    return result;
}
