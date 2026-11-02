#include "passkeyvault.h"
#include "passkeycrypto.h"
#include <QCborArray>
#include <QCborValue>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QUuid>
#include <QGuiApplication>

namespace {
const auto ownerOnly = QFileDevice::ReadOwner | QFileDevice::WriteOwner;
// Response maps are inserted in CTAP canonical order. Qt's SortKeysInMaps flag
// does not sort maps in the supported Qt kit, so do not rely on that flag.
QByteArray encoded(const QCborMap &map) { return QCborValue(map).toCbor(); }
QByteArray descriptorId(const QCborValue &value) { return value.toMap().value(QStringLiteral("id")).toByteArray(); }
bool contains(const QCborArray &list, const QByteArray &id)
{
    for (const auto &entry : list) if (descriptorId(entry) == id) return true;
    return false;
}
}
PasskeyVault::PasskeyVault(SecretStore *store, const QString &directory, QObject *parent, BiometricAuth *biometrics)
    : QObject(parent), m_store(store), m_directory(directory), m_biometrics(biometrics) {
    load();
    connect(QCoreApplication::instance(),&QCoreApplication::aboutToQuit,this,&PasskeyVault::lockSync);
    if(m_biometrics) connect(m_biometrics,&BiometricAuth::changed,this,[this] {
        if(!biometricAvailable() || (!m_biometrics->busy() && m_biometrics->foregroundRequired()
            && QGuiApplication::applicationState()!=Qt::ApplicationActive)) lockBiometrics();
        emit changed();
    });
    if(m_biometrics) connect(m_biometrics,&BiometricAuth::sessionLocked,this,&PasskeyVault::lockBiometrics);
    if(auto *app=qobject_cast<QGuiApplication *>(QCoreApplication::instance())) {
        connect(app,&QGuiApplication::applicationStateChanged,this,[this](Qt::ApplicationState state) {
            // Native verification may temporarily take focus itself. Every
            // operation still requires a fresh native prompt and request guard.
            if(state!=Qt::ApplicationActive && !(m_biometrics && m_biometrics->busy() && m_biometrics->nativePromptCanTakeFocus())) lockBiometrics();
        });
        connect(app,&QCoreApplication::aboutToQuit,this,&PasskeyVault::lockBiometrics);
    }
}
PasskeyVault::~PasskeyVault() { lockSync(); lockBiometrics(); }
void PasskeyVault::cancelBiometric() { ++m_biometricEpoch; if(m_biometrics) m_biometrics->cancel(); }
void PasskeyVault::lockBiometrics() { cancelBiometric(); emit changed(); }

bool PasskeyVault::load()
{
    m_index.clear();
    QFile file(m_directory + QStringLiteral("/catalog.cbor"));
    if (!file.exists()) return true;
    if (!file.open(QIODevice::ReadOnly) || file.size() > 1024 * 1024) return false;
    QCborParserError error;
    auto root = QCborValue::fromCbor(file.readAll(), &error);
    if (error.error != QCborError::NoError || !root.isMap()) return false;
    const auto map = root.toMap();
    if (map.value(0).toInteger() != 2 || map.value(1).toByteArray().size() != 16
        || !map.value(2).isBool() || !map.value(3).isArray()) return false;
    QSet<QByteArray> ids;
    for (const auto &entry : map.value(3).toArray()) {
        const auto meta = entry.toMap();
        const auto id = meta.value(0).toByteArray();
        if (id.size() != 32 || ids.contains(id) || meta.value(1).toString().isEmpty()
            || meta.value(2).toByteArray().isEmpty() || meta.value(2).toByteArray().size() > 64
            || meta.value(3).toString().size() > 256) return false;
        ids.insert(id);
    }
    m_index = map;
    return true;
}
bool PasskeyVault::save()
{
    if (!QDir().mkpath(m_directory)) return false;
    QFile::setPermissions(m_directory, ownerOnly | QFileDevice::ExeOwner);
    QSaveFile file(m_directory + QStringLiteral("/catalog.cbor"));
    const auto data = encoded(m_index);
    return data.size() <= 1024 * 1024 && file.open(QIODevice::WriteOnly)
        && file.setPermissions(ownerOnly) && file.write(data) == data.size() && file.commit();
}
bool PasskeyVault::begin()
{
    if (m_busy || !m_store) return false;
    QDir().mkpath(m_directory);
    m_lock = std::make_unique<QLockFile>(m_directory + QStringLiteral("/vault.lock"));
    if (!m_lock->tryLock(0)) { finish(tr("The passkey vault is in use by another browser process.")); return false; }
    if (!load()) { finish(tr("The passkey catalog is damaged. Restore your profile before continuing.")); return false; }
    m_busy = true; m_error.clear(); emit changed();
    return true;
}
void PasskeyVault::finish(const QString &error)
{
    m_error = error; m_busy = false; m_lock.reset(); emit changed();
}
QString PasskeyVault::itemKey(const QByteArray &id) const
{
    return QStringLiteral("com.arbiter.browser/passkeys/v1/%1/%2")
        .arg(QString::fromLatin1(m_index.value(1).toByteArray().toHex()), QString::fromLatin1(id.toHex()));
}
QVariantList PasskeyVault::credentials(const QString &rp) const
{
    QVariantList result;
    if (!configured()) return result;
    for (const auto &entry : m_index.value(3).toArray()) {
        const auto meta = entry.toMap();
        if (meta.value(1).toString() != rp) continue;
        result.append(QVariantMap{{QStringLiteral("id"), meta.value(0).toByteArray()},
            {QStringLiteral("userId"), meta.value(2).toByteArray()}, {QStringLiteral("name"), meta.value(3).toString()}});
    }
    return result;
}
void PasskeyVault::perform(bool registration, const QByteArray &bytes, const QByteArray &credentialId,
                           Result callback, std::function<bool()> current)
{
    if (!begin()) { callback(1, {}); return; }
    if (bytes.size() > 65536) { finish(tr("Invalid authenticator request.")); callback(1, {}); return; }
    QCborParserError parseError;
    const auto value = QCborValue::fromCbor(bytes, &parseError);
    const auto request = value.toMap();
    const auto rp = registration ? request.value(2).toMap().value(QStringLiteral("id")).toString() : request.value(1).toString();
    const auto hash = request.value(registration ? 1 : 2).toByteArray();
    if (bytes.size() > 65536 || parseError.error != QCborError::NoError || !value.isMap() || rp.isEmpty()
        || rp.toUtf8().size() > 253 || hash.size() != 32) { finish(tr("Invalid authenticator request.")); callback(1, {}); return; }
    QPointer<PasskeyVault> self(this);
    {
        const auto epoch=m_biometricEpoch;
        const auto original=current;
        current=[self,epoch,original] {
            return self && self->m_biometricEpoch==epoch
                && (!self->m_biometrics || !self->m_biometrics->foregroundRequired() || QGuiApplication::applicationState()==Qt::ApplicationActive)
                && (!original || original());
        };
    }
    auto unlocked = [self, request, rp, hash, credentialId, registration, callback, current](QByteArray key) mutable {
        if (!self) { PasskeyCrypto::wipe(key); return; }
        auto done = [self, callback](int status, const QByteArray &response, const QString &error = {}) {
            if (self) self->finish(error);
            callback(status, response);
        };
        if (current && !current()) { PasskeyCrypto::wipe(key); done(1, {}); return; }
        if (key.isEmpty()) { done(1, {}, self->m_error.isEmpty() ? tr("Device verification was cancelled or failed. Try again.") : self->m_error); return; }
        const auto metadata = self->m_index.value(3).toArray();
        if (registration) {
            bool es256 = false;
            for (const auto &algorithm : request.value(4).toArray()) {
                const auto alg = algorithm.toMap();
                es256 |= alg.value(QStringLiteral("type")).toString() == QStringLiteral("public-key") && alg.value(QStringLiteral("alg")).toInteger() == -7;
            }
            if (!es256) { PasskeyCrypto::wipe(key); done(3, {}, tr("This website does not accept ES256 passkeys.")); return; }
            for (const auto &entry : metadata) {
                auto meta = entry.toMap();
                if (meta.value(1).toString() == rp && contains(request.value(5).toArray(), meta.value(0).toByteArray())) {
                    PasskeyCrypto::wipe(key); done(2, {}, tr("A passkey in the website's exclusion list already exists.")); return;
                }
            }
            const auto user = request.value(3).toMap();
            const auto userId = user.value(QStringLiteral("id")).toByteArray();
            if (userId.isEmpty() || userId.size() > 64) { PasskeyCrypto::wipe(key); done(1, {}, tr("Invalid passkey user ID.")); return; }
            auto pair = PasskeyCrypto::generate();
            const auto id = PasskeyCrypto::random(32);
            if (pair.der.isEmpty() || id.size() != 32) { PasskeyCrypto::wipe(key); done(1, {}, tr("Could not generate a passkey.")); return; }
            QCborMap meta;
            meta.insert(0, id); meta.insert(1, rp); meta.insert(2, userId);
            meta.insert(3, user.value(QStringLiteral("name")).toString().left(256));
            meta.insert(4,true); // New credentials can be replicated through encrypted sync.
            QCborMap record; record.insert(0, meta); record.insert(1, pair.der);
            auto plain = encoded(record);
            auto sealed = PasskeyCrypto::seal(plain, key, self->m_index.value(1).toByteArray() + id);
            PasskeyCrypto::wipe(key); PasskeyCrypto::wipe(plain); PasskeyCrypto::wipe(pair.der);
            if (sealed.isEmpty() || sealed.size() > 2560) { done(1, {}, tr("The passkey exceeds the secure storage limit.")); return; }
            auto next = metadata; next.append(meta); self->m_index.insert(3, next);
            if (!self->save()) { self->m_index.insert(3, metadata); done(1, {}, tr("Could not reserve the passkey catalog entry.")); return; }
            auto auth = PasskeyCrypto::authenticatorData(rp, true,meta.value(4).toBool(),meta.value(5).toBool());
            auth.append(QByteArray(16, char(0))); // zero AAGUID; no identifying attestation
            auth.append(char(0)); auth.append(char(id.size())); auth.append(id); auth.append(pair.cose);
            QCborMap response; response.insert(1, QStringLiteral("none")); response.insert(2, auth); response.insert(3, QCborMap());
            self->m_store->write(self->itemKey(id), sealed, [self, done, response, metadata, current, id](const QString &error, const QByteArray &) {
                if (!self) return;
                if (!error.isEmpty()) { self->m_index.insert(3, metadata); self->save(); done(1, {}, error); return; }
                if (current && !current()) {
                    self->m_store->remove(self->itemKey(id), [self, done, metadata](const QString &removeError, const QByteArray &) {
                        if (!self) return;
                        if (removeError.isEmpty()) { self->m_index.insert(3, metadata); self->save(); }
                        done(1, {}, removeError);
                    });
                    return;
                }
                done(0, encoded(response));
            });
        } else {
            QCborMap selected;
            for (const auto &entry : metadata) {
                auto meta = entry.toMap();
                if (meta.value(1).toString() == rp && meta.value(0).toByteArray() == credentialId
                    && (request.value(3).toArray().isEmpty() || contains(request.value(3).toArray(), credentialId))) selected = meta;
            }
            if (selected.isEmpty()) { PasskeyCrypto::wipe(key); done(4, {}, tr("No matching passkey is available.")); return; }
            self->m_store->read(self->itemKey(credentialId), [self, key, selected, credentialId, rp, hash, done, current](const QString &error, const QByteArray &data) mutable {
                if (!self) { PasskeyCrypto::wipe(key); return; }
                if (current && !current()) { PasskeyCrypto::wipe(key); done(1, {}); return; }
                auto plain = PasskeyCrypto::open(data, key, self->m_index.value(1).toByteArray() + credentialId);
                PasskeyCrypto::wipe(key);
                auto record = QCborValue::fromCbor(plain).toMap(); PasskeyCrypto::wipe(plain);
                if (!error.isEmpty() || record.value(0).toMap() != selected) { done(1, {}, tr("Could not unlock the passkey, or its catalog entry was changed.")); return; }
                auto der = record.value(1).toByteArray();
                auto auth = PasskeyCrypto::authenticatorData(rp, false,selected.value(4).toBool(),selected.value(5).toBool());
                auto signature = PasskeyCrypto::sign(der, auth + hash); PasskeyCrypto::wipe(der);
                if (signature.isEmpty()) { done(1, {}, tr("Could not sign the authentication request.")); return; }
                QCborMap descriptor; descriptor.insert(QStringLiteral("id"), credentialId); descriptor.insert(QStringLiteral("type"), QStringLiteral("public-key"));
                QCborMap user; user.insert(QStringLiteral("id"), selected.value(2)); user.insert(QStringLiteral("name"), selected.value(3));
                QCborMap response; response.insert(1, descriptor); response.insert(2, auth); response.insert(3, signature); response.insert(4, user);
                done(0, encoded(response));
            });
            PasskeyCrypto::wipe(key);
        }
    };
    {
        if(!m_biometrics || !m_biometrics->available()) { m_error=tr("Device verification is unavailable. Set up device authentication in system settings."); unlocked({}); return; }
        m_biometrics->authenticate(tr("Verify to use your Arbiter passkey"),[self,unlocked](bool success,const QString &error) mutable {
            if(!self) return;
            if(!success) { self->m_error=error; unlocked({}); return; }
            self->automaticKey(unlocked);
        });
    }
}


void PasskeyVault::lockSync() { ++m_syncEpoch; m_syncKey.clear(); emit syncLocked(); emit changed(); }
void PasskeyVault::unlockForSync(SyncResult callback)
{
    if(!begin()) { callback(m_error,{}); return; }
    const QPointer<PasskeyVault> self(this);
    const auto epoch=++m_syncEpoch;
    auto unlocked=[self,callback,epoch](QByteArray key) {
        if(!self) { PasskeyCrypto::wipe(key); return; }
        if(epoch!=self->m_syncEpoch) { PasskeyCrypto::wipe(key); self->finish(); callback(tr("Sync unlocking was cancelled."),{}); return; }
        if(key.isEmpty() || !self->m_syncKey.set(key)) {
            PasskeyCrypto::wipe(key); self->finish(self->m_error.isEmpty() ? tr("Could not unlock the vault for sync.") : self->m_error); callback(self->m_error,{}); return;
        }
        PasskeyCrypto::wipe(key); self->finish(); callback({},{});
    };
    automaticKey(unlocked);
}
void PasskeyVault::exportForSync(SyncResult callback)
{
    if(!syncReady() || !begin()) { callback(tr("Unlock the desktop sync vault and try again."),{}); return; }
    const auto metadata=m_index.value(3).toArray();
    auto result=std::make_shared<QJsonArray>();
    auto step=std::make_shared<std::function<void(int)>>(); const std::weak_ptr<std::function<void(int)>> weak=step;
    const QPointer<PasskeyVault> self(this);
    *step=[self,metadata,result,weak,callback](int index) {
        if(!self) return;
        if(!self->syncReady()) { self->finish(); callback(tr("The sync session expired."),{}); return; }
        if(index==metadata.size()) { self->finish(); callback({},*result); return; }
        const auto meta=metadata[index].toMap();
        const auto id=meta.value(0).toByteArray();
        const auto next=weak.lock();
        // Credentials registered as device-bound must not become replicated later.
        if(!meta.value(4).toBool()) { QTimer::singleShot(0,self,[next,index] { (*next)(index+1); }); return; }
        self->m_store->readSilently(self->itemKey(id),[self,meta,id,next,result,index,callback](const QString &error,const QByteArray &sealed) {
            if(!self) return;
            if(!error.isEmpty() || !self->syncReady()) { self->finish(error); callback(error.isEmpty() ? tr("The sync session expired.") : error,{}); return; }
            auto key=self->m_syncKey.copy();
            auto plain=PasskeyCrypto::open(sealed,key,self->m_index.value(1).toByteArray()+id); PasskeyCrypto::wipe(key);
            const auto record=QCborValue::fromCbor(plain).toMap(); PasskeyCrypto::wipe(plain);
            auto portable=PasskeyCrypto::portableKey(record.value(1).toByteArray());
            if(record.value(0).toMap()!=meta || portable.der.isEmpty() || (record.value(2).isByteArray() && portable.cose!=record.value(2).toByteArray())) {
                PasskeyCrypto::wipe(portable.der); self->finish(tr("A passkey could not be verified for transfer.")); callback(self->m_error,{}); return;
            }
            auto encode=[](const QByteArray &bytes) { return QString::fromLatin1(bytes.toBase64(QByteArray::Base64UrlEncoding|QByteArray::OmitTrailingEquals)); };
            result->append(QJsonObject{{"kind","passkey"},{"rpId",meta.value(1).toString()},{"credentialId",encode(id)},
                {"privateKey",encode(portable.der)},{"publicKey",encode(portable.cose)},{"userId",encode(meta.value(2).toByteArray())},
                {"username",meta.value(3).toString()},{"backupEligible",meta.value(4).toBool()},{"backedUp",meta.value(5).toBool()}});
            PasskeyCrypto::wipe(portable.der);
            QTimer::singleShot(0,self,[next,index] { (*next)(index+1); });
        });
    };
    (*step)(0);
}
void PasskeyVault::upsertFromSync(const QJsonObject &data,SyncWrite callback)
{
    if(!syncReady() || !begin()) { callback(tr("Unlock the desktop sync vault and try again.")); return; }
    auto decode=[](const QJsonValue &value) { return QByteArray::fromBase64(value.toString().toLatin1(),QByteArray::Base64UrlEncoding); };
    const auto id=decode(data.value("credentialId")),user=decode(data.value("userId"));
    auto portable=PasskeyCrypto::portableKey(decode(data.value("privateKey")));
    const auto rp=data.value("rpId").toString(),name=data.value("username").toString();
    if(!data.value("backupEligible").toBool() || id.size()!=32 || user.isEmpty() || user.size()>64 || rp.isEmpty() || rp.size()>253 || name.size()>256 || portable.der.isEmpty() || (data.value("backedUp").toBool() && !data.value("backupEligible").toBool())
        || (!data.value("publicKey").toString().isEmpty() && portable.cose!=decode(data.value("publicKey")))) {
        PasskeyCrypto::wipe(portable.der); finish(tr("The synced passkey is invalid or unsupported.")); callback(m_error); return;
    }
    QCborMap meta; meta.insert(0,id); meta.insert(1,rp); meta.insert(2,user); meta.insert(3,name);
    if(data.value("backupEligible").toBool()) meta.insert(4,true);
    if(data.value("backedUp").toBool()) meta.insert(5,true);
    auto next=m_index.value(3).toArray(); bool exists=false;
    for(int i=0;i<next.size();++i) if(next[i].toMap().value(0).toByteArray()==id) {
        if(next[i].toMap().value(1).toString()!=rp || next[i].toMap().value(2).toByteArray()!=user
            || next[i].toMap().value(4).toBool()!=meta.value(4).toBool()) {
            PasskeyCrypto::wipe(portable.der); finish(tr("A synced passkey conflicts with an existing credential identity.")); callback(m_error); return;
        }
        next[i]=meta; exists=true; break;
    }
    if(!exists) next.append(meta);
    QCborMap record; record.insert(0,meta); record.insert(1,portable.der); record.insert(2,portable.cose);
    auto key=m_syncKey.copy(),plain=QCborValue(record).toCbor();
    const auto sealed=PasskeyCrypto::seal(plain,key,m_index.value(1).toByteArray()+id);
    PasskeyCrypto::wipe(key); PasskeyCrypto::wipe(plain); PasskeyCrypto::wipe(portable.der);
    if(sealed.isEmpty() || sealed.size()>2560) { finish(tr("The synced passkey is too large for secure storage.")); callback(m_error); return; }
    // Reserve an index entry first, as in regular passkey creation. If writing
    // fails, restore the previous index; retries retain the same credential ID.
    const auto previous=m_index; m_index.insert(3,next);
    if(!save()) { m_index=previous; finish(tr("Could not save the synced passkey index.")); callback(m_error); return; }
    const QPointer<PasskeyVault> self(this);
    m_store->write(itemKey(id),sealed,[self,previous,callback](const QString &error,const QByteArray &) {
        if(!self) return;
        if(!error.isEmpty()) { self->m_index=previous; self->save(); }
        self->finish(error); callback(error);
    });
}
void PasskeyVault::removeFromSync(const QByteArray &id,SyncWrite callback)
{
    if(!syncReady() || !begin()) { callback(tr("Unlock the desktop sync vault and try again.")); return; }
    auto next=m_index.value(3).toArray(); bool found=false;
    for(int i=0;i<next.size();++i) if(next[i].toMap().value(0).toByteArray()==id) { next.removeAt(i); found=true; break; }
    if(!found) { finish(); callback({}); return; }
    const QPointer<PasskeyVault> self(this);
    m_store->remove(itemKey(id),[self,next,callback](const QString &error,const QByteArray &) {
        if(!self) return;
        if(!error.isEmpty()) { self->finish(error); callback(error); return; }
        const auto old=self->m_index; self->m_index.insert(3,next);
        if(!self->save()) { self->m_index=old; self->finish(tr("The passkey was removed, but its index needs a retry.")); callback(self->m_error); return; }
        self->finish(); callback({});
    });
}


void PasskeyVault::saveRootKey(const QByteArray &key,std::function<void(QByteArray)> callback) {
    if(key.size()!=32) { callback({}); return; }
    const QPointer<PasskeyVault> self(this);
    auto protectedKey=std::make_shared<LockedKey>();
    if(!protectedKey->set(key)) { m_error=tr("Could not protect the credential key."); callback({}); return; }
    m_store->write(QStringLiteral("credential-passkey-root/")+QString::fromLatin1(m_index.value(1).toByteArray().toHex()),key,
        [self,protectedKey,callback](const QString &error,const QByteArray &) {
            if(!self) return;
            if(!error.isEmpty()) { self->m_error=error; callback({}); return; }
            self->m_index.insert(2,true);
            if(!self->save()) { self->m_index.insert(2,false); self->m_error=tr("Could not save the passkey catalog."); callback({}); return; }
            auto key=protectedKey->copy(); protectedKey->clear(); callback(key); PasskeyCrypto::wipe(key);
        });
}
void PasskeyVault::automaticKey(std::function<void(QByteArray)> callback) {
    if(!m_index.value(2).toBool()) {
        // Save the identity first so interrupted initialization can be retried
        // without replacing an existing vault or its credentials.
        if(m_index.isEmpty()) {
            m_index.insert(0,2); m_index.insert(1,PasskeyCrypto::random(16));
            m_index.insert(2,false); m_index.insert(3,QCborArray());
            if(m_index.value(1).toByteArray().size()!=16 || !save()) { m_error=tr("Could not create the credential catalog."); callback({}); return; }
        }
        const auto id=QStringLiteral("credential-passkey-root/")+QString::fromLatin1(m_index.value(1).toByteArray().toHex());
        QPointer<PasskeyVault> self(this);
        m_store->readOptional(id,[self,callback](const QString &error,const QByteArray &stored) {
            if(!self) return;
            if(!error.isEmpty()) { self->m_error=error; callback({}); return; }
            auto key=stored.isEmpty() ? PasskeyCrypto::random(32) : stored;
            self->saveRootKey(key,callback); PasskeyCrypto::wipe(key);
        });
        return;
    }
    QPointer<PasskeyVault> self(this);
    m_store->read(QStringLiteral("credential-passkey-root/")+QString::fromLatin1(m_index.value(1).toByteArray().toHex()),
        [self,callback](const QString &error,const QByteArray &key) {
            if(!self) return;
            if(!error.isEmpty() || key.size()!=32) { self->m_error=error.isEmpty() ? tr("The credential key is missing. Existing passkeys were preserved.") : error; callback({}); return; }
            callback(key);
        });
}
