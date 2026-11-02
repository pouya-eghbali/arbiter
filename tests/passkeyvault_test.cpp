#include "../src/passkeyvault.h"
#include "../src/passkeycrypto.h"
#include <QTest>
#include <QTemporaryDir>
#include <QFile>
#include <QCborArray>
#include <QScopeGuard>
#include <QApplication>
#include <QTimer>
#include <QEventLoop>
#include <QDir>
#include <openssl/evp.h>
#include <openssl/x509.h>

class MemoryStore : public SecretStore {
public:
    QHash<QString, QByteArray> items;
    bool failWrites = false, denySilent = false;
    void readOptional(const QString &key, Callback cb) override { cb({},items.value(key)); }
    int silentReads = 0;
    void readSilently(const QString &key, Callback cb) override {
        ++silentReads;
        if (denySilent) cb(QStringLiteral("approval required"), {}); else read(key,std::move(cb));
    }
    std::function<void()> afterWrite, afterRead;
    void read(const QString &key, Callback cb) override {
        if (afterRead) afterRead();
        cb(items.contains(key) ? QString() : QStringLiteral("missing"), items.value(key));
    }
    void write(const QString &key, const QByteArray &value, Callback cb) override {
        if (!failWrites) items.insert(key, value);
        if (afterWrite) afterWrite();
        cb(failWrites ? QStringLiteral("locked") : QString(), {});
    }
    void remove(const QString &key, Callback cb) override { items.remove(key); cb({}, {}); }
};
class FakeBiometrics : public BiometricAuth {
public:
    FakeBiometrics() { refresh(); }
    bool automatic=true, success=true;
    int prompts=0;quint64 pending=0;
    bool foregroundRequired() const override { return false; }
    QString name() const override { return "Test device verification"; }
    void refresh() override { availability(true,{}); }
    void respond(bool ok) { complete(pending,ok,ok ? QString() : QString("Not verified")); }
protected:
    void startNative(const QString &,quint64 token) override {++prompts;pending=token;if(automatic)respond(success);}
    void cancelNative() override {}
};
namespace {
QByteArray encode(const QCborMap &map) { return QCborValue(map).toCbor(); }
QCborMap makeRequest(const QString &rp = QStringLiteral("example.com")) {
    QCborMap request, relyingParty, user, algorithm;
    relyingParty.insert(QStringLiteral("id"), rp);
    user.insert(QStringLiteral("id"), QByteArray("user-id")); user.insert(QStringLiteral("name"), QStringLiteral("alice"));
    algorithm.insert(QStringLiteral("type"), QStringLiteral("public-key")); algorithm.insert(QStringLiteral("alg"), -7);
    request.insert(1, QByteArray(32, 'c')); request.insert(2, relyingParty); request.insert(3, user); request.insert(4, QCborArray{algorithm});
    return request;
}
QCborMap getRequest(const QString &rp = QStringLiteral("example.com")) {
    QCborMap request; request.insert(1, rp); request.insert(2, QByteArray(32, 'd')); return request;
}
QByteArray perform(PasskeyVault &vault, bool make, const QCborMap &request, const QByteArray &id, int expected) {
    QByteArray result; bool called = false;
    vault.perform(make, encode(request), id, [&](int status, const QByteArray &response) {
        called = true; if (status != expected) QTest::qFail(qPrintable(QStringLiteral("status %1, expected %2; %3").arg(status).arg(expected).arg(vault.error())), __FILE__, __LINE__);
        result = response;
    });
    if (!called) QTest::qFail("callback was not called", __FILE__, __LINE__);
    return result;
}
}
class PasskeyVaultTest : public QObject {
    Q_OBJECT
private slots:
    void lockedKeyClearsAndRejectsInvalidInput() {
        LockedKey key; QVERIFY(!key.present()); QVERIFY(key.copy().isEmpty());
        const auto value=PasskeyCrypto::random(32); QVERIFY(key.set(value)); QCOMPARE(key.copy(),value);
        key.clear(); QVERIFY(!key.present()); QVERIFY(key.copy().isEmpty());
        QVERIFY(!key.set("short")); QVERIFY(!key.present());
    }
    void authenticatedEncryption() {
        auto key = PasskeyCrypto::random(32); auto sealed = PasskeyCrypto::seal("secret", key, "rp+credential");
        QCOMPARE(PasskeyCrypto::open(sealed, key, "rp+credential"), QByteArray("secret"));
        QVERIFY(PasskeyCrypto::open(sealed, key, "different-rp").isEmpty());
        sealed[14] ^= 1; QVERIFY(PasskeyCrypto::open(sealed, key, "rp+credential").isEmpty());
        QVERIFY(PasskeyCrypto::open(sealed, PasskeyCrypto::random(32), "rp+credential").isEmpty());
    }
    void signature() {
        auto key = PasskeyCrypto::generate(); QVERIFY(!key.der.isEmpty());
        auto cose = QCborValue::fromCbor(key.cose).toMap(); QCOMPARE(cose.value(3).toInteger(), -7);
        QCOMPARE(cose.value(-2).toByteArray().size(), 32); QCOMPARE(cose.value(-3).toByteArray().size(), 32);
        const auto *cursor = reinterpret_cast<const unsigned char *>(key.der.constData());
        auto *native = d2i_AutoPrivateKey(nullptr, &cursor, key.der.size()); QVERIFY(native);
        auto *ctx = EVP_MD_CTX_new(); QVERIFY(ctx);
        auto sig = PasskeyCrypto::sign(key.der, "challenge"); QVERIFY(!sig.isEmpty());
        QCOMPARE(EVP_DigestVerifyInit(ctx, nullptr, EVP_sha256(), nullptr, native), 1);
        QCOMPARE(EVP_DigestVerify(ctx, reinterpret_cast<const unsigned char *>(sig.constData()), sig.size(), reinterpret_cast<const unsigned char *>("challenge"), 9), 1);
        QCOMPARE(EVP_DigestVerify(ctx, reinterpret_cast<const unsigned char *>(sig.constData()), sig.size(), reinterpret_cast<const unsigned char *>("wrong-challenge"), 15), 0);
        EVP_MD_CTX_free(ctx); EVP_PKEY_free(native);
    }
    void registrationAndAssertion() {
        QTemporaryDir dir; MemoryStore store; FakeBiometrics auth; PasskeyVault vault(&store, dir.path(),nullptr,&auth);

        auto created = QCborValue::fromCbor(perform(vault, true, makeRequest(), {}, 0)).toMap();
        QCOMPARE(created.value(1).toString(), QStringLiteral("none"));
        auto authData = created.value(2).toByteArray(); QCOMPARE(static_cast<unsigned char>(authData.at(32)), 0x4d);
        QCOMPARE(authData.mid(33, 4), QByteArray(4, char(0))); QCOMPARE(authData.mid(37, 16), QByteArray(16, char(0)));
        auto entries = vault.credentials(QStringLiteral("example.com")); QCOMPARE(entries.size(), 1);
        auto id = entries.first().toMap().value(QStringLiteral("id")).toByteArray(); QCOMPARE(id.size(), 32); QCOMPARE(authData.mid(55, 32), id);
        auto assertion = QCborValue::fromCbor(perform(vault, false, getRequest(), id, 0)).toMap();
        QCOMPARE(assertion.value(2).toByteArray().size(), 37); QCOMPARE(assertion.value(2).toByteArray().at(32), char(0x0d));
        QVERIFY(!assertion.value(3).toByteArray().isEmpty());
        QCOMPARE(assertion.value(4).toMap().value(QStringLiteral("id")).toByteArray(), QByteArray("user-id"));
        QVERIFY(vault.credentials(QStringLiteral("evil.example")).isEmpty());
        QVERIFY(perform(vault, false, getRequest(QStringLiteral("evil.example")), id, 4).isEmpty());
        QCborMap descriptor; descriptor.insert(QStringLiteral("type"), QStringLiteral("public-key")); descriptor.insert(QStringLiteral("id"), id);
        auto excluded = makeRequest(); excluded.insert(5, QCborArray{descriptor}); QVERIFY(perform(vault, true, excluded, {}, 2).isEmpty());
        auto noAlgorithm = makeRequest(); QCborMap alg; alg.insert(QStringLiteral("alg"), -257); noAlgorithm.insert(4, QCborArray{alg});
        QVERIFY(perform(vault, true, noAlgorithm, {}, 3).isEmpty());
        auto deny = getRequest(); descriptor.insert(QStringLiteral("id"), QByteArray(32, 'x')); deny.insert(3, QCborArray{descriptor});
        QVERIFY(perform(vault, false, deny, id, 4).isEmpty());
        PasskeyVault reopened(&store, dir.path(),nullptr,&auth); QVERIFY(reopened.configured());
        QVERIFY(!perform(reopened, false, getRequest(), id, 0).isEmpty());
    }
    void cancellationAndFailedCredentialWrites() {
        QTemporaryDir dir; MemoryStore store; FakeBiometrics auth; PasskeyVault vault(&store, dir.path(),nullptr,&auth);

        vault.unlockForSync([](const QString &e,const QJsonArray &){QVERIFY(e.isEmpty());});
        bool current = true; int status = -1;
        store.afterWrite = [&] { current = false; };
        vault.perform(true, encode(makeRequest()), {}, [&](int code, const QByteArray &response) {
            status = code; QVERIFY(response.isEmpty());
        }, [&] { return current; });
        QCOMPARE(status, 1); QCOMPARE(store.items.size(), 1);
        QVERIFY(vault.credentials(QStringLiteral("example.com")).isEmpty());
        store.afterWrite = {}; store.failWrites = true;
        QVERIFY(perform(vault, true, makeRequest(), {}, 1).isEmpty());
        QCOMPARE(store.items.size(), 1); QVERIFY(vault.credentials(QStringLiteral("example.com")).isEmpty());
        store.failWrites = false;
        QVERIFY(!perform(vault, true, makeRequest(), {}, 0).isEmpty());
        auto id = vault.credentials(QStringLiteral("example.com")).first().toMap().value(QStringLiteral("id")).toByteArray();
        int reads = 0; current = true;
        store.afterRead = [&] { if (++reads == 2) current = false; };
        vault.perform(false, encode(getRequest()), id, [&](int code, const QByteArray &response) {
            status = code; QVERIFY(response.isEmpty());
        }, [&] { return current; });
        QCOMPARE(status, 1); QCOMPARE(reads, 2);
    }
    void tamperedMetadata() {
        QTemporaryDir dir; MemoryStore store; FakeBiometrics auth; PasskeyVault vault(&store, dir.path(),nullptr,&auth);
        perform(vault, true, makeRequest(), {}, 0);
        auto id = vault.credentials(QStringLiteral("example.com")).first().toMap().value(QStringLiteral("id")).toByteArray();
        QFile file(dir.filePath(QStringLiteral("catalog.cbor"))); QVERIFY(file.open(QIODevice::ReadOnly));
        auto index = QCborValue::fromCbor(file.readAll()).toMap(); file.close();
        auto entries = index.value(3).toArray(); auto meta = entries.at(0).toMap(); meta.insert(1, QStringLiteral("evil.example")); entries[0] = meta; index.insert(3, entries);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate)); file.write(encode(index)); file.close();
        PasskeyVault tampered(&store, dir.path(),nullptr,&auth);
        QVERIFY(perform(tampered, false, getRequest(QStringLiteral("evil.example")), id, 1).isEmpty());
    }
    void failedDeviceVerificationAndCancellationCannotSign() {
        QTemporaryDir dir;MemoryStore store;FakeBiometrics auth;
        PasskeyVault vault(&store,dir.path(),nullptr,&auth);
        auth.success=false;QVERIFY(perform(vault,true,makeRequest(),{},1).isEmpty());QVERIFY(store.items.isEmpty());
        auth.success=true;auth.automatic=false;int status=-1,callbacks=0;
        vault.perform(true,encode(makeRequest()),{},[&](int code,const QByteArray &response){++callbacks;status=code;QVERIFY(response.isEmpty());});
        QVERIFY(vault.busy());vault.lockBiometrics();QCOMPARE(status,1);QCOMPARE(callbacks,1);
        auth.respond(true);QCOMPARE(callbacks,1);QVERIFY(store.items.isEmpty());
    }
    void missingRootKeyDoesNotResetCredentials() {
        QTemporaryDir dir;MemoryStore store;FakeBiometrics auth;
        PasskeyVault vault(&store,dir.path(),nullptr,&auth);perform(vault,true,makeRequest(),{},0);
        const auto id=vault.credentials("example.com").first().toMap().value("id").toByteArray();
        QString root;for(auto i=store.items.begin();i!=store.items.end();++i)if(i.key().startsWith("credential-passkey-root/"))root=i.key();
        QVERIFY(!root.isEmpty());store.items.remove(root);const auto remaining=store.items;
        QVERIFY(perform(vault,false,getRequest(),id,1).isEmpty());QCOMPARE(store.items,remaining);
    }
    void syncExportNeverFallsBackToInteractiveRead() {
        QTemporaryDir dir;MemoryStore store;FakeBiometrics auth;
        PasskeyVault vault(&store,dir.path(),nullptr,&auth);perform(vault,true,makeRequest(),{},0);
        vault.unlockForSync([](const QString &e,const QJsonArray &){QVERIFY(e.isEmpty());});
        store.denySilent=true;QString error;vault.exportForSync([&](const QString &e,const QJsonArray &){error=e;});
        QVERIFY(!error.isEmpty());QCOMPARE(store.silentReads,1);
    }
};
QTEST_MAIN(PasskeyVaultTest)
#include "passkeyvault_test.moc"
