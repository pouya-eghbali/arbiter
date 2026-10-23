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
const QByteArray verifier("Arbiter passkey vault v1");
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
    m_cacheExpiry.setSingleShot(true); m_cacheExpiry.setInterval(15*60*1000);
    connect(&m_cacheExpiry,&QTimer::timeout,this,&PasskeyVault::lockBiometrics);
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
PasskeyVault::~PasskeyVault() { lockBiometrics(); }
bool PasskeyVault::biometricReady() const {
    return biometricEnabled() && biometricAvailable() && m_cacheExpiry.isActive()
        && m_cachedKey.present() && m_cachedVaultId==m_index.value(1).toByteArray();
}
bool PasskeyVault::cacheKey(const QByteArray &key) {
    if(m_biometrics && m_biometrics->foregroundRequired() && QGuiApplication::applicationState()!=Qt::ApplicationActive) return false;
    if(!m_cachedKey.set(key)) return false;
    m_cachedVaultId=m_index.value(1).toByteArray(); m_cacheExpiry.start(); return true;
}
void PasskeyVault::cancelBiometric() { ++m_biometricEpoch; if(m_biometrics) m_biometrics->cancel(); }
void PasskeyVault::lockBiometrics() {
    m_cacheExpiry.stop(); m_cachedKey.clear(); m_cachedVaultId.clear();
    cancelBiometric(); emit changed();
}
void PasskeyVault::setBiometricEnabled(bool enabled, const QString &passphrase) {
    if(!begin()) return;
    if(!configured()) { finish(tr("Set up the passkey vault first.")); return; }
    if(!enabled) {
        lockBiometrics(); m_index.insert(4,false);
        finish(save() ? QString() : tr("Could not save the biometric preference.")); return;
    }
    if(!biometricAvailable()) { finish(tr("Biometric verification is unavailable. Use your vault passphrase.")); return; }
    QPointer<PasskeyVault> self(this);
    const auto epoch=m_biometricEpoch;
    unlock(passphrase,[self,epoch](QByteArray key) {
        if(!self) { PasskeyCrypto::wipe(key); return; }
        if(epoch!=self->m_biometricEpoch) { PasskeyCrypto::wipe(key); self->finish(tr("Biometric setup was cancelled.")); return; }
        if(key.size()!=32) { self->finish(tr("Could not verify the vault passphrase. Wait before trying again.")); return; }
        if(!self->cacheKey(key)) { PasskeyCrypto::wipe(key); self->finish(tr("Could not protect the biometric session key in memory. Use your vault passphrase.")); return; }
        PasskeyCrypto::wipe(key);
        self->m_biometrics->authenticate(tr("Enable biometric unlocking for your passkey vault"),[self,epoch](bool success,const QString &error) {
            if(!self) return;
            if(!success || epoch!=self->m_biometricEpoch || !self->m_cachedKey.present() || !self->m_cacheExpiry.isActive()
                || (self->m_biometrics->foregroundRequired() && QGuiApplication::applicationState()!=Qt::ApplicationActive)) {
                self->lockBiometrics(); self->finish(error.isEmpty() ? tr("Biometric setup was cancelled. Use your vault passphrase.") : error); return;
            }
            const bool previous=self->biometricEnabled(); self->m_index.insert(4,true);
            if(!self->save()) { self->m_index.insert(4,previous); self->lockBiometrics(); self->finish(tr("Could not save the biometric preference.")); return; }
            self->finish();
        });
    });
}
void PasskeyVault::unlockBiometric(std::function<void(QByteArray)> callback) {
    if(!biometricReady()) { m_error=tr("Enter your vault passphrase to enable biometric unlocking for this session."); callback({}); return; }
    QPointer<PasskeyVault> self(this);
    const auto epoch=m_biometricEpoch;
    m_biometrics->authenticate(tr("Unlock your passkey vault for this website"),[self,callback,epoch](bool success,const QString &error) {
        if(!self) return;
        if(!success || epoch!=self->m_biometricEpoch || !self->biometricReady()) { self->m_error=error.isEmpty() ? tr("The biometric session expired. Use your vault passphrase.") : error; callback({}); return; }
        // Re-check the authenticated header with the cached key, so a changed
        // vault/header cannot silently reuse a previous session's authorization.
        self->m_store->read(self->itemKey({}),[self,callback,epoch](const QString &error,const QByteArray &data) {
            if(!self) return;
            if(epoch!=self->m_biometricEpoch || !self->biometricReady() || !error.isEmpty() || data.size()>2560) { self->m_error=tr("Could not unlock the vault. Use your vault passphrase."); callback({}); return; }
            const auto header=QCborValue::fromCbor(data).toMap();
            auto key=self->m_cachedKey.copy();
            auto check=PasskeyCrypto::open(header.value(1).toByteArray(),key,self->m_index.value(1).toByteArray());
            const bool valid=check==verifier; PasskeyCrypto::wipe(check);
            if(!valid) { PasskeyCrypto::wipe(key); self->lockBiometrics(); self->m_error=tr("The vault has changed. Use your vault passphrase."); callback({}); return; }
            callback(key); PasskeyCrypto::wipe(key);
        });
    });
}

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
    if (map.value(0).toInteger() != 1 || map.value(1).toByteArray().size() != 16
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
void PasskeyVault::setup(const QString &passphrase, const QString &confirmation)
{
    if (!begin()) return;
    if (!m_index.isEmpty()) { finish(tr("A vault already exists or setup was interrupted. Restore the existing vault; it will not be overwritten.")); return; }
    if (passphrase.toUcs4().size() < 12 || passphrase.toUtf8().size() > 1024 || passphrase != confirmation) {
        finish(tr("Use at least 12 characters and enter the same passphrase twice.")); return;
    }
    auto salt = PasskeyCrypto::random(32);
    auto phrase = passphrase.toUtf8();
    auto key = PasskeyCrypto::derive(phrase, salt); PasskeyCrypto::wipe(phrase);
    const auto vaultId = QUuid::createUuid().toRfc4122();
    QCborMap header;
    header.insert(0, salt);
    header.insert(1, PasskeyCrypto::seal(verifier, key, vaultId));
    PasskeyCrypto::wipe(key);
    if (header.value(1).toByteArray().isEmpty()) { finish(tr("Could not initialize cryptography.")); return; }
    m_index.insert(0, 1); m_index.insert(1, vaultId); m_index.insert(2, false); m_index.insert(3, QCborArray());
    // Reserve the vault identity before writing the OS secret. A crash cannot
    // silently overwrite an existing vault or replace its verification policy.
    if (!save()) { finish(tr("Could not save the passkey catalog.")); return; }
    QPointer<PasskeyVault> self(this);
    m_store->write(itemKey({}), encoded(header), [self](const QString &error, const QByteArray &) {
        if (!self) return;
        if (!error.isEmpty()) { self->finish(error); return; }
        self->m_index.insert(2, true);
        if (!self->save()) {
            self->m_index.insert(2, false);
            self->finish(tr("Could not finish saving the vault catalog."));
            return;
        }
        self->finish();
    });
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
void PasskeyVault::unlock(const QString &passphrase, std::function<void(QByteArray)> callback)
{
    if (!configured() || QDateTime::currentMSecsSinceEpoch() < m_retryAfter) { callback({}); return; }
    QPointer<PasskeyVault> self(this);
    m_store->read(itemKey({}), [self, passphrase, callback](const QString &error, const QByteArray &data) {
        if (!self) return;
        if (!error.isEmpty()) { self->m_error = error; callback({}); return; }
        if (data.size() > 2560) { self->m_error = tr("The secure vault record is invalid."); callback({}); return; }
        const auto header = QCborValue::fromCbor(data).toMap();
        auto phrase = passphrase.toUtf8();
        auto key = PasskeyCrypto::derive(phrase, header.value(0).toByteArray()); PasskeyCrypto::wipe(phrase);
        auto check = PasskeyCrypto::open(header.value(1).toByteArray(), key, self->m_index.value(1).toByteArray());
        const bool valid = check == verifier; PasskeyCrypto::wipe(check);
        if (!valid) {
            PasskeyCrypto::wipe(key);
            self->m_retryAfter = QDateTime::currentMSecsSinceEpoch() + qMin(60000, 1000 * (1 << qMin(++self->m_failures, 6)));
        } else {
            self->m_failures = 0;
            if(self->biometricEnabled() && self->biometricAvailable()) self->cacheKey(key);
        }
        callback(key); PasskeyCrypto::wipe(key);
    });
}
void PasskeyVault::perform(bool registration, const QByteArray &bytes, const QByteArray &credentialId,
                           const QString &passphrase, Result callback, std::function<bool()> current, bool biometric)
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
    if(biometric) {
        const auto epoch=m_biometricEpoch;
        const auto original=current;
        current=[self,epoch,original] {
            return self && self->m_biometricEpoch==epoch
                && (!self->m_biometrics || !self->m_biometrics->foregroundRequired() || QGuiApplication::applicationState()==Qt::ApplicationActive)
                && (!original || original());
        };
    }
    auto unlocked = [self, request, rp, hash, credentialId, registration, callback, current, biometric](QByteArray key) mutable {
        if (!self) { PasskeyCrypto::wipe(key); return; }
        auto done = [self, callback](int status, const QByteArray &response, const QString &error = {}) {
            if (self) self->finish(error);
            callback(status, response);
        };
        if (current && !current()) { PasskeyCrypto::wipe(key); done(1, {}); return; }
        if (biometric && self->m_biometrics && self->m_biometrics->foregroundRequired()
            && QGuiApplication::applicationState()!=Qt::ApplicationActive) {
            PasskeyCrypto::wipe(key); self->lockBiometrics(); done(1, {}, tr("Focus Arbiter and use your vault passphrase.")); return;
        }
        if (key.isEmpty()) { done(1, {}, self->m_error.isEmpty() ? tr("Verification failed. Wait before trying again, then enter your vault passphrase.") : self->m_error); return; }
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
            QCborMap record; record.insert(0, meta); record.insert(1, pair.der);
            auto plain = encoded(record);
            auto sealed = PasskeyCrypto::seal(plain, key, self->m_index.value(1).toByteArray() + id);
            PasskeyCrypto::wipe(key); PasskeyCrypto::wipe(plain); PasskeyCrypto::wipe(pair.der);
            if (sealed.isEmpty() || sealed.size() > 2560) { done(1, {}, tr("The passkey exceeds the secure storage limit.")); return; }
            auto next = metadata; next.append(meta); self->m_index.insert(3, next);
            if (!self->save()) { self->m_index.insert(3, metadata); done(1, {}, tr("Could not reserve the passkey catalog entry.")); return; }
            auto auth = PasskeyCrypto::authenticatorData(rp, true);
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
                auto auth = PasskeyCrypto::authenticatorData(rp, false);
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
    if(biometric) unlockBiometric(unlocked);
    else unlock(passphrase,unlocked);
}
