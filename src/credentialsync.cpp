#include "credentialsync.h"
#include "passwordmanager.h"
#include "passkeyvault.h"
#include "passkeycrypto.h"
#include "qrcodegen.hpp"
#include "settings.h"
#include <QBuffer>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkInterface>
#include <QSaveFile>
#include <QStandardPaths>
#include <QSslCertificate>
#include <QSysInfo>
#include <QUuid>
#include <openssl/crypto.h>

namespace {
QString encode(const QByteArray &bytes) { return QString::fromLatin1(bytes.toBase64(QByteArray::Base64UrlEncoding|QByteArray::OmitTrailingEquals)); }
QByteArray decode(const QJsonValue &value) { return QByteArray::fromBase64(value.toString().toLatin1(),QByteArray::Base64UrlEncoding); }
QString identifier(const QJsonObject &data) {
    if(data.value("kind")=="password") return ArbiterSync::passwordId(data.value("origin").toString(),data.value("username").toString());
    if(data.value("kind")=="passkey") return ArbiterSync::passkeyId(data.value("rpId").toString(),decode(data.value("credentialId")));
    return {};
}
bool equalToken(const QByteArray &a,const QByteArray &b) { return a.size()==32 && b.size()==32 && CRYPTO_memcmp(a.constData(),b.constData(),32)==0; }
QString uuid() { return QUuid::createUuid().toString(QUuid::WithoutBraces); }
}
CredentialSync::CredentialSync(PasswordManager *passwords,PasskeyVault *passkeys,QObject *parent)
    :CredentialSync(passwords,passkeys,QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)+QStringLiteral("/sync"),nullptr,{},parent) {
    m_persistSettings=true;setKeepUnlocked(Settings().value("Sync/keepUnlocked",false).toBool());
}
CredentialSync::CredentialSync(PasswordManager *passwords,PasskeyVault *passkeys,const QString &directory,
                               SecretStore *store,const QString &loopbackAddress,QObject *parent)
    :QObject(parent),m_passwords(passwords),m_passkeys(passkeys),m_defaultStore(this,QStringLiteral("Arbiter Sync")),
     m_store(store ? store : &m_defaultStore),m_loopbackAddress(loopbackAddress),m_server(this),m_directory(directory) {
    m_pairExpiry.setSingleShot(true); m_pairExpiry.setInterval(120000);
    connect(&m_pairExpiry,&QTimer::timeout,this,&CredentialSync::cancelPairing);
    connect(passkeys,&PasskeyVault::syncLocked,this,&CredentialSync::lock);
    connect(passkeys,&PasskeyVault::changed,this,[this] {
        if(m_key.present() && !m_passkeysReady()) lock();
        emit changed();
    });
}
bool CredentialSync::m_passkeysReady() const { return m_passkeys->syncReady(); }
QVariantList CredentialSync::devices() const {
    QVariantList out; const auto peers=m_document.value("peers").toObject();
    for(auto i=peers.begin();i!=peers.end();++i) out.append(QVariantMap{{"id",i.key()},{"name",i.value().toObject().value("name").toString()}});
    return out;
}
void CredentialSync::setKeepUnlocked(bool enabled) {
    if(m_keepUnlocked==enabled) {m_passkeys->setSyncStayUnlocked(enabled);return;}
    m_keepUnlocked=enabled;
    if(m_persistSettings) Settings().setValue("Sync/keepUnlocked",enabled);
    m_passkeys->setSyncStayUnlocked(enabled);emit changed();
}
void CredentialSync::finish(const QString &error) { m_busy=false; m_error=error; emit changed(); }
void CredentialSync::unlock(const QString &phrase,bool biometric) {
    if(m_busy) return;
    m_busy=true; m_error.clear(); emit changed(); const auto epoch=++m_epoch;
    m_passkeys->unlockForSync(phrase,biometric,protect([this,epoch](const QString &error,const QJsonArray &) {
        if(epoch!=m_epoch) return;
        if(!error.isEmpty()) { finish(error); return; }
        loadIdentity(protect([this,epoch](const QString &error) {
            if(epoch!=m_epoch) return;
            if(!error.isEmpty()) { lock(); finish(error); return; }
            refresh(protect([this,epoch](const QString &error) {
                if(epoch!=m_epoch) return;
                if(!error.isEmpty()) { lock(); finish(error); return; }
                finish();
            }));
        }));
    }));
}
void CredentialSync::lock() {
    if(m_locking) return;
    m_locking=true;
    ++m_epoch; m_busy=false; cancelPairing(); m_key.clear(); m_server.stop();
    m_state={}; m_document={}; m_current={}; m_address.clear(); m_passkeys->lockSync(); m_locking=false; emit changed();
}
void CredentialSync::loadIdentity(Done callback) {
    if(!QDir().mkpath(m_directory)) { callback(tr("Could not create the encrypted sync directory.")); return; }
    QFile::setPermissions(m_directory,QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner);
    m_store->readOptional(QStringLiteral("sync-v1-master"),protect([this,callback](const QString &error,const QByteArray &stored) {
        if(!error.isEmpty()) { callback(error); return; }
        const bool exists=QFile::exists(m_directory+"/state-v1.enc");
        if(stored.isEmpty()) {
            if(exists) { callback(tr("The encrypted sync state exists, but its OS vault key is missing. It was not overwritten.")); return; }
            const auto key=PasskeyCrypto::random(32);
            m_store->write(QStringLiteral("sync-v1-master"),key,protect([this,key,callback](const QString &error,const QByteArray &) {
                if(!error.isEmpty() || !m_key.set(key)) { callback(error.isEmpty() ? tr("Could not protect the sync key in memory.") : error); return; }
                loadIdentity(callback);
            })); return;
        }
        if(stored.size()!=32 || !m_key.set(stored)) { callback(tr("The stored sync key is invalid or unavailable.")); return; }
        if(exists) {
            QFile file(m_directory+"/state-v1.enc"); bool ok=false;
            if(!file.open(QIODevice::ReadOnly) || file.size()>ArbiterSync::MaximumBytes+28) { callback(tr("Could not read encrypted sync state.")); return; }
            auto plain=ArbiterSync::open(file.readAll(),stored,"ArbiterDesktopSync/v1",&ok);
            QJsonParseError parse; const auto document=QJsonDocument::fromJson(plain,&parse); PasskeyCrypto::wipe(plain);
            QString stateError;
            if(!ok || parse.error!=QJsonParseError::NoError || !document.isObject() || document.object().value("v")!=1
                || !ArbiterSync::State::parse(document.object().value("state").toObject(),&m_state,&stateError)) {
                callback(tr("Encrypted sync state could not be authenticated. It was not overwritten.")); return;
            }
            m_document=document.object();
            if(decode(m_document.value("transportKey")).size()!=32) { callback(tr("The encrypted sync transport key is missing or invalid.")); return; }
        } else {
            m_document={{"v",1},{"deviceId",uuid()},{"vaultId",uuid()},{"name",QSysInfo::machineHostName().left(80)},
                {"peers",QJsonObject{}},{"baseline",QJsonObject{}},{"transportKey",encode(PasskeyCrypto::random(32))},{"port",0}};
            m_state={};
        }
        m_store->readOptional(QStringLiteral("sync-v1-tls"),protect([this,callback](const QString &error,const QByteArray &privateKey) {
            if(!error.isEmpty()) { callback(error); return; }
            QFile cert(m_directory+"/certificate.pem");
            if(privateKey.isEmpty()) {
                if(cert.exists()) { callback(tr("The pinned sync certificate lost its private key. Existing pairing was preserved.")); return; }
                QByteArray certificate,key;
                if(!CredentialSyncServer::createIdentity(&certificate,&key)) { callback(tr("Could not create a local TLS identity.")); return; }
                m_store->write(QStringLiteral("sync-v1-tls"),key,protect([this,certificate,callback](const QString &error,const QByteArray &) {
                    if(!error.isEmpty()) { callback(error); return; }
                    QSaveFile file(m_directory+"/certificate.pem");
                    if(!file.open(QIODevice::WriteOnly) || file.write(certificate)!=certificate.size() || !file.commit()) { callback(tr("Could not save the public sync certificate.")); return; }
                    loadIdentity(callback);
                })); PasskeyCrypto::wipe(key); return;
            }
            if(!cert.open(QIODevice::ReadOnly) || cert.size()>16384) { callback(tr("Could not load the pinned sync certificate.")); return; }
            m_certificate=cert.readAll();
            if(!m_server.start(m_certificate,privateKey,quint16(m_document.value("port").toInt()),
                protect([this](const QString &path,const QByteArray &token,const QByteArray &body,CredentialSyncServer::Reply reply) { request(path,token,body,std::move(reply)); }))) {
                callback(tr("Could not listen for local sync: %1").arg(m_server.error())); return;
            }
            m_document.insert("port",m_server.port());
            QStringList addresses;
            for(const auto &address:QNetworkInterface::allAddresses()) if(address.protocol()==QAbstractSocket::IPv4Protocol && !address.isLoopback() && CredentialSyncServer::localAddress(address)) addresses.append(address.toString());
            if(!m_loopbackAddress.isEmpty() && QHostAddress(m_loopbackAddress).isLoopback()) addresses.prepend(m_loopbackAddress);
            if(addresses.isEmpty()) { callback(tr("Connect this computer to a local Wi-Fi or Ethernet network before pairing.")); return; }
            std::sort(addresses.begin(),addresses.end(),[](const QString &a,const QString &b) { return a.startsWith("192.168.")!=b.startsWith("192.168.") ? a.startsWith("192.168.") : a<b; });
            if(!m_loopbackAddress.isEmpty() && QHostAddress(m_loopbackAddress).isLoopback()) addresses.prepend(m_loopbackAddress);
            m_address="https://"+addresses.first()+":"+QString::number(m_server.port());
            callback({});
        }));
    }));
}
bool CredentialSync::save() {
    m_document.insert("state",m_state.json());
    auto plain=QJsonDocument(m_document).toJson(QJsonDocument::Compact),key=m_key.copy();
    const auto sealed=ArbiterSync::seal(plain,key,"ArbiterDesktopSync/v1"); PasskeyCrypto::wipe(plain); PasskeyCrypto::wipe(key);
    if(sealed.isEmpty()) return false;
    QSaveFile file(m_directory+"/state-v1.enc");
    return file.open(QIODevice::WriteOnly) && file.setPermissions(QFile::ReadOwner|QFile::WriteOwner) && file.write(sealed)==sealed.size() && file.commit();
}
void CredentialSync::refresh(Done callback) {
    m_passkeys->exportForSync(protect([this,callback](const QString &error,const QJsonArray &keys) {
        if(!error.isEmpty()) { callback(error); return; }
        m_passwords->exportAfterVerification(protect([this,keys,callback](const QString &error,const QJsonArray &passwords) {
            if(!error.isEmpty()) { callback(error); return; }
            QJsonObject current;
            for(const auto &array: {keys,passwords}) for(const auto &value:array) { const auto data=value.toObject(); current.insert(identifier(data),data); }
            const auto baseline=m_document.value("baseline").toObject(),records=m_state.records();
            QString stateError; auto candidate=m_state;
            const auto device=m_document.value("deviceId").toString();
            for(auto i=current.begin();i!=current.end();++i) if(baseline.value(i.key())!=i.value()) {
                const auto versions=records.value(i.key()).toArray();
                if(versions.size()==1 && !versions.first().toObject().value("deleted").toBool() && versions.first().toObject().value("data")==i.value()) continue;
                if(!candidate.update(i.key(),device,i.value().toObject(),false,&stateError)) { callback(stateError); return; }
            }
            for(auto i=baseline.begin();i!=baseline.end();++i) if(!current.contains(i.key())) {
                const auto versions=records.value(i.key()).toArray();
                if(versions.size()==1 && versions.first().toObject().value("deleted").toBool()) continue;
                if(!candidate.update(i.key(),device,{},true,&stateError)) { callback(stateError); return; }
            }
            const auto previous=m_state; const auto oldDocument=m_document; m_state=candidate; m_document.insert("baseline",current);
            if(!save()) { m_state=previous; m_document=oldDocument; callback(tr("Could not save encrypted sync changes.")); return; }
            m_current=current; callback({});
        }));
    }));
}
QByteArray CredentialSync::payload(const QJsonObject &data) const {
    auto key=decode(m_document.value("transportKey")); const auto bytes=ArbiterSync::seal(QJsonDocument(data).toJson(QJsonDocument::Compact),key,"ArbiterSync/v1/"+m_document.value("vaultId").toString().toUtf8());
    PasskeyCrypto::wipe(key); return bytes;
}
void CredentialSync::cancelPairing() { m_pairExpiry.stop(); m_pairCode.clear(); m_code.clear(); m_qr.clear(); emit changed(); }
void CredentialSync::pair() {
    if(!unlocked() || m_busy) return;
    m_pairCode=PasskeyCrypto::random(32); m_pairExpiry.start();
    const auto pin=QString::fromLatin1(QSslCertificate(m_certificate).digest(QCryptographicHash::Sha256).toHex());
    auto key=decode(m_document.value("transportKey"));
    const QJsonObject info{{"v",1},{"endpoint",m_address},{"pin",pin},{"key",encode(key)},
        {"vaultId",m_document.value("vaultId")},{"code",encode(m_pairCode)},{"name",m_document.value("name")}};
    PasskeyCrypto::wipe(key);
    m_code="arbiter-pair://v1?data="+encode(QJsonDocument(info).toJson(QJsonDocument::Compact));
    try {
        const auto qr=qrcodegen::QrCode::encodeText(m_code.toUtf8().constData(),qrcodegen::QrCode::Ecc::MEDIUM);
        const int scale=5,quiet=4; QImage image((qr.getSize()+quiet*2)*scale,(qr.getSize()+quiet*2)*scale,QImage::Format_RGB32); image.fill(Qt::white);
        for(int y=0;y<qr.getSize();++y) for(int x=0;x<qr.getSize();++x) if(qr.getModule(x,y)) for(int yy=0;yy<scale;++yy) for(int xx=0;xx<scale;++xx) image.setPixel((x+quiet)*scale+xx,(y+quiet)*scale+yy,qRgb(0,0,0));
        QByteArray png; QBuffer buffer(&png); buffer.open(QIODevice::WriteOnly); image.save(&buffer,"PNG"); m_qr="data:image/png;base64,"+QString::fromLatin1(png.toBase64());
    } catch(...) { cancelPairing(); m_error=tr("Could not render the pairing code."); }
    emit changed();
}
void CredentialSync::revoke(const QString &id) {
    if(!unlocked() || m_busy) return;
    auto peers=m_document.value("peers").toObject(),old=peers; peers.remove(id); m_document.insert("peers",peers);
    if(!save()) { m_document.insert("peers",old); m_error=tr("Could not revoke the device. Try again."); }
    emit changed();
}
void CredentialSync::request(const QString &path,const QByteArray &token,const QByteArray &body,CredentialSyncServer::Reply reply) {
    if(!unlocked() || m_busy) { reply(409,{}); return; }
    bool authorized=false; const auto provided=QByteArray::fromBase64(token,QByteArray::Base64UrlEncoding);
    if(path=="/v1/pair") authorized=m_pairExpiry.isActive() && equalToken(provided,m_pairCode);
    else for(const auto &peer:m_document.value("peers").toObject()) if(equalToken(provided,decode(peer.toObject().value("token")))) authorized=true;
    if(!authorized) { reply(401,{}); return; }
    bool authenticated=false; auto key=decode(m_document.value("transportKey"));
    auto plain=ArbiterSync::open(body,key,"ArbiterSync/v1/"+m_document.value("vaultId").toString().toUtf8(),&authenticated); PasskeyCrypto::wipe(key);
    QJsonParseError parse; const auto data=QJsonDocument::fromJson(plain,&parse); PasskeyCrypto::wipe(plain);
    if(!authenticated || parse.error!=QJsonParseError::NoError || !data.isObject()) { reply(400,{}); return; }
    if(path=="/v1/pair") {
        const auto id=data.object().value("deviceId").toString(),name=data.object().value("name").toString();
        if(QUuid(id).isNull() || name.isEmpty() || name.size()>80) { reply(400,{}); return; }
        auto peers=m_document.value("peers").toObject();
        if(peers.size()>=16 && !peers.contains(id)) { reply(409,{}); return; }
        const auto token=PasskeyCrypto::random(32); const auto old=peers;
        peers.insert(id,QJsonObject{{"name",name},{"token",encode(token)}}); m_document.insert("peers",peers);
        if(!save()) { m_document.insert("peers",old); reply(500,{}); return; }
        const auto response=payload({{"token",encode(token)}}); cancelPairing(); reply(200,response); return;
    }
    ArbiterSync::State incoming; QString stateError;
    if(!ArbiterSync::State::parse(data.object(),&incoming,&stateError)) { reply(400,{}); return; }
    const auto records=incoming.records();
    for(auto i=records.begin();i!=records.end();++i) for(const auto &version:i.value().toArray()) {
        const auto v=version.toObject(); if(!v.value("deleted").toBool() && identifier(v.value("data").toObject())!=i.key()) { reply(400,{}); return; }
    }
    m_busy=true; emit changed(); const auto epoch=m_epoch;
    refresh(protect([this,incoming,reply,epoch](const QString &error) {
        if(epoch!=m_epoch) { reply(409,{}); return; }
        if(!error.isEmpty()) { finish(error); reply(409,{}); return; }
        auto merged=m_state; QString mergeError;
        if(!merged.merge(incoming,&mergeError)) { finish(mergeError); reply(400,{}); return; }
        // An incoming copy acknowledges a real replica on the phone. Keep BE
        // immutable, and set BS only for a credential actually held by a peer.
        const auto mergedRecords=merged.records();
        for(auto i=mergedRecords.begin();i!=mergedRecords.end();++i) {
            const auto versions=i.value().toArray(),remote=incoming.records().value(i.key()).toArray();
            if(versions.size()!=1 || remote.size()!=1) continue;
            const auto version=versions.first().toObject(),peer=remote.first().toObject();
            auto credential=version.value("data").toObject();
            if(version.value("deleted").toBool() || peer.value("deleted").toBool() || credential.value("kind")!="passkey"
                || !credential.value("backupEligible").toBool() || credential.value("backedUp").toBool()
                || peer.value("data").toObject().value("privateKey")!=credential.value("privateKey")) continue;
            credential.insert("backedUp",true);
            if(!merged.update(i.key(),m_document.value("deviceId").toString(),credential,false,&mergeError)) { finish(mergeError); reply(400,{}); return; }
        }
        const auto previous=m_state; m_state=merged;
        if(!save()) { m_state=previous; finish(tr("Could not persist incoming encrypted changes.")); reply(500,{}); return; }
        apply(protect([this,reply,epoch](const QString &error) {
            if(epoch!=m_epoch || !unlocked()) { finish(); reply(409,{}); return; }
            if(!error.isEmpty()) { finish(error); reply(409,{}); return; }
            refresh(protect([this,reply,epoch](const QString &error) {
                if(epoch!=m_epoch || !unlocked()) { finish(); reply(409,{}); return; }
                if(!error.isEmpty()) { finish(error); reply(409,{}); return; }
                const auto response=payload(m_state.json()); finish(); reply(200,response);
            }));
        }));
    }));
}
void CredentialSync::apply(Done callback) {
    QStringList ids; const auto records=m_state.records();
    for(auto i=records.begin();i!=records.end();++i) if(i.value().toArray().size()==1) ids.append(i.key());
    auto step=std::make_shared<std::function<void(int)>>(); const std::weak_ptr<std::function<void(int)>> weak=step;
    *step=protect([this,ids,records,weak,callback](int index) {
        if(!unlocked()) { callback(tr("The sync session expired.")); return; }
        if(index==ids.size()) { callback({}); return; }
        const auto id=ids[index]; const auto version=records.value(id).toArray().first().toObject();
        const bool deleted=version.value("deleted").toBool(); const auto data=deleted ? m_current.value(id).toObject() : version.value("data").toObject();
        const auto next=weak.lock();
        auto done=protect([this,next,index,callback](const QString &error) { if(!error.isEmpty()) callback(error); else QTimer::singleShot(0,this,[next,index] { (*next)(index+1); }); });
        if(data.isEmpty() || (!deleted && m_current.value(id)==data)) { done(QString()); return; }
        if(data.value("kind")=="password") {
            auto completed=protect([done](bool,const QString &error) { done(error); });
            if(deleted) m_passwords->removeFromSync(data.value("origin").toString(),data.value("username").toString(),completed);
            else m_passwords->upsertFromSync(data,completed);
        } else if(data.value("kind")=="passkey") {
            if(deleted) m_passkeys->removeFromSync(decode(data.value("credentialId")),done);
            else m_passkeys->upsertFromSync(data,done);
        } else done(tr("Unsupported credential type."));
    });
    (*step)(0);
}
