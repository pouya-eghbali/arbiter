#include "../../src/credentialsync.h"
#include "../../src/passwordmanager.h"
#include "../../src/passkeyvault.h"
#include "../../src/passkeycrypto.h"
#include <QCborArray>
#include <QJsonArray>
#include <QFile>
#include <QJsonDocument>
#include <QSignalSpy>
#include <QSslSocket>
#include <QTemporaryDir>
#include <QtTest>
#include "testdeviceauth.h"

class MemorySecrets : public SecretStore {
public:
    QHash<QString,QByteArray> items;
    void read(const QString &key,Callback callback) override { callback(items.contains(key) ? QString() : QStringLiteral("Missing fixture item"),items.value(key)); }
    void readOptional(const QString &key,Callback callback) override { callback({},items.value(key)); }
    void write(const QString &key,const QByteArray &bytes,Callback callback) override { items[key]=bytes; callback({},{}); }
    void remove(const QString &key,Callback callback) override { items.remove(key); callback({},{}); }
};
class CredentialTransferTest : public QObject {
    Q_OBJECT
    QJsonObject info(const CredentialSync &sync) {
        const QUrl code(sync.pairingCode());
        const auto encoded=code.query().section('=',1);
        return QJsonDocument::fromJson(QByteArray::fromBase64(encoded.toLatin1(),QByteArray::Base64UrlEncoding)).object();
    }
    struct Response { int status=0; QJsonObject data; };
    Response post(const QJsonObject &pair,const QString &path,const QString &token,const QJsonObject &data,bool origin=false) {
        const QUrl endpoint(pair.value("endpoint").toString());
        const auto key=QByteArray::fromBase64(pair.value("key").toString().toLatin1(),QByteArray::Base64UrlEncoding);
        const auto context="ArbiterSync/v1/"+pair.value("vaultId").toString().toUtf8();
        const auto body=ArbiterSync::seal(QJsonDocument(data).toJson(QJsonDocument::Compact),key,context);
        QSslSocket socket; socket.setPeerVerifyMode(QSslSocket::VerifyNone);
        QEventLoop loop; QByteArray received;
        connect(&socket,&QSslSocket::encrypted,&loop,[&] {
            const auto pin=QString::fromLatin1(socket.peerCertificate().digest(QCryptographicHash::Sha256).toHex());
            if(pin!=pair.value("pin").toString()) { qWarning() << "Fixture peer pin did not match"; socket.abort(); loop.quit(); return; }
            socket.write("POST "+path.toLatin1()+" HTTP/1.1\r\nHost: localhost\r\nAuthorization: Bearer "+token.toLatin1()+
                "\r\nContent-Type: application/octet-stream\r\nContent-Length: "+QByteArray::number(body.size())+"\r\n"+
                (origin ? QByteArray("Origin: https://untrusted.example\r\n") : QByteArray())+"\r\n"+body);
        });
        connect(&socket,&QSslSocket::readyRead,&loop,[&] { received+=socket.readAll(); });
        connect(&socket,&QSslSocket::disconnected,&loop,&QEventLoop::quit);
        QTimer::singleShot(10000,&loop,&QEventLoop::quit);
        socket.connectToHostEncrypted(endpoint.host(),quint16(endpoint.port())); loop.exec(); received+=socket.readAll();
        const int status=received.left(received.indexOf("\r\n")).split(' ').value(1).toInt();
        if(status==0) qWarning() << "Fixture HTTPS request failed" << endpoint << socket.errorString() << received.left(40);
        if(status!=200) return {status,{}};
        bool authenticated=false; auto clear=ArbiterSync::open(received.mid(received.indexOf("\r\n\r\n")+4),key,context,&authenticated);
        if(!authenticated) return {};
        return {status,QJsonDocument::fromJson(clear).object()};
    }
private slots:
    void initTestCase() {
        const auto backend=qEnvironmentVariable("ARBITER_TEST_TLS_BACKEND");
        if(!backend.isEmpty()) QVERIFY2(QSslSocket::setActiveBackend(backend),qPrintable("TLS backend unavailable: "+backend));
    }
    void explicitPauseCancelsAnInFlightSyncUnlock() {
        class DeferredSecrets final : public MemorySecrets {
        public:
            QList<std::function<void()>> waiting;
            void readOptional(const QString &key,Callback callback) override {
                waiting.append([this,key,callback] { callback({},items.value(key)); });
            }
        };
        QTemporaryDir directory; MemorySecrets passwordStore,passkeyStore;DeferredSecrets syncStore;
        PasswordManager passwords(directory.path()+"/passwords","Fixture",&passwordStore);
        PasskeyVault passkeys(&passkeyStore,directory.path()+"/passkeys");
        CredentialSync sync(&passwords,&passkeys,directory.path()+"/sync",&syncStore,"127.0.0.1");
        sync.unlock();QTRY_VERIFY(!syncStore.waiting.isEmpty());QVERIFY(sync.busy());
        passkeys.lockSync();QVERIFY(!sync.busy());QVERIFY(!sync.unlocked());
        const auto waiting=syncStore.waiting;syncStore.waiting.clear();for(const auto &callback:waiting)callback();
        QVERIFY(!sync.unlocked());QVERIFY(sync.address().isEmpty());
        QVERIFY(!QFile::exists(directory.path()+"/sync/state-v1.enc"));
    }
    void passkeyTransfersWithoutChangingItsCredentialOrSigningKey() {
        QTemporaryDir a,b; MemorySecrets storeA,storeB;
        TestDeviceAuth auth;PasskeyVault source(&storeA,a.path(),nullptr,&auth),destination(&storeB,b.path(),nullptr,&auth);
        QCborMap rp,user,algorithm,request;
        rp.insert(QStringLiteral("id"),QStringLiteral("example.com")); user.insert(QStringLiteral("id"),QByteArray("fixture-user")); user.insert(QStringLiteral("name"),QStringLiteral("alice"));
        algorithm.insert(QStringLiteral("type"),QStringLiteral("public-key")); algorithm.insert(QStringLiteral("alg"),-7);
        request.insert(1,QByteArray(32,'c')); request.insert(2,rp); request.insert(3,user); request.insert(4,QCborArray{algorithm});
        int status=-1; source.perform(true,QCborValue(request).toCbor(),{},[&](int value,const QByteArray &) { status=value; }); QCOMPARE(status,0);
        QJsonArray exported; QString error;
        source.unlockForSync([&](const QString &e,const QJsonArray &) { error=e; });
        source.exportForSync([&](const QString &e,const QJsonArray &records) { error=e;exported=records; });
        QTRY_VERIFY(exported.size()==1 || !error.isEmpty()); QVERIFY2(error.isEmpty(),qPrintable(error)); QCOMPARE(exported.size(),1);
        destination.unlockForSync([&](const QString &e,const QJsonArray &) { error=e; }); QVERIFY2(error.isEmpty(),qPrintable(error));
        auto ineligible=exported.first().toObject(); ineligible.insert("backupEligible",false);
        bool rejected=false; destination.upsertFromSync(ineligible,[&](const QString &e) { rejected=!e.isEmpty(); });
        QTRY_VERIFY(rejected);
        bool imported=false; destination.upsertFromSync(exported.first().toObject(),[&](const QString &e) { error=e; imported=true; });
        QTRY_VERIFY(imported); QVERIFY2(error.isEmpty(),qPrintable(error));
        QJsonArray again; destination.exportForSync([&](const QString &e,const QJsonArray &records) { error=e; again=records; });
        QTRY_COMPARE(again.size(),1); QCOMPARE(again,exported);
        const auto id=QByteArray::fromBase64(exported.first().toObject().value("credentialId").toString().toLatin1(),QByteArray::Base64UrlEncoding);
        QCborMap assertion; assertion.insert(1,QStringLiteral("example.com")); assertion.insert(2,QByteArray(32,'d'));
        destination.perform(false,QCborValue(assertion).toCbor(),id,[&](int value,const QByteArray &bytes) { status=value; QVERIFY(!bytes.isEmpty()); }); QCOMPARE(status,0);
        destination.lockSync(); QVERIFY(!destination.syncReady());
        destination.exportForSync([&](const QString &e,const QJsonArray &) { error=e; }); QVERIFY(!error.isEmpty());
    }
    void threeComputersConvergeWithOfflineConflictsAndDeletion() {
        struct Computer {
            QTemporaryDir directory;
            MemorySecrets passwordStore,passkeyStore,syncStore;
            PasswordManager passwords;
            PasskeyVault passkeys;
            CredentialSync sync;
            Computer():passwords(directory.path()+"/passwords","Fixture",&passwordStore),
                passkeys(&passkeyStore,directory.path()+"/passkeys"),
                sync(&passwords,&passkeys,directory.path()+"/sync",&syncStore,"127.0.0.1") {
            }
        } a, b, c;
        const auto url=QUrl("https://three.example");
        const auto id=ArbiterSync::passwordId(url.toString(),"user");
        a.passwords.save(url,"user","original");
        for(auto computer:{&a,&b,&c}) { computer->sync.unlock(); QTRY_VERIFY_WITH_TIMEOUT(!computer->sync.busy(),15000); QVERIFY2(computer->sync.unlocked(),qPrintable(computer->sync.error())); }
        a.sync.pair(); b.sync.reviewPairing(a.sync.pairingCode()); QVERIFY(!b.sync.pendingDevice().isEmpty()); b.sync.confirmPairing();
        QTRY_VERIFY_WITH_TIMEOUT(!b.sync.busy(),15000); QVERIFY2(b.sync.error().isEmpty(),qPrintable(b.sync.error())); QCOMPARE(b.passwords.entries().size(),1);
        a.passwords.save(url,"user","remote change");
        b.sync.sync();
        QTimer::singleShot(0,&b.sync,[&] {b.passwords.save(url,"user","edit during sync");});
        QTRY_VERIFY_WITH_TIMEOUT(!b.sync.busy(),15000); QCOMPARE(b.sync.conflicts(),1);
        QJsonArray current;
        b.passwords.exportAfterVerification([&](const QString &,const QJsonArray &values){current=values;});
        QTRY_COMPARE(current.size(),1); QCOMPARE(current.first().toObject().value("password").toString(),"edit during sync");
        const auto inFlight=b.sync.conflictChoices().first().toMap();
        b.sync.resolveConflict(id,inFlight.value("version").toString(),inFlight.value("generation").toString());
        QTRY_VERIFY_WITH_TIMEOUT(!b.sync.busy(),15000); QCOMPARE(b.sync.conflicts(),0);
        b.sync.pair(); c.sync.reviewPairing(b.sync.pairingCode()); c.sync.confirmPairing();
        QTRY_VERIFY_WITH_TIMEOUT(!c.sync.busy(),15000); QVERIFY2(c.sync.error().isEmpty(),qPrintable(c.sync.error())); QCOMPARE(c.passwords.entries().size(),1);
        // C has never paired with A. Changes travel through B.
        b.sync.lock();
        a.passwords.save(url,"user","edit on A");
        c.passwords.save(url,"user","edit on C");
        b.sync.unlock(); QTRY_VERIFY_WITH_TIMEOUT(!b.sync.busy(),15000); QVERIFY(b.sync.unlocked());
        c.sync.sync(); QTRY_VERIFY_WITH_TIMEOUT(!c.sync.busy(),15000); QVERIFY2(c.sync.error().isEmpty(),qPrintable(c.sync.error()));
        b.sync.sync(); QTRY_VERIFY_WITH_TIMEOUT(!b.sync.busy(),15000); QVERIFY2(b.sync.error().isEmpty(),qPrintable(b.sync.error()));
        QCOMPARE(a.sync.conflicts(),1); QCOMPARE(b.sync.conflicts(),1);
        c.sync.sync(); QTRY_VERIFY_WITH_TIMEOUT(!c.sync.busy(),15000); QCOMPARE(c.sync.conflicts(),1);
        const auto choice=b.sync.conflictChoices().first().toMap();
        QVERIFY(!choice.contains("password"));
        b.sync.resolveConflict(id,choice.value("version").toString(),choice.value("generation").toString());
        QTRY_VERIFY_WITH_TIMEOUT(!b.sync.busy(),15000); QVERIFY2(b.sync.error().isEmpty(),qPrintable(b.sync.error()));
        c.sync.sync(); QTRY_VERIFY_WITH_TIMEOUT(!c.sync.busy(),15000);
        for(auto computer:{&a,&b,&c}) QCOMPARE(computer->sync.conflicts(),0);
        // A plain old offline copy cannot undo a causally newer delete.
        a.sync.lock();
        c.passwords.remove(c.passwords.entries().first().toMap().value("id").toString());
        c.sync.sync(); QTRY_VERIFY_WITH_TIMEOUT(!c.sync.busy(),15000); QVERIFY(b.passwords.entries().isEmpty());
        a.sync.unlock(); QTRY_VERIFY_WITH_TIMEOUT(!a.sync.busy(),15000);
        b.sync.sync(); QTRY_VERIFY_WITH_TIMEOUT(!b.sync.busy(),15000);
        QVERIFY2(a.passwords.entries().isEmpty(),qPrintable(b.sync.error()));
        c.sync.sync(); QTRY_VERIFY_WITH_TIMEOUT(!c.sync.busy(),15000); QVERIFY(c.passwords.entries().isEmpty());
        // Disconnecting a connection preserves all credentials on its peers.
        b.passwords.save(QUrl("https://retained.example"),"user","retained");
        b.sync.sync(); QTRY_VERIFY_WITH_TIMEOUT(!b.sync.busy(),15000);
        c.sync.sync(); QTRY_VERIFY_WITH_TIMEOUT(!c.sync.busy(),15000);
        QString outgoing;
        for(const auto &v:b.sync.devices()) if(v.toMap().value("id").toString().startsWith("out:")) outgoing=v.toMap().value("id").toString();
        QVERIFY(!outgoing.isEmpty()); b.sync.revoke(outgoing); QCOMPARE(b.sync.devices().size(),1);
        for(auto computer:{&a,&b,&c}) QCOMPARE(computer->passwords.entries().size(),1);
    }
    void pinnedLocalPairingSyncRevocationAndTombstones() {
        QTemporaryDir directory; MemorySecrets passwordsStore,passkeysStore,syncStore;
        PasswordManager passwords(directory.path()+"/passwords","Fixture",&passwordsStore);
        PasskeyVault passkeys(&passkeysStore,directory.path()+"/passkeys");
        passwords.save(QUrl("https://example.com"),"fixture-user","fixture-original");
        CredentialSync sync(&passwords,&passkeys,directory.path()+"/sync",&syncStore,"127.0.0.1");
        auto *tls=sync.findChild<QSslServer *>(); QVERIFY(tls);
        connect(tls,&QSslServer::errorOccurred,this,[](QSslSocket *s,QAbstractSocket::SocketError) { qWarning() << "Fixture server TLS error" << s->errorString(); });
        connect(tls,&QSslServer::alertSent,this,[](QSslSocket *,QSsl::AlertLevel,QSsl::AlertType,const QString &message) { qWarning() << "Fixture TLS alert" << message; });
        sync.unlock(); QTRY_VERIFY(!sync.busy()); QVERIFY2(sync.unlocked(),qPrintable(sync.error()));
        sync.pair(); QVERIFY(!sync.pairingImage().isEmpty()); const auto pairing=info(sync);
        const auto wireKey=QByteArray::fromBase64(pairing.value("key").toString().toLatin1(),QByteArray::Base64UrlEncoding);
        QVERIFY(wireKey!=syncStore.items.value("sync-v1-master"));
        QFile encryptedState(directory.path()+"/sync/state-v1.enc"); QVERIFY(encryptedState.open(QIODevice::ReadOnly));
        bool authenticated=false; ArbiterSync::open(encryptedState.readAll(),wireKey,"ArbiterDesktopSync/v1",&authenticated); QVERIFY(!authenticated);
        // Windows cannot atomically replace a file while this read handle is open.
        encryptedState.close();
        const QString phone="00000000-0000-4000-8000-000000000009";
        const QJsonObject identity{{"deviceId",phone},{"name","Fixture phone"}};
        QCOMPARE(post(pairing,"/v1/pair",pairing.value("code").toString(),identity,true).status,400);
        const auto join=post(pairing,"/v1/pair",pairing.value("code").toString(),identity); QCOMPARE(join.status,200);
        const auto token=join.data.value("token").toString(); QVERIFY(!token.isEmpty()); QVERIFY(sync.pairingCode().isEmpty());
        QCOMPARE(post(pairing,"/v1/pair",pairing.value("code").toString(),identity).status,401);
        const auto first=post(pairing,"/v1/sync",token,{{"v",1},{"records",QJsonObject{}}}); QCOMPARE(first.status,200);
        ArbiterSync::State phoneState; QString error; QVERIFY(ArbiterSync::State::parse(first.data,&phoneState,&error));
        const auto id=ArbiterSync::passwordId("https://example.com","fixture-user");
        QVERIFY(phoneState.update(id,phone,{{"kind","password"},{"origin","https://example.com"},{"username","fixture-user"},{"password","fixture-edited"}},false,&error));
        QCOMPARE(post(pairing,"/v1/sync",token,phoneState.json()).status,200);
        QJsonArray exported; passwords.exportAfterVerification([&](const QString &e,const QJsonArray &records) { error=e; exported=records; });
        QTRY_COMPARE(exported.size(),1); QCOMPARE(exported.first().toObject().value("password").toString(),"fixture-edited");
        QVERIFY(phoneState.update(id,phone,{},true,&error)); QCOMPARE(post(pairing,"/v1/sync",token,phoneState.json()).status,200); QVERIFY(passwords.entries().isEmpty());
        sync.revoke(phone); QCOMPARE(post(pairing,"/v1/sync",token,phoneState.json()).status,401);
        sync.lock(); QVERIFY(!sync.unlocked());
    }
};
QTEST_GUILESS_MAIN(CredentialTransferTest)
#include "credentialtransfer_test.moc"
