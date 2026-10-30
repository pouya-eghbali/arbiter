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
    const QString phrase=QStringLiteral("fixture vault passphrase only");
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
    void passkeyTransfersWithoutChangingItsCredentialOrSigningKey() {
        QTemporaryDir a,b; MemorySecrets storeA,storeB;
        PasskeyVault source(&storeA,a.path()),destination(&storeB,b.path()); source.setup(phrase,phrase); destination.setup(phrase,phrase);
        QCborMap rp,user,algorithm,request;
        rp.insert(QStringLiteral("id"),QStringLiteral("example.com")); user.insert(QStringLiteral("id"),QByteArray("fixture-user")); user.insert(QStringLiteral("name"),QStringLiteral("alice"));
        algorithm.insert(QStringLiteral("type"),QStringLiteral("public-key")); algorithm.insert(QStringLiteral("alg"),-7);
        request.insert(1,QByteArray(32,'c')); request.insert(2,rp); request.insert(3,user); request.insert(4,QCborArray{algorithm});
        int status=-1; source.perform(true,QCborValue(request).toCbor(),{},phrase,[&](int value,const QByteArray &) { status=value; }); QCOMPARE(status,0);
        QJsonArray exported; QString error;
        source.unlockForSync(phrase,false,[&](const QString &e,const QJsonArray &records) { error=e; exported=records; });
        QTRY_VERIFY(exported.size()==1 || !error.isEmpty()); QVERIFY2(error.isEmpty(),qPrintable(error)); QCOMPARE(exported.size(),1);
        destination.unlockForSync(phrase,false,[&](const QString &e,const QJsonArray &) { error=e; }); QVERIFY2(error.isEmpty(),qPrintable(error));
        auto ineligible=exported.first().toObject(); ineligible.insert("backupEligible",false);
        bool rejected=false; destination.upsertFromSync(ineligible,[&](const QString &e) { rejected=!e.isEmpty(); });
        QTRY_VERIFY(rejected);
        bool imported=false; destination.upsertFromSync(exported.first().toObject(),[&](const QString &e) { error=e; imported=true; });
        QTRY_VERIFY(imported); QVERIFY2(error.isEmpty(),qPrintable(error));
        QJsonArray again; destination.exportForSync([&](const QString &e,const QJsonArray &records) { error=e; again=records; });
        QTRY_COMPARE(again.size(),1); QCOMPARE(again,exported);
        const auto id=QByteArray::fromBase64(exported.first().toObject().value("credentialId").toString().toLatin1(),QByteArray::Base64UrlEncoding);
        QCborMap assertion; assertion.insert(1,QStringLiteral("example.com")); assertion.insert(2,QByteArray(32,'d'));
        destination.perform(false,QCborValue(assertion).toCbor(),id,phrase,[&](int value,const QByteArray &bytes) { status=value; QVERIFY(!bytes.isEmpty()); }); QCOMPARE(status,0);
        destination.lockSync(); QVERIFY(!destination.syncReady());
        destination.exportForSync([&](const QString &e,const QJsonArray &) { error=e; }); QVERIFY(!error.isEmpty());
    }
    void pinnedLocalPairingSyncRevocationAndTombstones() {
        QTemporaryDir directory; MemorySecrets passwordsStore,passkeysStore,syncStore;
        PasswordManager passwords(directory.path()+"/passwords","Fixture",&passwordsStore);
        PasskeyVault passkeys(&passkeysStore,directory.path()+"/passkeys"); passkeys.setup(phrase,phrase);
        passwords.save(QUrl("https://example.com"),"fixture-user","fixture-original");
        CredentialSync sync(&passwords,&passkeys,directory.path()+"/sync",&syncStore,"127.0.0.1");
        auto *tls=sync.findChild<QSslServer *>(); QVERIFY(tls);
        connect(tls,&QSslServer::errorOccurred,this,[](QSslSocket *s,QAbstractSocket::SocketError) { qWarning() << "Fixture server TLS error" << s->errorString(); });
        connect(tls,&QSslServer::alertSent,this,[](QSslSocket *,QSsl::AlertLevel,QSsl::AlertType,const QString &message) { qWarning() << "Fixture TLS alert" << message; });
        sync.unlock(phrase); QTRY_VERIFY(!sync.busy()); QVERIFY2(sync.unlocked(),qPrintable(sync.error()));
        sync.pair(); QVERIFY(!sync.pairingImage().isEmpty()); const auto pairing=info(sync);
        const auto wireKey=QByteArray::fromBase64(pairing.value("key").toString().toLatin1(),QByteArray::Base64UrlEncoding);
        QVERIFY(wireKey!=syncStore.items.value("sync-v1-master"));
        QFile encryptedState(directory.path()+"/sync/state-v1.enc"); QVERIFY(encryptedState.open(QIODevice::ReadOnly));
        bool authenticated=false; ArbiterSync::open(encryptedState.readAll(),wireKey,"ArbiterDesktopSync/v1",&authenticated); QVERIFY(!authenticated);
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
