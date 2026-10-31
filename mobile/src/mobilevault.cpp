#include "mobilevault.h"
#include "mobileplatform.h"
#include "credentialsyncpeer.h"
#include <QDateTime>
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
    // Preserve existing one-computer pairings while upgrading to a connection map.
    if (document.value("peer").isObject()) {
        auto peers = document.value("peers").toObject();
        const auto peer = document.value("peer").toObject();
        peers.insert(peer.value("vaultId").toString(), peer);
        document["peers"] = peers;
        document.remove("peer");
    }
    state.rememberDevice(document.value("device").toString(),QSysInfo::machineHostName().left(80));
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
bool MobileVault::persist() {
    const auto desired = m_state;
    return transaction([&](QJsonObject &, ArbiterSync::State &state) {
        QString error; return state.merge(desired, &error);
    }, true);
}
QVariantList MobileVault::devices() const {
    QVariantList result;
    const auto peers = m_document.value("peers").toObject();
    for (auto i = peers.begin(); i != peers.end(); ++i) {
        const auto p = i.value().toObject();
        result.append(QVariantMap{{"id", i.key()}, {"name", p.value("name").toString()},
            {"lastSeen", p.value("lastSeen").toDouble()}, {"pending", p.value("ack").toString()!=m_state.digest()}});
    }
    return result;
}
QString MobileVault::peerName() const {
    QStringList names;
    for (const auto &peer : devices()) names.append(peer.toMap().value("name").toString());
    return names.join(", ");
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
    if (!unlocked()) return {};
    QVariantList choices; QMap<QString,QString> names;
    const auto directory=m_state.deviceNames();
    for(auto i=directory.begin();i!=directory.end();++i) names[i.key()]=i.value().toString();
    names[m_document.value("device").toString()]=tr("This device");
    for(const auto &v:m_document.value("peers").toObject()) {
        const auto p=v.toObject(); names[p.value("deviceId").toString()]=p.value("name").toString();
    }
    for(const auto &v:m_state.conflictChoices()) {
        auto choice=v.toObject(); const auto id=choice.value("device").toString();
        choice["device"]=names.value(id,id); choices.append(choice.toVariantMap());
    }
    return choices;
}
void MobileVault::resolveConflict(const QString &id, const QString &version, const QString &generation) {
    if (m_busy) return;
    authenticated([this, id, version, generation] {
        QString error;
        if (!transaction([&](QJsonObject &doc, ArbiterSync::State &state) {
            return state.resolve(id, version, generation, doc.value("device").toString(), &error);
        }, false)) fail(tr("Credential changes were updated. Unlock the vault and review them again."));
        else { emit changed(); emit completed(true); }
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
bool MobileVault::localEndpoint(const QUrl &url) { return ArbiterSync::localEndpoint(url); }
QJsonObject MobileVault::pairingInfo(const QString &invitation) { return ArbiterSync::pairingInfo(invitation); }
void MobileVault::pair(const QString &invitation) {
    if (m_busy) return;
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
    if (m_busy || m_pairing.isEmpty() || !unlocked())
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
                 peer["deviceId"] = response.value("deviceId");
                 if (!transaction([&](QJsonObject &doc, ArbiterSync::State &) {
                     auto peers = doc.value("peers").toObject();
                     if (peers.size() >= 16 && !peers.contains(peer.value("vaultId").toString())) return false;
                     peers[peer.value("vaultId").toString()] = peer;
                     doc["peers"] = peers; return true;
                 }, true)) {
                     fail(tr("Could not update the vault."));
                     return;
                 }
                 sync();
             });
}
void MobileVault::exchange(const QString &path, const QJsonObject &peer, const QByteArray &token,
                           const QJsonObject &payload, std::function<void(QJsonObject)> callback) {
    m_busy = true; m_error.clear(); emit changed();
    const auto epoch = m_epoch;
    ArbiterSync::exchange(this, path, peer, token, payload, MobilePlatform::seal, MobilePlatform::open,
        [this, epoch, callback = std::move(callback)](QString error, QJsonObject response) {
            if (epoch != m_epoch) return;
            m_busy = false;
            if (!error.isEmpty()) { fail(error); return; }
            callback(response);
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
    if (m_busy) return;
    if (!load(false)) { emit completed(false); return; }
    m_busy = true; m_error.clear(); emit changed();
    syncNext(m_document.value("peers").toObject().keys(), {});
}
void MobileVault::syncNext(QStringList peers, QStringList errors) {
    if (peers.isEmpty()) {
        m_busy = false; m_error = errors.join("\n"); emit changed(); emit completed(errors.isEmpty()); return;
    }
    const auto id = peers.takeFirst();
    const auto peer = m_document.value("peers").toObject().value(id).toObject();
    const auto epoch = m_epoch;
    ArbiterSync::exchange(this, "/v1/sync", peer, decode(peer.value("token").toString()), m_state.json(),
        MobilePlatform::seal, MobilePlatform::open,
        [this, epoch, id, peer, peers, errors](QString error, QJsonObject response) mutable {
            if (epoch != m_epoch) return;
            if (error.isEmpty()) {
                ArbiterSync::State remote;
                if (!ArbiterSync::State::parse(response, &remote, &error) || !transaction(
                    [&](QJsonObject &doc, ArbiterSync::State &state) {
                        auto contacts = doc.value("peers").toObject();
                        auto contact = contacts.value(id).toObject();
                        if (contact.value("token") != peer.value("token") || !state.merge(remote, &error)) return false;
                        contact["lastSeen"] = QDateTime::currentMSecsSinceEpoch(); contact["ack"] = remote.digest(); contacts[id] = contact;
                        doc["peers"] = contacts; return true;
                    }, false)) error = tr("Could not merge sync data.");
            }
            if (!error.isEmpty()) errors.append(peer.value("name").toString() + ": " + error);
            syncNext(peers, errors);
        });
}
void MobileVault::backgroundSync(std::function<void(bool)> done) {
    auto connection = std::make_shared<QMetaObject::Connection>();
    *connection =
        connect(this, &MobileVault::completed, this, [connection, done = std::move(done)](bool success) {
            QObject::disconnect(*connection);
            done(success);
        });
    if (!load(false) || m_document.value("peers").toObject().isEmpty()) {
        emit completed(true);
        return;
    }
    sync();
}
void MobileVault::disconnectPeer(const QString &id) {
    if (m_busy) return;
    authenticated([this, id] {
        if (!transaction([&](QJsonObject &doc, ArbiterSync::State &) {
            auto peers = doc.value("peers").toObject();
            if (id.isEmpty()) peers = {}; else peers.remove(id);
            doc["peers"] = peers; return true;
        }, false)) fail(tr("Could not update the vault."));
        else emit changed();
    });
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
