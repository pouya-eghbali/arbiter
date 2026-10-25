#include <QtTest>
#include <QtWebEngineQuick>
#include <QQmlApplicationEngine>
#include <QQmlExpression>
#include <QQmlContext>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QWheelEvent>
#include <QFile>
#include "../src/historyswipe.h"

class HistorySwipeEngineTest : public QObject {
    Q_OBJECT
private slots:
    void compositorDecidesNavigation_data() {
        QTest::addColumn<QString>("kind"); QTest::addColumn<bool>("shouldNavigate");
        QTest::newRow("boundary") << "boundary" << true;
        QTest::newRow("forward") << "forward" << true;
        QTest::newRow("short") << "short" << false;
        QTest::newRow("momentum-after-short") << "momentum-short" << false;
        QTest::newRow("momentum-after-ready") << "momentum-ready" << true;
        QTest::newRow("root-scroll") << "wide" << false;
        QTest::newRow("nested-scroll") << "nested" << false;
        QTest::newRow("iframe-scroll") << "iframe" << false;
        QTest::newRow("css-contain") << "contain" << false;
        QTest::newRow("css-none") << "none" << false;
        QTest::newRow("wheel-cancelled") << "prevent" << false;
        QTest::newRow("vertical") << "vertical" << false;
        QTest::newRow("mouse-wheel") << "wheel" << false;
        QTest::newRow("momentum-only") << "momentum" << false;
        QTest::newRow("reversed") << "reverse" << false;
    }
    void compositorDecidesNavigation() {
        QFETCH(QString,kind); QFETCH(bool,shouldNavigate);
        QTemporaryDir files; QVERIFY(files.isValid());
        QString html="<html><head><style>html,body{margin:0;height:100%;}body{height:3000px;}</style></head><body>History swipe fixture";
        if(kind=="wide") html+="<div style='width:4000px;height:800px'>Wide content</div>";
        if(kind=="nested") html+="<div style='position:fixed;inset:0;overflow:auto'><div style='width:4000px;height:800px'>Scrollable child</div></div>";
        if(kind=="iframe") html+="<iframe style='position:fixed;inset:0;width:100%;height:100%;border:0' srcdoc=\"<html><body style='width:4000px;height:800px'>Scrollable frame</body></html>\"></iframe>";
        if(kind=="contain" || kind=="none") html+="<style>html{overscroll-behavior-x:"+kind+"}</style>";
        if(kind=="prevent") html+="<script>addEventListener('wheel',e=>e.preventDefault(),{passive:false})</script>";
        html+="</body></html>";
        for(auto name:{"first.html","second.html"}) { QFile f(files.filePath(name)); QVERIFY(f.open(QIODevice::WriteOnly)); f.write(html.toUtf8()); }
        QQmlApplicationEngine engine;
        engine.loadData(R"(
            import QtQuick
            import QtWebEngine
            Window {
                width: 900; height: 650; visible: true
                WebEngineView { id: page; objectName: "page"; anchors.fill: parent; profile: WebEngineProfile { offTheRecord: true } }
            }
        )");
        QVERIFY(!engine.rootObjects().isEmpty());
        auto *window=qobject_cast<QQuickWindow *>(engine.rootObjects().first()); QVERIFY(window);
        auto *page=window->findChild<QQuickItem *>("page"); QVERIFY(page);
        auto evaluate=[&](const QString &code) { QQmlExpression expression(QQmlEngine::contextForObject(window),window,code); const auto result=expression.evaluate(); if(expression.hasError()) qWarning()<<expression.error(); return result; };
        page->setProperty("url",QUrl::fromLocalFile(files.filePath("first.html")));
        QTRY_VERIFY_WITH_TIMEOUT(evaluate("!page.loading && page.mainFrame.url.toString().endsWith('first.html')").toBool(),10000);
        page->setProperty("url",QUrl::fromLocalFile(files.filePath("second.html")));
        QTRY_VERIFY_WITH_TIMEOUT(evaluate("!page.loading && page.mainFrame.url.toString().endsWith('second.html') && page.canGoBack").toBool(),10000);
        if(kind=="forward") {
            evaluate("page.goBack()");
            QTRY_VERIFY(evaluate("!page.loading && page.mainFrame.url.toString().endsWith('first.html') && page.canGoForward").toBool());
        }
        HistorySwipe swipe; swipe.setView(page); swipe.setCanGoBack(true); swipe.setCanGoForward(true);
        QSignalSpy triggered(&swipe,&HistorySwipe::triggered);
        connect(&swipe,&HistorySwipe::triggered,page,[&](int direction) { evaluate(direction<0 ? "page.goBack()" : "page.goForward()"); });
        QTest::qWait(200);
        const QPointF pos(450,320);
        auto wheel=[&](int dx,int dy,Qt::ScrollPhase phase) {
            QWheelEvent event(pos,window->mapToGlobal(pos.toPoint()),QPoint(dx,dy),QPoint(dx*2,dy*2),Qt::NoButton,Qt::NoModifier,phase,false,Qt::MouseEventSynthesizedBySystem);
            QCoreApplication::sendEvent(window,&event); QTest::qWait(70);
        };
        const bool vertical=kind=="vertical";
        const int delta=kind=="wide" || kind=="nested" || kind=="iframe" || kind=="forward" ? -40 : (kind=="short" || kind=="momentum-short") ? 10 : 40;
        wheel(vertical?0:(delta>0?2:-2),vertical?2:0,kind=="wheel"?Qt::NoScrollPhase:kind=="momentum"?Qt::ScrollMomentum:Qt::ScrollBegin);
        for(int i=0;i<8;++i) wheel(vertical?0:delta,vertical?40:0,kind=="wheel"?Qt::NoScrollPhase:kind=="momentum"?Qt::ScrollMomentum:Qt::ScrollUpdate);
        if(kind=="reverse") for(int i=0;i<4;++i) wheel(-40,0,Qt::ScrollUpdate);
        if(shouldNavigate) { QVERIFY2(swipe.active(),"No compositor overscroll feedback"); QCOMPARE(swipe.direction(),kind=="forward"?1:-1); QCOMPARE(swipe.progress(),1.); }
        if(kind=="momentum-short" || kind=="momentum-ready") {
            wheel(0,0,Qt::ScrollEnd);
            for(int i=0;i<8;++i) wheel(40,0,Qt::ScrollMomentum);
        }
        wheel(0,0,kind=="wheel"?Qt::NoScrollPhase:Qt::ScrollEnd);
        QTest::qWait(300);
        QCOMPARE(triggered.size(),shouldNavigate?1:0);
        if(shouldNavigate) QTRY_VERIFY(evaluate(kind=="forward" ? "page.mainFrame.url.toString().endsWith('second.html')" : "page.mainFrame.url.toString().endsWith('first.html')").toBool());
        if(kind=="wide") QVERIFY(evaluate("page.scrollPosition.x > 0").toBool());
    }
};
int main(int argc,char **argv) {
    QStandardPaths::setTestModeEnabled(true);
    QtWebEngineQuick::initialize();
    QGuiApplication app(argc,argv);
    HistorySwipeEngineTest test;
    return QTest::qExec(&test,argc,argv);
}
#include "historyswipe_engine_test.moc"
