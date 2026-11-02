#include "../../src/credentialstore.h"
#include "../../src/credentialsync.h"
#include "../../src/passwordmanager.h"
#include "../../src/passkeyvault.h"
#include "../../src/passkeycrypto.h"
#include <QApplication>
#include <QTemporaryDir>
#include <QFile>
#include <QCborArray>
#include <QtTest>

class Secrets final : public SecretStore {
public:
    QHash<QString,QByteArray> items;
    int reads=0,writes=0,silent=0;
    bool denied=false,failWrites=false;
    void read(const QString &id,Callback cb) override { ++reads; cb(denied ? "denied" : items.contains(id) ? QString() : "missing",denied ? QByteArray() : items.value(id)); }
    void readOptional(const QString &id,Callback cb) override { ++reads; cb(denied ? "denied" : QString(),denied ? QByteArray() : items.value(id)); }
    void readOptionalSilently(const QString &id,Callback cb) override { ++silent; cb(denied ? "denied" : QString(),denied ? QByteArray() : items.value(id)); }
    void write(const QString &id,const QByteArray &value,Callback cb) override { ++writes; if(!failWrites) items[id]=value; cb(failWrites ? "write failed" : QString(),{}); }
    void remove(const QString &id,Callback cb) override { items.remove(id); cb({},{}); }
};
class Auth final : public BiometricAuth {
public:
    Auth() { refresh(); }
    bool success=true; int prompts=0;
    bool foregroundRequired() const override { return false; }
    QString name() const override { return "Device verification"; }
    void refresh() override { availability(true,{}); }
    void screenLock() { emit sessionLocked(); }
protected:
    void startNative(const QString &,quint64 token) override { ++prompts; complete(token,success,success ? QString() : "denied"); }
    void cancelNative() override {}
};
class CredentialStoreTest final : public QObject {
    Q_OBJECT
    struct Result { QString error; QByteArray data; };
    Result operation(std::function<void(SecretStore::Callback)> action) {
        QEventLoop loop; Result result; bool done=false;
        action([&](const QString &error,const QByteArray &data){ result={error,data};done=true;loop.quit(); });
        if(!done) { QTimer::singleShot(5000,&loop,&QEventLoop::quit); loop.exec(); }
        if(!done) QTest::qFail("Credential operation timed out",__FILE__,__LINE__);
        return result;
    }
    QByteArray request(bool create) {
        QCborMap r; r.insert(create ? 1 : 2,QByteArray(32,'a'));
        if(create) {
            QCborMap rp,user,alg; rp.insert(QStringLiteral("id"),QStringLiteral("example.com"));
            user.insert(QStringLiteral("id"),QByteArray("user")); user.insert(QStringLiteral("name"),QStringLiteral("alice"));
            alg.insert(QStringLiteral("type"),QStringLiteral("public-key")); alg.insert(QStringLiteral("alg"),-7);
            r.insert(2,rp);r.insert(3,user);r.insert(4,QCborArray{alg});
        } else r.insert(1,QStringLiteral("example.com"));
        return QCborValue(r).toCbor();
    }
    QByteArray perform(PasskeyVault &vault,bool create,const QByteArray &id={},const QString &phrase={},int expected=0) {
        QEventLoop loop; QByteArray result; int status=-1;
        vault.perform(create,request(create),id,phrase,[&](int s,const QByteArray &r){status=s;result=r;loop.quit();});
        if(status<0) {QTimer::singleShot(5000,&loop,&QEventLoop::quit);loop.exec();}
        if(status!=expected) QTest::qFail(qPrintable(QString("Unexpected passkey status %1: %2").arg(status).arg(vault.error())),__FILE__,__LINE__);
        return result;
    }
private slots:
    void encryptedSharedStoreAndRestart() {
        QTemporaryDir dir; Secrets keys,passwords,passkeys,sync;
        passwords.items["Arbiter/Passwords/v1/fixture/one"]="password-secret";
        passkeys.items["com.arbiter.browser/passkeys/v1/fixture/one"]="passkey-secret";
        {
            CredentialStore store(dir.path(),&keys,&passwords,&passkeys,&sync);
            QCOMPARE(operation([&](auto cb){store.read("Arbiter/Passwords/v1/fixture/one",cb);}).data,QByteArray("password-secret"));
            QCOMPARE(operation([&](auto cb){store.read("com.arbiter.browser/passkeys/v1/fixture/one",cb);}).data,QByteArray("passkey-secret"));
            QCOMPARE(keys.items.size(),1);
            QFile encrypted(dir.filePath("credentials.enc"));QVERIFY(encrypted.open(QIODevice::ReadOnly));
            const auto bytes=encrypted.readAll();QVERIFY(!bytes.contains("password-secret"));QVERIFY(!bytes.contains("passkey-secret"));
        }
        passwords.denied=true;passkeys.denied=true;
        CredentialStore reopened(dir.path(),&keys,&passwords,&passkeys,&sync);
        QCOMPARE(operation([&](auto cb){reopened.readSilently("Arbiter/Passwords/v1/fixture/one",cb);}).data,QByteArray("password-secret"));
        QCOMPARE(operation([&](auto cb){reopened.readSilently("com.arbiter.browser/passkeys/v1/fixture/one",cb);}).data,QByteArray("passkey-secret"));
        QCOMPARE(passwords.reads,1);QCOMPARE(passkeys.reads,1);
    }
    void deletionDoesNotReviveLegacyAndFailedMigrationPreservesOriginal() {
        QTemporaryDir dir; Secrets keys,legacy; const QString id="Arbiter/Passwords/v1/fixture/one";
        legacy.items[id]="original";
        CredentialStore store(dir.path(),&keys,&legacy,&legacy,&legacy);
        keys.failWrites=true;
        QVERIFY(!operation([&](auto cb){store.read(id,cb);}).error.isEmpty());QCOMPARE(legacy.items[id],QByteArray("original"));
        keys.failWrites=false;
        QVERIFY(operation([&](auto cb){store.read(id,cb);}).error.isEmpty());
        QVERIFY(operation([&](auto cb){store.remove(id,cb);}).error.isEmpty());
        QVERIFY(!legacy.items.contains(id));
        // Even an old external backup cannot bring the credential back.
        legacy.items[id]="stale backup";
        QCOMPARE(operation([&](auto cb){store.readOptional(id,cb);}).data,QByteArray());
        QVERIFY(!operation([&](auto cb){store.read(id,cb);}).error.isEmpty());QCOMPARE(legacy.reads,1);
    }
    void corruptedOrMissingKeyNeverResetsVault() {
        QTemporaryDir dir; Secrets keys,legacy;
        { CredentialStore store(dir.path(),&keys,&legacy,&legacy,&legacy);
          QVERIFY(operation([&](auto cb){store.write("fixture","secret",cb);}).error.isEmpty()); }
        QFile file(dir.filePath("credentials.enc"));QVERIFY(file.open(QIODevice::ReadOnly));const auto original=file.readAll();file.close();
        keys.items.clear();
        { CredentialStore store(dir.path(),&keys,&legacy,&legacy,&legacy);
          QVERIFY(!operation([&](auto cb){store.read("fixture",cb);}).error.isEmpty());QVERIFY(keys.items.isEmpty()); }
        QVERIFY(file.open(QIODevice::ReadOnly));QCOMPARE(file.readAll(),original);file.close();
        QVERIFY(file.open(QIODevice::WriteOnly));file.write("tampered");file.close();
        // Use a nonmatching OS key to exercise authentication failure as well.
        QFile identity(dir.filePath("identity"));QVERIFY(identity.open(QIODevice::ReadOnly));
        keys.items["com.arbiter.browser/credentials/v1/"+QString::fromLatin1(identity.readAll().toHex())]=PasskeyCrypto::random(32);
        CredentialStore store(dir.path(),&keys,&legacy,&legacy,&legacy);
        QVERIFY(!operation([&](auto cb){store.write("fixture","replacement",cb);}).error.isEmpty());
        QVERIFY(file.open(QIODevice::ReadOnly));QCOMPARE(file.readAll(),QByteArray("tampered"));
    }
    void freshPasskeysUseDeviceAuthenticationWithoutSetupAndKeepIdentity() {
        QTemporaryDir dir; Secrets keys,legacy;Auth auth; QByteArray id;
        {
            CredentialStore store(dir.filePath("credentials"),&keys,&legacy,&legacy,&legacy);
            PasskeyVault vault(&store,dir.filePath("passkeys"),nullptr,&auth);
            QVERIFY(vault.configured());QVERIFY(vault.automaticVault());
            perform(vault,true);QCOMPARE(auth.prompts,1);
            id=vault.credentials("example.com").first().toMap().value("id").toByteArray();
            auth.success=false;perform(vault,false,id,{},1);
            auth.success=true;perform(vault,false,id);QCOMPARE(auth.prompts,3);
        }
        legacy.denied=true;
        CredentialStore reopened(dir.filePath("credentials"),&keys,&legacy,&legacy,&legacy);
        PasskeyVault vault(&reopened,dir.filePath("passkeys"),nullptr,&auth);
        perform(vault,false,id);QCOMPARE(vault.credentials("example.com").first().toMap().value("id").toByteArray(),id);
    }
    void legacyPassphraseMigratesOnceWithoutChangingPasskey() {
        QTemporaryDir dir; Secrets keys,legacy;Auth auth;QByteArray id,otherId;
        const QString phrase="old passkey phrase fixture";
        { PasskeyVault old(&legacy,dir.filePath("passkeys"),nullptr,&auth);
          old.setup(phrase,phrase);QVERIFY(old.configured());perform(old,true,{},phrase);
          id=old.credentials("example.com").first().toMap().value("id").toByteArray();
          perform(old,true,{},phrase);otherId=old.credentials("example.com").last().toMap().value("id").toByteArray(); }
        // Simulate an older device-bound credential, which must migrate locally
        // while remaining ineligible for replication.
        QFile catalog(dir.filePath("passkeys/catalog.cbor"));QVERIFY(catalog.open(QIODevice::ReadOnly));
        auto index=QCborValue::fromCbor(catalog.readAll()).toMap();catalog.close();
        auto entries=index.value(3).toArray();auto bound=entries[0].toMap();bound.remove(4);entries[0]=bound;index.insert(3,entries);
        QString item;for(auto i=legacy.items.begin();i!=legacy.items.end();++i)if(i.key().endsWith(QString::fromLatin1(id.toHex())))item=i.key();
        QVERIFY(!item.isEmpty());const auto header=QCborValue::fromCbor(legacy.items.value(item.left(item.size()-64))).toMap();
        auto key=PasskeyCrypto::derive(phrase.toUtf8(),header.value(0).toByteArray());
        auto record=QCborValue::fromCbor(PasskeyCrypto::open(legacy.items[item],key,index.value(1).toByteArray()+id)).toMap();record.insert(0,bound);
        legacy.items[item]=PasskeyCrypto::seal(QCborValue(record).toCbor(),key,index.value(1).toByteArray()+id);PasskeyCrypto::wipe(key);
        QVERIFY(catalog.open(QIODevice::WriteOnly));catalog.write(QCborValue(index).toCbor());catalog.close();
        {
            CredentialStore store(dir.filePath("credentials"),&keys,&legacy,&legacy,&legacy);
            PasskeyVault vault(&store,dir.filePath("passkeys"),nullptr,&auth);
            QVERIFY(vault.needsMigration());vault.migrateLegacy(phrase);
            QTRY_VERIFY(!vault.busy());QVERIFY2(vault.error().isEmpty(),qPrintable(vault.error()));
            QVERIFY(vault.automaticVault());QVERIFY(!vault.needsMigration());
            // Every local record is now migrated, not just the selected one.
        }
        legacy.denied=true;
        CredentialStore store(dir.filePath("credentials"),&keys,&legacy,&legacy,&legacy);
        PasskeyVault vault(&store,dir.filePath("passkeys"),nullptr,&auth);
        perform(vault,false,id);perform(vault,false,otherId);QVERIFY(vault.error().isEmpty());
    }
    void rejectedOrCancelledEnrollmentDoesNotEnableSync() {
        QTemporaryDir dir;Secrets keys,legacy;Auth auth;
        CredentialStore store(dir.filePath("credentials"),&keys,&legacy,&legacy,&legacy);
        PasswordManager passwords(dir.filePath("passwords"),"Default",&store);
        PasskeyVault passkeys(&store,dir.filePath("passkeys"),nullptr,&auth);
        CredentialSync sync(&passwords,&passkeys,dir.filePath("sync"),&store,"127.0.0.1");
        PasswordManager::VerifyCallback pending;
        passwords.setUserVerification("Device",true,[&](const QString &,auto cb){pending=cb;});
        sync.setEnabled(true);QVERIFY(sync.busy());QVERIFY(pending);
        pending(false,"not verified");QVERIFY(!sync.enabled());QVERIFY(!sync.unlocked());
        QVERIFY(!QFile::exists(dir.filePath("sync/enabled")));QVERIFY(keys.items.isEmpty());
        sync.setEnabled(true);QVERIFY(sync.busy());
        auto stale=pending;sync.setEnabled(false);stale(true,{});
        QTest::qWait(50);QVERIFY(!sync.enabled());QVERIFY(!sync.unlocked());QVERIFY(keys.items.isEmpty());
    }
    void legacySyncIdentityAndCredentialsSurviveMigration() {
        QTemporaryDir dir;Secrets keys,passwordSecrets,passkeySecrets,syncSecrets;Auth auth;
        const QString phrase="old passkey phrase fixture";QString identity;QByteArray certificate;
        {
            PasswordManager passwords(dir.filePath("passwords"),"Default",&passwordSecrets);
            bool imported=false;passwords.importAccount(QUrl("https://example.com"),"alice","legacy password",[&](bool ok,const QString &error){QVERIFY(error.isEmpty());imported=ok;});QVERIFY(imported);
            PasskeyVault passkeys(&passkeySecrets,dir.filePath("passkeys"),nullptr,&auth);
            passkeys.setup(phrase,phrase);perform(passkeys,true,{},phrase);
            CredentialSync sync(&passwords,&passkeys,dir.filePath("sync"),&syncSecrets,"127.0.0.1");
            sync.unlock(phrase);QTRY_VERIFY2(!sync.busy() && sync.unlocked(),qPrintable(sync.error()));
            sync.pair();identity=sync.pairingCode();
            QFile cert(dir.filePath("sync/certificate.pem"));QVERIFY(cert.open(QIODevice::ReadOnly));certificate=cert.readAll();
        }
        auto info=[](const QString &code){return QJsonDocument::fromJson(QByteArray::fromBase64(QUrl(code).query().section('=',1).toLatin1(),QByteArray::Base64UrlEncoding)).object();};
        {
            CredentialStore store(dir.filePath("credentials"),&keys,&passwordSecrets,&passkeySecrets,&syncSecrets);
            PasswordManager passwords(dir.filePath("passwords"),"Default",&store);
            passwords.setUserVerification("Device",true,[](const QString &,auto cb){cb(true,{});});
            PasskeyVault passkeys(&store,dir.filePath("passkeys"),nullptr,&auth);
            CredentialSync sync(&passwords,&passkeys,dir.filePath("sync"),&store,"127.0.0.1");
            sync.setEnabled(true,phrase);QTRY_VERIFY2(!sync.busy() && sync.enabled(),qPrintable(sync.error()));
            QVERIFY(passkeys.automaticVault());sync.pair();
            QCOMPARE(info(sync.pairingCode()).value("vaultId"),info(identity).value("vaultId"));
            QCOMPARE(info(sync.pairingCode()).value("pin"),info(identity).value("pin"));
        }
        passwordSecrets.denied=true;passkeySecrets.denied=true;syncSecrets.denied=true;
        CredentialStore store(dir.filePath("credentials"),&keys,&passwordSecrets,&passkeySecrets,&syncSecrets);
        PasswordManager passwords(dir.filePath("passwords"),"Default",&store);
        PasskeyVault passkeys(&store,dir.filePath("passkeys"),nullptr,&auth);
        CredentialSync sync(&passwords,&passkeys,dir.filePath("sync"),&store,"127.0.0.1");
        QTRY_VERIFY2(!sync.busy() && sync.unlocked(),qPrintable(sync.error()));
        bool exported=false;
        passwords.exportAfterVerification([&](const QString &error,const QJsonArray &records){QVERIFY(error.isEmpty());QCOMPARE(records.size(),1);QCOMPARE(records.first().toObject().value("password").toString(),QString("legacy password"));exported=true;});QTRY_VERIFY(exported);
        const auto id=passkeys.credentials("example.com").first().toMap().value("id").toByteArray();perform(passkeys,false,id);
        QFile cert(dir.filePath("sync/certificate.pem"));QVERIFY(cert.open(QIODevice::ReadOnly));QCOMPARE(cert.readAll(),certificate);
    }
    void syncOptInSurvivesRestartAndScreenLockAndOptOutPersists() {
        QTemporaryDir dir;Secrets keys,legacy;Auth auth;QString invitation;
        auto run=[&](bool first) {
            CredentialStore store(dir.filePath("credentials"),&keys,&legacy,&legacy,&legacy);
            PasswordManager passwords(dir.filePath("passwords"),"Default",&store);
            int verifications=0;
            passwords.setUserVerification("Device",true,[&](const QString &,auto cb){++verifications;cb(true,{});});
            PasskeyVault passkeys(&store,dir.filePath("passkeys"),nullptr,&auth);
            CredentialSync sync(&passwords,&passkeys,dir.filePath("sync"),&store,"127.0.0.1");
            if(first) {
                QVERIFY(!sync.enabled());sync.setEnabled(true);
            }
            QTRY_VERIFY2_WITH_TIMEOUT(sync.unlocked() && !sync.busy(),qPrintable(sync.error()),10000);
            QVERIFY2(sync.error().isEmpty(),qPrintable(sync.error()));QVERIFY(sync.enabled());
            QCOMPARE(verifications,first ? 1 : 0);
            auth.screenLock();QVERIFY(sync.unlocked());
            sync.pair();QVERIFY(!sync.pairingCode().isEmpty());
            const auto data=QUrl(sync.pairingCode()).query().section('=',1);
            const auto info=QJsonDocument::fromJson(QByteArray::fromBase64(data.toLatin1(),QByteArray::Base64UrlEncoding)).object();
            if(first) invitation=info.value("vaultId").toString();
            else { QCOMPARE(info.value("vaultId").toString(),invitation);sync.setEnabled(false);QVERIFY(!sync.enabled());QVERIFY(!sync.unlocked()); }
        };
        run(true);legacy.denied=true;run(false);
        CredentialStore store(dir.filePath("credentials"),&keys,&legacy,&legacy,&legacy);
        PasswordManager passwords(dir.filePath("passwords"),"Default",&store);PasskeyVault passkeys(&store,dir.filePath("passkeys"),nullptr,&auth);
        CredentialSync sync(&passwords,&passkeys,dir.filePath("sync"),&store,"127.0.0.1");
        QTest::qWait(100);QVERIFY(!sync.enabled());QVERIFY(!sync.unlocked());
    }
};
QTEST_MAIN(CredentialStoreTest)
#include "credentialstore_test.moc"
