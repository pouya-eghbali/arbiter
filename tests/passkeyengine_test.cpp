// This fixture automates consent only in the test process. Production never
// installs a virtual authenticator or exposes this provider to web content.
#include "../src/passkeyvault.h"
#include <QApplication>
#include <QCborMap>
#include <QCryptographicHash>
#include <QEventLoop>
#include <QTemporaryDir>
#include <QWebEngineView>
#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QWebEngineWebAuthUxRequest>
#include <QtWebEngineCore/qtwebenginecoreglobal.h>
#include <QtTest>
#include <openssl/evp.h>
#include <openssl/x509.h>

class TestStore final : public SecretStore {
public:
    QHash<QString, QByteArray> entries;
    void read(const QString &key, Callback cb) override { cb(entries.contains(key) ? QString() : QStringLiteral("missing"), entries.value(key)); }
    void write(const QString &key, const QByteArray &bytes, Callback cb) override { entries[key] = bytes; cb({}, {}); }
    void remove(const QString &key, Callback cb) override { entries.remove(key); cb({}, {}); }
};
class TestProvider final : public QObject {
    Q_OBJECT
    Q_PROPERTY(int abi READ abi CONSTANT)
    Q_PROPERTY(bool configured READ configured CONSTANT)
public:
    explicit TestProvider(PasskeyVault *vault) : vault(vault) {}
    PasskeyVault *vault;
    int abi() const { return 1; }
    bool configured() const { return vault->configured(); }
    Q_INVOKABLE QVariantList credentials(const QString &rp) const { return vault->credentials(rp); }
};
class PasskeyEngineTest : public QObject {
    Q_OBJECT
    QPointer<QObject> bridge;
    QPointer<QWebEngineWebAuthUxRequest> request;
    PasskeyVault *vault = nullptr;
    bool handledPayload = false;
    bool selectedLocal = false;
    int shown = 0;
    QVariant js(QWebEnginePage *page, const QString &script) {
        QEventLoop loop; QVariant result; QTimer timer; timer.setSingleShot(true);
        connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
        page->runJavaScript(script, [&](const QVariant &value) { result = value; loop.quit(); });
        timer.start(5000); loop.exec(); return result;
    }
private slots:
    void answer() {
        if (!bridge || handledPayload) return;
        if (!selectedLocal && bridge->property("localAvailable").toBool()) {
            selectedLocal = true;
            QTimer::singleShot(0, bridge, [guard = bridge] { if (guard) QMetaObject::invokeMethod(guard, "useLocal"); });
        }
        auto payload = bridge->property("payload").toByteArray(); if (payload.isEmpty()) return;
        handledPayload = true;
        auto entries = vault->credentials(QStringLiteral("example.test"));
        QByteArray id = entries.isEmpty() ? QByteArray() : entries.first().toMap().value(QStringLiteral("id")).toByteArray();
        vault->perform(bridge->property("registration").toBool(), payload, id, QStringLiteral("synthetic vault passphrase"),
            [guard = bridge](int status, const QByteArray &response) {
                if (guard) QMetaObject::invokeMethod(guard, "complete", Q_ARG(QByteArray, response), Q_ARG(int, status));
            });
    }
    void registrationAssertionAndRpValidation() {
#ifndef QT_ARBITER_WEBAUTHN_ABI
        QSKIP("Requires the patched engine SDK; stock Qt cannot exercise this boundary.");
#else
        QTemporaryDir dir; TestStore store; PasskeyVault localVault(&store, dir.path()); vault = &localVault;
        localVault.setup(QStringLiteral("synthetic vault passphrase"), QStringLiteral("synthetic vault passphrase")); QVERIFY(localVault.configured());
        TestProvider provider(vault);
        qApp->setProperty("arbiter.passkeys", QVariant::fromValue(static_cast<QObject *>(&provider)));
        struct Reset { ~Reset() { qApp->setProperty("arbiter.passkeys", QVariant()); } } reset;
        // The production vault is deliberately disabled off the record. Use a
        // named, temporary persistent profile, matching the application's mode.
        QWebEngineProfile profile(QStringLiteral("ArbiterPasskeyEngineTest"));
        profile.setPersistentStoragePath(dir.path() + QStringLiteral("/browser"));
        profile.setCachePath(dir.path() + QStringLiteral("/cache"));
        QVERIFY(!profile.isOffTheRecord());
        QWebEngineView view;
        view.setPage(new QWebEnginePage(&profile, &view));
        view.resize(800, 600); view.show();
        connect(view.page(), &QWebEnginePage::webAuthUxRequested, this, [this](QWebEngineWebAuthUxRequest *ux) {
            ++shown; request = ux; bridge = ux->property("arbiterRequest").value<QObject *>(); handledPayload = false; selectedLocal = false;
            if (!bridge) { ux->cancel(); return; }
            connect(bridge, SIGNAL(changed()), this, SLOT(answer()));
            QTimer::singleShot(0, this, &PasskeyEngineTest::answer);
        });
        QSignalSpy loaded(&view, &QWebEngineView::loadFinished);
        view.setHtml("<!doctype html><p>Local authenticator fixture</p>", QUrl("https://example.test/"));
        QVERIFY(loaded.wait(15000)); QVERIFY(loaded.last().first().toBool());
        js(view.page(), R"JS(
            globalThis.platformAvailable = 'pending'; globalThis.conditionalAvailable = 'pending';
            PublicKeyCredential.isUserVerifyingPlatformAuthenticatorAvailable().then(v => platformAvailable = v);
            PublicKeyCredential.isConditionalMediationAvailable().then(v => conditionalAvailable = v);
            true;
        )JS");
        QTRY_COMPARE_WITH_TIMEOUT(js(view.page(), "platformAvailable"), QVariant(true), 5000);
        QTRY_COMPARE_WITH_TIMEOUT(js(view.page(), "conditionalAvailable"), QVariant(false), 5000);
        js(view.page(), R"JS(
            globalThis.authResult = 'pending';
            globalThis.b64 = a => btoa(String.fromCharCode(...new Uint8Array(a)));
            navigator.credentials.create({publicKey: {
                challenge: crypto.getRandomValues(new Uint8Array(32)),
                rp: {id: 'example.test', name: 'Test'}, user: {id: new Uint8Array([1,2,3]), name: 'alice', displayName: 'Alice'},
                pubKeyCredParams: [{type: 'public-key', alg: -7}],
                authenticatorSelection: {authenticatorAttachment: 'platform', residentKey: 'required', userVerification: 'required'},
                attestation: 'none', timeout: 60000
            }}).then(c => { globalThis.credential = c; globalThis.publicKey = b64(c.response.getPublicKey()); globalThis.authResult = 'created'; },
                     e => globalThis.authResult = e.name);
            true;
        )JS");
        QTRY_COMPARE_WITH_TIMEOUT(js(view.page(), "authResult").toString(), QStringLiteral("created"), 15000);
        QVERIFY(shown > 0); QCOMPARE(vault->credentials(QStringLiteral("example.test")).size(), 1);
        js(view.page(), R"JS(
            globalThis.authResult = 'pending';
            navigator.credentials.get({publicKey: {challenge: crypto.getRandomValues(new Uint8Array(32)), rpId: 'example.test',
                userVerification: 'required', timeout: 60000}}).then(c => {
                    globalThis.signedData = b64(c.response.authenticatorData); globalThis.clientData = b64(c.response.clientDataJSON);
                    globalThis.signature = b64(c.response.signature); globalThis.authResult = 'asserted';
                }, e => globalThis.authResult = e.name);
            true;
        )JS");
        QTRY_COMPARE_WITH_TIMEOUT(js(view.page(), "authResult").toString(), QStringLiteral("asserted"), 15000);
        auto publicDer = QByteArray::fromBase64(js(view.page(), "publicKey").toByteArray());
        auto auth = QByteArray::fromBase64(js(view.page(), "signedData").toByteArray());
        auto client = QByteArray::fromBase64(js(view.page(), "clientData").toByteArray());
        auto signature = QByteArray::fromBase64(js(view.page(), "signature").toByteArray());
        auto signedBytes = auth + QCryptographicHash::hash(client, QCryptographicHash::Sha256);
        const auto *cursor = reinterpret_cast<const unsigned char *>(publicDer.constData());
        auto *key = d2i_PUBKEY(nullptr, &cursor, publicDer.size()); QVERIFY(key);
        auto *ctx = EVP_MD_CTX_new(); QVERIFY(ctx);
        QCOMPARE(EVP_DigestVerifyInit(ctx, nullptr, EVP_sha256(), nullptr, key), 1);
        QCOMPARE(EVP_DigestVerify(ctx, reinterpret_cast<const unsigned char *>(signature.constData()), signature.size(),
            reinterpret_cast<const unsigned char *>(signedBytes.constData()), signedBytes.size()), 1);
        EVP_MD_CTX_free(ctx); EVP_PKEY_free(key);
        const int before = shown;
        js(view.page(), R"JS(
            globalThis.authResult = 'pending';
            navigator.credentials.get({publicKey: {challenge: new Uint8Array(32), rpId: 'evil.test'}})
                .then(() => authResult = 'unexpected', e => authResult = e.name); true;
        )JS");
        QTRY_COMPARE_WITH_TIMEOUT(js(view.page(), "authResult").toString(), QStringLiteral("SecurityError"), 5000);
        QCOMPARE(shown, before);
#endif
    }
};
QTEST_MAIN(PasskeyEngineTest)
#include "passkeyengine_test.moc"
