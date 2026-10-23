#include "../src/passkeycontroller.h"
#include <QApplication>
#include <QEventLoop>
#include <QTemporaryDir>
#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QWebEngineView>
#include <QtTest>

class TestBiometrics final : public BiometricAuth {
public:
    bool success=true;
    bool foregroundRequired() const override { return false; }
    QString name() const override { return "Test fingerprint"; }
    void refresh() override { availability(true,"Available"); }
protected:
    void startNative(const QString &,quint64 token) override { complete(token,success,success ? QString() : QString("Fingerprint failed")); }
    void cancelNative() override {}
};
class ControllerStore final : public SecretStore {
public:
    QHash<QString,QByteArray> items;
    void read(const QString &key,Callback callback) override { callback(items.contains(key) ? QString() : QString("Missing"),items.value(key)); }
    void write(const QString &key,const QByteArray &data,Callback callback) override { items[key]=data; callback({},{}); }
    void remove(const QString &key,Callback callback) override { items.remove(key); callback({},{}); }
};
// All controller instances in this test inject a verifier; no real OS prompt.
BiometricAuth *createBiometricAuth(QObject *) { qFatal("Inject the test verifier"); return nullptr; }

class PasskeyControllerTest : public QObject {
    Q_OBJECT
    QVariant js(QWebEnginePage *page,const QString &script) {
        QVariant result; QEventLoop loop; QTimer timeout; timeout.setSingleShot(true);
        connect(&timeout,&QTimer::timeout,&loop,&QEventLoop::quit);
        page->runJavaScript(script,[&](const QVariant &value) {result=value;loop.quit();});
        timeout.start(5000); loop.exec(); return result;
    }
private slots:
    void biometricFailureKeepsChallengeForPassphraseFallback() {
        QTemporaryDir directory; ControllerStore store; TestBiometrics auth; auth.refresh();
        PasskeyController controller(&store,directory.path()+"/vault",&auth);
        if(!controller.enabled()) QSKIP("Requires the patched WebEngine SDK.");
        QWebEngineProfile profile("ArbiterBiometricControllerTest");
        profile.setPersistentStoragePath(directory.path()+"/browser"); profile.setCachePath(directory.path()+"/cache");
        QWebEngineView view; view.setPage(new QWebEnginePage(&profile,&view)); view.resize(800,600); view.show();
        QSignalSpy loaded(&view,&QWebEngineView::loadFinished);
        view.setHtml("<!doctype html><title>Biometric controller fixture</title>",QUrl("https://example.test/"));
        QVERIFY(loaded.wait(15000)); QVERIFY(loaded.last().first().toBool());
        const QString phrase("synthetic biometric controller phrase");
        controller.setup(phrase,phrase); QVERIFY(controller.configured());
        controller.setBiometricEnabled(true,phrase); QVERIFY2(controller.biometricReady(),qPrintable(controller.error()));
        bool selected=false;
        connect(view.page(),&QWebEnginePage::webAuthUxRequested,this,[&](QWebEngineWebAuthUxRequest *request) {
            selected=false; controller.handle(request);
        });
        connect(&controller,&PasskeyController::changed,this,[&] {
            if(controller.active() && controller.localAvailable() && !controller.verifying() && !selected) {
                selected=true; QTimer::singleShot(0,&controller,&PasskeyController::useLocal);
            }
        });
        js(view.page(),R"JS(
            globalThis.result='pending';
            navigator.credentials.create({publicKey:{challenge:new Uint8Array(32).fill(1),rp:{id:'example.test',name:'Test'},user:{id:new Uint8Array([1]),name:'alice',displayName:'Alice'},pubKeyCredParams:[{type:'public-key',alg:-7}],authenticatorSelection:{residentKey:'required',userVerification:'required'},timeout:30000}})
            .then(c=>{globalThis.credentialId=Array.from(new Uint8Array(c.rawId));result='created'},e=>result=e.name);
            true;
        )JS");
        QTRY_VERIFY_WITH_TIMEOUT(controller.verifying(),10000);
        controller.verifyBiometric(0);
        QTRY_COMPARE_WITH_TIMEOUT(js(view.page(),"result").toString(),QString("created"),10000);
        QTRY_VERIFY_WITH_TIMEOUT(!controller.active(),5000);
        auth.success=false;
        js(view.page(),R"JS(
            globalThis.result='pending';
            navigator.credentials.get({publicKey:{challenge:new Uint8Array(32).fill(2),rpId:'example.test',allowCredentials:[{type:'public-key',id:new Uint8Array(credentialId)}],userVerification:'required',timeout:30000}})
            .then(c=>result='signed',e=>result=e.name); true;
        )JS");
        QTRY_VERIFY_WITH_TIMEOUT(controller.verifying(),10000);
        controller.verifyBiometric(0);
        QVERIFY(!controller.busy()); QVERIFY(!controller.error().isEmpty()); QVERIFY(controller.verifying());
        QCOMPARE(js(view.page(),"result").toString(),QString("pending"));
        controller.verify(0,phrase);
        QTRY_COMPARE_WITH_TIMEOUT(js(view.page(),"result").toString(),QString("signed"),10000);
    }
};
QTEST_MAIN(PasskeyControllerTest)
#include "passkeycontroller_test.moc"
