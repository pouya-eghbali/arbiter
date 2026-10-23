#include "../src/webauthncontroller.h"
#include <QEventLoop>
#include <QWebEnginePage>
#include <QWebEngineView>
#include <QtTest>

class WebAuthnControllerTest : public QObject
{
    Q_OBJECT
    QVariant evaluate(QWebEnginePage *page, const QString &script) {
        QEventLoop loop;
        QVariant result;
        QTimer timer;
        timer.setSingleShot(true);
        connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
        page->runJavaScript(script, [&](const QVariant &value) { result = value; loop.quit(); });
        timer.start(5000);
        loop.exec();
        return result;
    }
private slots:
    void pinValidation() {
        QVERIFY(WebAuthnController::validatePin("1234", "", 4, false).isEmpty());
        QVERIFY(!WebAuthnController::validatePin("123", "", 4, false).isEmpty());
        QVERIFY(!WebAuthnController::validatePin("1234", "1235", 4, true).isEmpty());
        QVERIFY(!WebAuthnController::validatePin("1234", "1234", 6, true).isEmpty());
        QVERIFY(WebAuthnController::validatePin(QString(63, 'x'), "", 4, false).isEmpty());
        QVERIFY(!WebAuthnController::validatePin(QString(64, 'x'), "", 4, false).isEmpty());
        QVERIFY(!WebAuthnController::validatePin(QString("1234") + QChar(0), "", 4, false).isEmpty());
        const auto unicode = QString::fromUtf8("😀😀😀😀");
        QVERIFY(WebAuthnController::validatePin(unicode, unicode, 4, true).isEmpty());
        QVERIFY(!WebAuthnController::validatePin(unicode, unicode, 5, true).isEmpty());
        QVERIFY(!WebAuthnController::validatePin(QString(32, QChar(0x00e9)), "", 4, false).isEmpty());
    }
    void inactiveActionsAreHarmless() {
        WebAuthnController controller;
        QVERIFY(!controller.active());
        QVERIFY(controller.accounts().isEmpty());
        QVERIFY(!controller.confirmPin());
        QVERIFY(!controller.canRetry());
        controller.retry();
        controller.submitPin("1234", "1234");
        controller.selectAccount("alice");
        controller.cancel();
        controller.handle(nullptr);
        QVERIFY(!controller.active());
        QVERIFY(controller.error().isEmpty());
    }
    void realEngineRequestCanBeCancelled() {
#ifdef Q_OS_WIN
        QSKIP("Windows owns its native authenticator dialog; this test exercises Qt-owned discovery UI.");
#endif
        QWebEngineView view;
        view.resize(800, 600);
        view.show();
        WebAuthnController controller;
        connect(view.page(), &QWebEnginePage::webAuthUxRequested, &controller, &WebAuthnController::handle);
        QSignalSpy loaded(&view, &QWebEngineView::loadFinished);
        view.setHtml("<!doctype html><p>Synthetic WebAuthn request</p>", QUrl("https://example.test/"));
        QVERIFY(loaded.wait(15000));
        QVERIFY(loaded.last().first().toBool());
        evaluate(view.page(), R"JS(
            globalThis.authResult = 'pending';
            navigator.credentials.get({ publicKey: {
                challenge: new Uint8Array(32), rpId: 'example.test',
                allowCredentials: [{type: 'public-key', id: new Uint8Array([1,2,3,4]), transports: ['usb']}],
                userVerification: 'discouraged', timeout: 60000
            }}).then(() => globalThis.authResult = 'unexpected-success',
                     error => globalThis.authResult = error.name);
            true;
        )JS");
        QTRY_VERIFY_WITH_TIMEOUT(controller.active(), 10000);
        QCOMPARE(controller.relyingParty(), QString("example.test"));
        QVERIFY(!controller.description().isEmpty());
        controller.cancel();
        QVERIFY(!controller.active());
        QTRY_COMPARE_WITH_TIMEOUT(evaluate(view.page(), "globalThis.authResult").toString(), QString("NotAllowedError"), 5000);
    }
};
QTEST_MAIN(WebAuthnControllerTest)
#include "webauthncontroller_test.moc"
