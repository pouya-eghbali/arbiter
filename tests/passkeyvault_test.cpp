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
    bool failWrites = false;
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
namespace {
const QString phrase = QStringLiteral("test vault passphrase only");
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
QByteArray perform(PasskeyVault &vault, bool make, const QCborMap &request, const QByteArray &id, int expected, const QString &pass = phrase) {
    QByteArray result; bool called = false;
    vault.perform(make, encode(request), id, pass, [&](int status, const QByteArray &response) {
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
        QTemporaryDir dir; MemoryStore store; PasskeyVault vault(&store, dir.path());
        vault.setup(phrase, phrase); QVERIFY2(vault.configured(), qPrintable(vault.error()));
        auto created = QCborValue::fromCbor(perform(vault, true, makeRequest(), {}, 0)).toMap();
        QCOMPARE(created.value(1).toString(), QStringLiteral("none"));
        auto auth = created.value(2).toByteArray(); QCOMPARE(static_cast<unsigned char>(auth.at(32)), 0x45);
        QCOMPARE(auth.mid(33, 4), QByteArray(4, char(0))); QCOMPARE(auth.mid(37, 16), QByteArray(16, char(0)));
        auto entries = vault.credentials(QStringLiteral("example.com")); QCOMPARE(entries.size(), 1);
        auto id = entries.first().toMap().value(QStringLiteral("id")).toByteArray(); QCOMPARE(id.size(), 32); QCOMPARE(auth.mid(55, 32), id);
        auto assertion = QCborValue::fromCbor(perform(vault, false, getRequest(), id, 0)).toMap();
        QCOMPARE(assertion.value(2).toByteArray().size(), 37); QCOMPARE(assertion.value(2).toByteArray().at(32), char(5));
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
        PasskeyVault reopened(&store, dir.path()); QVERIFY(reopened.configured());
        QVERIFY(!perform(reopened, false, getRequest(), id, 0).isEmpty());
    }
    void wrongPassphraseAndLockedStorage() {
        QTemporaryDir dir; MemoryStore store; PasskeyVault vault(&store, dir.path());
        vault.setup(phrase, phrase); QVERIFY(vault.configured());
        QVERIFY(perform(vault, true, makeRequest(), {}, 1, QStringLiteral("wrong password")).isEmpty());
        QVERIFY(perform(vault, true, makeRequest(), {}, 1).isEmpty()); // backoff applies even to correct secret
        QVERIFY(vault.credentials(QStringLiteral("example.com")).isEmpty()); QCOMPARE(store.items.size(), 1);
        QTemporaryDir lockedDir; store.failWrites = true; PasskeyVault locked(&store, lockedDir.path());
        locked.setup(phrase, phrase); QVERIFY(!locked.configured()); QVERIFY(!locked.error().isEmpty());
        store.failWrites = false; locked.setup(phrase, phrase); QVERIFY(!locked.configured()); // no silent reset of reserved identity
    }
    void cancellationAndFailedCredentialWrites() {
        QTemporaryDir dir; MemoryStore store; PasskeyVault vault(&store, dir.path());
        vault.setup(phrase, phrase); QVERIFY(vault.configured());
        bool current = true; int status = -1;
        store.afterWrite = [&] { current = false; };
        vault.perform(true, encode(makeRequest()), {}, phrase, [&](int code, const QByteArray &response) {
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
        vault.perform(false, encode(getRequest()), id, phrase, [&](int code, const QByteArray &response) {
            status = code; QVERIFY(response.isEmpty());
        }, [&] { return current; });
        QCOMPARE(status, 1); QCOMPARE(reads, 2);
    }
    void setupCommitFailureIsNotConfigured() {
        QTemporaryDir dir; MemoryStore store; PasskeyVault vault(&store, dir.path());
        store.afterWrite = [&] {
            const auto path = dir.filePath(QStringLiteral("catalog.cbor"));
            QVERIFY(QFile::remove(path));
            QVERIFY(QDir().mkdir(path)); // Prevent the final atomic catalog replacement.
        };
        vault.setup(phrase, phrase);
        QVERIFY(!vault.configured()); QVERIFY(!vault.error().isEmpty());
    }
    void tamperedMetadata() {
        QTemporaryDir dir; MemoryStore store; PasskeyVault vault(&store, dir.path());
        vault.setup(phrase, phrase); perform(vault, true, makeRequest(), {}, 0);
        auto id = vault.credentials(QStringLiteral("example.com")).first().toMap().value(QStringLiteral("id")).toByteArray();
        QFile file(dir.filePath(QStringLiteral("catalog.cbor"))); QVERIFY(file.open(QIODevice::ReadOnly));
        auto index = QCborValue::fromCbor(file.readAll()).toMap(); file.close();
        auto entries = index.value(3).toArray(); auto meta = entries.at(0).toMap(); meta.insert(1, QStringLiteral("evil.example")); entries[0] = meta; index.insert(3, entries);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate)); file.write(encode(index)); file.close();
        PasskeyVault tampered(&store, dir.path());
        QVERIFY(perform(tampered, false, getRequest(QStringLiteral("evil.example")), id, 1).isEmpty());
    }
    void actualCredentialStore() {
        if (!qEnvironmentVariableIsSet("ARBITER_TEST_KEYCHAIN"))
            QSKIP("Opt in to disposable OS credential-store tests with ARBITER_TEST_KEYCHAIN=1.");
        QTemporaryDir dir; KeychainSecretStore store(nullptr, QStringLiteral("Arbiter Passkeys")); PasskeyVault vault(&store, dir.path());
        QStringList keys;
        const auto cleanup = qScopeGuard([&] {
            for (const auto &key : keys) {
                QEventLoop loop; QTimer timeout; timeout.setSingleShot(true);
                connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
                bool done = false; QString error;
                store.remove(key, [&](const QString &e, const QByteArray &) { error = e; done = true; loop.quit(); });
                timeout.start(15000); if (!done) loop.exec();
                if (!done || !error.isEmpty()) QTest::qFail("Could not clean a disposable passkey test item", __FILE__, __LINE__);
            }
        });
        vault.setup(phrase, phrase);
        QFile file(dir.filePath(QStringLiteral("catalog.cbor"))); QVERIFY(file.open(QIODevice::ReadOnly));
        const auto index = QCborValue::fromCbor(file.readAll()).toMap(); file.close();
        auto prefix = QStringLiteral("com.arbiter.browser/passkeys/v1/%1/").arg(QString::fromLatin1(index.value(1).toByteArray().toHex()));
        keys.append(prefix);
        QTRY_VERIFY_WITH_TIMEOUT(!vault.busy(), 15000); QVERIFY2(vault.configured(), qPrintable(vault.error()));
        int status = -1; QByteArray response;
        vault.perform(true, encode(makeRequest()), {}, phrase, [&](int code, const QByteArray &bytes) { status = code; response = bytes; });
        QTRY_VERIFY_WITH_TIMEOUT(status != -1, 15000); QCOMPARE(status, 0); QVERIFY(!response.isEmpty());
        const auto id = vault.credentials(QStringLiteral("example.com")).first().toMap().value(QStringLiteral("id")).toByteArray();
        keys.append(prefix + QString::fromLatin1(id.toHex()));
        status = -1;
        vault.perform(false, encode(getRequest()), id, phrase, [&](int code, const QByteArray &bytes) { status = code; response = bytes; });
        QTRY_VERIFY_WITH_TIMEOUT(status != -1, 15000); QCOMPARE(status, 0);
        QVERIFY(!QCborValue::fromCbor(response).toMap().value(3).toByteArray().isEmpty());
    }
};
#ifdef Q_OS_MACOS
int main(int argc, char **argv) {
    QApplication app(argc, argv); PasskeyVaultTest test;
    QTimer::singleShot(0, &app, [&] { app.exit(QTest::qExec(&test, argc, argv)); });
    return app.exec();
}
#else
QTEST_MAIN(PasskeyVaultTest)
#endif
#include "passkeyvault_test.moc"
