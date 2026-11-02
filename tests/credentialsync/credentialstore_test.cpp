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
    QByteArray perform(PasskeyVault &vault,bool create,const QByteArray &id={},int expected=0) {
        QEventLoop loop; QByteArray result; int status=-1;
        vault.perform(create,request(create),id,[&](int s,const QByteArray &r){status=s;result=r;loop.quit();});
        if(status<0) {QTimer::singleShot(5000,&loop,&QEventLoop::quit);loop.exec();}
        if(status!=expected) QTest::qFail(qPrintable(QString("Unexpected passkey status %1: %2").arg(status).arg(vault.error())),__FILE__,__LINE__);
        return result;
    }
private slots:
    void sharedStoreRestartsWithoutPerCredentialKeychainReads() {
        QTemporaryDir dir;Secrets keys;
        const QByteArray password(12000,'p'),passkey(200,'k');
        {
            CredentialStore store(dir.path(),&keys);
            QVERIFY(operation([&](auto cb){store.write("password",password,cb);}).error.isEmpty());
            QVERIFY(operation([&](auto cb){store.write("passkey",passkey,cb);}).error.isEmpty());
            QCOMPARE(keys.items.size(),1);QCOMPARE(keys.reads,1);
            QFile encrypted(dir.filePath("credentials.enc"));QVERIFY(encrypted.open(QIODevice::ReadOnly));QVERIFY(!encrypted.readAll().contains(password));
        }
        CredentialStore store(dir.path(),&keys);
        QCOMPARE(operation([&](auto cb){store.readSilently("password",cb);}).data,password);
        QCOMPARE(operation([&](auto cb){store.readSilently("passkey",cb);}).data,passkey);
        QCOMPARE(keys.silent,1);
        const int reads=keys.reads+keys.silent;
        QVERIFY(operation([&](auto cb){store.remove("password",cb);}).error.isEmpty());
        QVERIFY(!operation([&](auto cb){store.read("password",cb);}).error.isEmpty());
        QCOMPARE(operation([&](auto cb){store.readOptional("Arbiter/Passwords/v1/unmigrated",cb);}).data,QByteArray());
        QCOMPARE(keys.reads+keys.silent,reads); // Missing entries never reach a legacy backend.
    }


    void corruptedOrMissingKeyNeverResetsVault() {
        QTemporaryDir dir; Secrets keys;
        { CredentialStore store(dir.path(),&keys);
          QVERIFY(operation([&](auto cb){store.write("fixture","secret",cb);}).error.isEmpty()); }
        QFile file(dir.filePath("credentials.enc"));QVERIFY(file.open(QIODevice::ReadOnly));const auto original=file.readAll();file.close();
        keys.items.clear();
        { CredentialStore store(dir.path(),&keys);
          QVERIFY(!operation([&](auto cb){store.read("fixture",cb);}).error.isEmpty());QVERIFY(keys.items.isEmpty()); }
        QVERIFY(file.open(QIODevice::ReadOnly));QCOMPARE(file.readAll(),original);file.close();
        QVERIFY(file.open(QIODevice::WriteOnly));file.write("tampered");file.close();
        // Use a nonmatching OS key to exercise authentication failure as well.
        QFile identity(dir.filePath("identity"));QVERIFY(identity.open(QIODevice::ReadOnly));
        keys.items["com.arbiter.browser/credentials/v1/"+QString::fromLatin1(identity.readAll().toHex())]=PasskeyCrypto::random(32);
        CredentialStore store(dir.path(),&keys);
        QVERIFY(!operation([&](auto cb){store.write("fixture","replacement",cb);}).error.isEmpty());
        QVERIFY(file.open(QIODevice::ReadOnly));QCOMPARE(file.readAll(),QByteArray("tampered"));
    }
    void freshPasskeysUseDeviceAuthenticationWithoutSetupAndKeepIdentity() {
        QTemporaryDir dir; Secrets keys;Auth auth; QByteArray id;
        {
            CredentialStore store(dir.filePath("credentials"),&keys);
            PasskeyVault vault(&store,dir.filePath("passkeys"),nullptr,&auth);
            QVERIFY(vault.configured());
            perform(vault,true);QCOMPARE(auth.prompts,1);
            id=vault.credentials("example.com").first().toMap().value("id").toByteArray();
            auth.success=false;perform(vault,false,id,1);
            auth.success=true;perform(vault,false,id);QCOMPARE(auth.prompts,3);
        }

        CredentialStore reopened(dir.filePath("credentials"),&keys);
        PasskeyVault vault(&reopened,dir.filePath("passkeys"),nullptr,&auth);
        perform(vault,false,id);QCOMPARE(vault.credentials("example.com").first().toMap().value("id").toByteArray(),id);
    }

    void rejectedOrCancelledEnrollmentDoesNotEnableSync() {
        QTemporaryDir dir;Secrets keys;Auth auth;
        CredentialStore store(dir.filePath("credentials"),&keys);
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

    void syncOptInSurvivesRestartAndScreenLockAndOptOutPersists() {
        QTemporaryDir dir;Secrets keys;Auth auth;QString invitation;
        auto run=[&](bool first) {
            CredentialStore store(dir.filePath("credentials"),&keys);
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
        run(true);run(false);
        CredentialStore store(dir.filePath("credentials"),&keys);
        PasswordManager passwords(dir.filePath("passwords"),"Default",&store);PasskeyVault passkeys(&store,dir.filePath("passkeys"),nullptr,&auth);
        CredentialSync sync(&passwords,&passkeys,dir.filePath("sync"),&store,"127.0.0.1");
        QTest::qWait(100);QVERIFY(!sync.enabled());QVERIFY(!sync.unlocked());
    }
};
QTEST_MAIN(CredentialStoreTest)
#include "credentialstore_test.moc"
