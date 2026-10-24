#include "../src/nativeshare.h"
#include <QApplication>
#include <QTimer>
#include <QSignalSpy>
#include <QtTest>
class NativeShareTest : public QObject {
    Q_OBJECT
private slots:
    void rejectsCoveredOrChangedView() {
        QWindow host; QObject view,other;
        host.setProperty("webView",QVariant::fromValue(&view));
        host.setProperty("privateCovered",true);
        NativeShare share; share.setHost(&host);
        QVERIFY(!share.show(&view,QUrl("https://example.com/"),"Example",QPointF(20,20)));
        QVERIFY(!share.active());
        host.setProperty("privateCovered",false);
        QVERIFY(!share.show(&other,QUrl("https://example.com/"),"Example",QPointF(20,20)));
        QVERIFY(!share.active());
        QVERIFY(!share.show(&view,QUrl("about:blank"),"Blank",QPointF(20,20)));
        QVERIFY(!share.active());
        share.cancel(); share.cancel();
    }
    void nativePickerOpensAndCancels() {
        if (!qEnvironmentVariableIsSet("ARBITER_TEST_NATIVE_SHARE")) QSKIP("Opt-in native desktop UI test");
        QWindow host; QObject view;
        host.setTitle("Arbiter native sharing test"); host.resize(500,300);
        host.setProperty("webView",QVariant::fromValue(&view)); host.setProperty("privateCovered",false);
        host.show(); QVERIFY(QTest::qWaitForWindowExposed(&host)); host.requestActivate(); QTest::qWait(200);
        NativeShare share; share.setHost(&host); QSignalSpy changes(&share,&NativeShare::activeChanged);
        QTimer::singleShot(800,&share,&NativeShare::cancel);
        QVERIFY2(share.show(&view,QUrl("https://example.com/"),"Example page",QPointF(250,260)),qPrintable(share.error()));
        QVERIFY(changes.count()>0);
        QTRY_VERIFY_WITH_TIMEOUT(!share.active(),3000);
        share.cancel(); QVERIFY(!share.active());
    }
};
QTEST_MAIN(NativeShareTest)
#include "nativeshare_test.moc"
