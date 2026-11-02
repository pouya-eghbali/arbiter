#include "credentialstore.h"
#include "passkeycrypto.h"
#include <QDir>
#include <QFile>
#include <QLockFile>
#include <QSaveFile>
#include <QTimer>
#include <QCborValue>

namespace {
constexpr auto maximumBytes=64*1024*1024;
const auto permissions=QFileDevice::ReadOwner|QFileDevice::WriteOwner;
bool writeFile(const QString &path,const QByteArray &data) {
    QSaveFile file(path);
    return file.open(QIODevice::WriteOnly) && file.setPermissions(permissions)
        && file.write(data)==data.size() && file.commit();
}
}
CredentialStore::CredentialStore(const QString &directory,SecretStore *keys,SecretStore *passwords,
    SecretStore *passkeys,SecretStore *sync,QObject *parent)
    :SecretStore(parent),m_directory(directory),m_keys(keys),m_passwords(passwords),m_passkeys(passkeys),m_sync(sync) {}
void CredentialStore::read(const QString &key,Callback cb) { enqueue({key,{},0,true,false,std::move(cb)}); }
void CredentialStore::readOptional(const QString &key,Callback cb) { enqueue({key,{},0,true,true,std::move(cb)}); }
void CredentialStore::readSilently(const QString &key,Callback cb) { enqueue({key,{},0,false,false,std::move(cb)}); }
void CredentialStore::readOptionalSilently(const QString &key,Callback cb) { enqueue({key,{},0,false,true,std::move(cb)}); }
void CredentialStore::write(const QString &key,const QByteArray &data,Callback cb) { enqueue({key,data,1,true,false,std::move(cb)}); }
void CredentialStore::remove(const QString &key,Callback cb) { enqueue({key,{},2,true,true,std::move(cb)}); }
void CredentialStore::enqueue(Job job) {
    m_jobs.enqueue(std::move(job));
    if(!m_running) { m_running=true; QTimer::singleShot(0,this,[this]{next();}); }
}
void CredentialStore::next() {
    if(m_jobs.isEmpty()) return;
    if(!QDir().mkpath(m_directory) || !QFile::setPermissions(m_directory,permissions|QFileDevice::ExeOwner)) {
        complete(tr("Could not create the credential store.")); return;
    }
    m_lock=std::make_unique<QLockFile>(m_directory+"/store.lock");
    if(!m_lock->tryLock(0)) { complete(tr("The credential store is in use. Try again.")); return; }
    QFile identity(m_directory+"/identity");
    if(identity.exists()) {
        if(!identity.open(QIODevice::ReadOnly) || identity.size()!=16) { complete(tr("The credential store identity is damaged. It was not replaced.")); return; }
        const auto id=identity.readAll();
        if(!m_id.isEmpty() && m_id!=id) { m_key.clear(); }
        m_id=id;
    } else {
        if(QFile::exists(m_directory+"/credentials.enc")) { complete(tr("The credential store identity is missing. Existing credentials were not overwritten.")); return; }
        m_id=PasskeyCrypto::random(16);
        if(m_id.size()!=16 || !writeFile(identity.fileName(),m_id)) { complete(tr("Could not save the credential store identity.")); return; }
    }
    if(m_key.present()) { execute(); return; }
    if(!m_keys) { complete(tr("The OS credential store is unavailable.")); return; }
    const QString account="com.arbiter.browser/credentials/v1/"+QString::fromLatin1(m_id.toHex());
    QPointer<CredentialStore> self(this);
    auto loaded=[self,account](const QString &error,const QByteArray &key) {
        if(!self) return;
        if(!error.isEmpty()) { self->complete(error); return; }
        if(!key.isEmpty()) { self->opened({},key); return; }
        if(QFile::exists(self->m_directory+"/credentials.enc")) {
            self->complete(tr("The OS credential key is missing. Existing credentials were not overwritten.")); return;
        }
        auto generated=PasskeyCrypto::random(32);
        if(generated.size()!=32) { self->complete(tr("Could not initialize credential encryption.")); return; }
        // The queued callback owns a protected copy until the OS write finishes.
        auto protectedKey=std::make_shared<LockedKey>();
        if(!protectedKey->set(generated)) { PasskeyCrypto::wipe(generated); self->complete(tr("Could not protect the credential key.")); return; }
        self->m_keys->write(account,generated,[self,protectedKey](const QString &error,const QByteArray &) {
            if(!self) return;
            auto key=protectedKey->copy(); protectedKey->clear(); self->opened(error,key); PasskeyCrypto::wipe(key);
        });
        PasskeyCrypto::wipe(generated);
    };
    if(m_jobs.head().interactive) m_keys->readOptional(account,loaded);
    else m_keys->readOptionalSilently(account,loaded);
}
void CredentialStore::opened(const QString &error,const QByteArray &key) {
    if(!error.isEmpty() || key.size()!=32 || !m_key.set(key)) { complete(error.isEmpty() ? tr("The OS credential key is invalid.") : error); return; }
    execute();
}
SecretStore *CredentialStore::legacy(const QString &key) const {
    if(key.startsWith("com.arbiter.browser/passkeys/v1/")) return m_passkeys;
    if(key.startsWith("sync-v1-")) return m_sync;
    if(key.startsWith("Arbiter/Passwords/v1/")) return m_passwords;
    return nullptr;
}
bool CredentialStore::save(const QCborMap &records) {
    auto plain=QCborValue(records).toCbor(),key=m_key.copy();
    const auto sealed=PasskeyCrypto::seal(plain,key,"ArbiterCredentials/v1/"+m_id);
    PasskeyCrypto::wipe(key); PasskeyCrypto::wipe(plain);
    return !sealed.isEmpty() && sealed.size()<=maximumBytes && writeFile(m_directory+"/credentials.enc",sealed);
}
void CredentialStore::execute() {
    m_records.clear();
    QFile file(m_directory+"/credentials.enc");
    if(file.exists()) {
        if(!file.open(QIODevice::ReadOnly) || file.size()>maximumBytes) { complete(tr("Could not read the encrypted credentials.")); return; }
        auto key=m_key.copy(),plain=PasskeyCrypto::open(file.readAll(),key,"ArbiterCredentials/v1/"+m_id);
        PasskeyCrypto::wipe(key); QCborParserError error;
        auto value=QCborValue::fromCbor(plain,&error); PasskeyCrypto::wipe(plain);
        if(error.error!=QCborError::NoError || !value.isMap()) { complete(tr("Credential authentication failed. Existing credentials were not overwritten.")); return; }
        m_records=value.toMap();
    }
    const auto &job=m_jobs.head();
    if(job.operation) {
        // A null tombstone prevents a removed legacy item from being reimported.
        m_records.insert(job.key,job.operation==2 ? QCborValue(nullptr) : QCborValue(job.data));
        if(!save(m_records)) { complete(tr("Could not save encrypted credentials.")); return; }
        if(job.operation==2) {
            // An explicit deletion also removes any retained migration source.
            // Commit the tombstone first, so a failed cleanup cannot resurrect it.
            if(auto *source=legacy(job.key)) {
                const QPointer<CredentialStore> self(this);
                source->remove(job.key,[self](const QString &error,const QByteArray &) { if(self) self->complete(error); });
                return;
            }
        }
        complete({}); return;
    }
    if(m_records.contains(job.key)) {
        const auto value=m_records.value(job.key);
        complete(value.isByteArray() || job.optional ? QString() : tr("This credential is no longer available."),value.toByteArray()); return;
    }
    auto *source=legacy(job.key);
    if(!source) { complete(job.optional ? QString() : tr("The credential is missing.")); return; }
    const QPointer<CredentialStore> self(this);
    auto imported=[self](const QString &error,const QByteArray &data) {
        if(!self) return;
        if(!error.isEmpty()) { self->complete(error); return; }
        if(data.isEmpty() && !self->m_jobs.head().optional) { self->complete(tr("The legacy credential is missing. It was not replaced.")); return; }
        if(!data.isEmpty()) {
            self->m_records.insert(self->m_jobs.head().key,data);
            if(!self->save(self->m_records)) { self->complete(tr("Could not migrate the credential. The original was preserved.")); return; }
        }
        // Keep legacy records intact as migration backups. All future reads
        // use the authenticated shared store, including after an app restart.
        self->complete({},data);
    };
    if(job.interactive) source->readOptional(job.key,imported);
    else source->readOptionalSilently(job.key,imported);
}
void CredentialStore::complete(const QString &error,const QByteArray &data) {
    auto result=data; auto job=m_jobs.dequeue();
    PasskeyCrypto::wipe(job.data); m_records.clear(); m_lock.reset();
    QPointer<CredentialStore> self(this); job.callback(error,result); PasskeyCrypto::wipe(result);
    if(self) {
        if(!m_jobs.isEmpty()) QTimer::singleShot(0,this,[this]{next();});
        else m_running=false;
    }
}
