#include "../src/adblocker.h"
#include "../src/settings.h"
#include <QtTest>
#include <QtWebEngineQuick>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QStandardPaths>
#include <QQuickWebEngineProfile>
#include <QQmlExpression>
#include <QFile>
class AdBlockTest : public QObject {
    Q_OBJECT
private slots:
    void rules() {
        AdBlockEngine engine("||ads.example^$third-party\n@@||ads.example/allowed.js\n##.advert\nnews.example##.sponsored\nnews.example#@#.advert");
        QVERIFY(engine.valid());
        QVERIFY(engine.blocks(QUrl("https://ads.example/banner.js"),QUrl("https://news.example/"),"script","GET"));
        QVERIFY(!engine.blocks(QUrl("https://ads.example/allowed.js"),QUrl("https://news.example/"),"script","GET"));
        QVERIFY(!engine.blocks(QUrl("https://ads.example/banner.js"),QUrl("https://ads.example/"),"script","GET"));
        const auto selectors=engine.cosmetic(QUrl("https://news.example/"),{"advert"},{});
        QVERIFY(selectors.contains(".sponsored")); QVERIFY(!selectors.contains(".advert"));
        AdBlockEngine bundled(AdBlocker::bundledRules()); QVERIFY(bundled.valid());
        QVERIFY(bundled.blocks(QUrl("https://googleads.g.doubleclick.net/pagead/ads"),QUrl("https://news.example/"),"script","GET"));
    }
    void independentViews() {
        AdBlocker manager("/arbiter-ad.js$script\n##.advert",nullptr);
        ViewAdBlock state; state.setManager(&manager); QVERIFY(state.enabled());
        state.setPaused(true); QVERIFY(!state.enabled()); state.setPaused(false);
        manager.setEnabled(false); QVERIFY(!state.enabled()); manager.setEnabled(true); QVERIFY(state.enabled());
        QTcpServer server; QVERIFY(server.listen(QHostAddress::LocalHost)); int scripts=0;
        connect(&server,&QTcpServer::newConnection,this,[&] {
            auto *socket=server.nextPendingConnection();
            connect(socket,&QTcpSocket::readyRead,socket,[&,socket] {
                auto bytes=socket->property("request").toByteArray()+socket->readAll(); socket->setProperty("request",bytes);
                if (!bytes.contains("\r\n\r\n") || socket->property("answered").toBool()) return;
                socket->setProperty("answered",true);
                const bool script=bytes.startsWith("GET /arbiter-ad.js");
                if (script) ++scripts;
                const QByteArray body=script ? QByteArray("window.adLoaded=true") : QByteArray("<!doctype html><div class='advert'>advert</div><script src='/arbiter-ad.js'></script>");
                socket->write("HTTP/1.1 200 OK\r\nCache-Control: no-store\r\nConnection: close\r\nContent-Type: "+QByteArray(script ? "text/javascript" : "text/html")+"\r\nContent-Length: "+QByteArray::number(body.size())+"\r\n\r\n"+body);
                socket->disconnectFromHost();
            }); connect(socket,&QTcpSocket::disconnected,socket,&QObject::deleteLater);
        });
        const QUrl page(QString("http://127.0.0.1:%1/").arg(server.serverPort()));
        qmlRegisterType<ViewAdBlock>("ArbiterTest",1,0,"ViewAdBlock");
        QQmlApplicationEngine engine;
#ifdef ARBITER_WEBENGINE_QML_PATH
        engine.addImportPath(QString::fromUtf8(ARBITER_WEBENGINE_QML_PATH));
#endif
        connect(&engine,&QQmlEngine::warnings,this,[](const QList<QQmlError> &errors) { for(const auto &error:errors) fprintf(stderr,"%s\n",qPrintable(error.toString())); });
        engine.rootContext()->setContextProperty("blockManager",&manager);
        engine.rootContext()->setContextProperty("testUrl",page);
        engine.loadData(R"(
import QtQuick
import QtWebEngine
import ArbiterTest
Window {
    width: 800; height: 500; visible: true
    property alias protectedView: a
    property alias pausedView: b
    property alias pausedState: bBlock
    property var results: ({})
    WebEngineProfile { id: sharedProfile; offTheRecord: true }
    ViewAdBlock { id: aBlock; manager: blockManager }
    ViewAdBlock { id: bBlock; manager: blockManager; paused: true }
    WebEngineView { id: a; width: 400; height: 500; profile: sharedProfile; urlRequestInterceptor: aBlock; url: testUrl }
    WebEngineView { id: b; x: 400; width: 400; height: 500; profile: sharedProfile; urlRequestInterceptor: bBlock; url: testUrl }
    function inspect() {
        a.mainFrame.runJavaScript("window.adLoaded === true",0,function(value) { results.a=value; results.aDone=true })
        b.mainFrame.runJavaScript("window.adLoaded === true",0,function(value) { results.b=value; results.bDone=true })
    }
    function reloadPaused() { b.reload() }
})",QUrl("qrc:/adblock-test.qml"));
        QTRY_VERIFY_WITH_TIMEOUT(!engine.rootObjects().isEmpty(),5000); auto *root=engine.rootObjects().first();
        auto *a=root->property("protectedView").value<QObject *>(); auto *b=root->property("pausedView").value<QObject *>();
        QVERIFY(a && b);
        QTRY_VERIFY_WITH_TIMEOUT(!a->property("loading").toBool() && !b->property("loading").toBool() && scripts==1,10000);
        QMetaObject::invokeMethod(root,"inspect");
        const auto results=[&] { return root->property("results").value<QJSValue>(); };
        QTRY_VERIFY(results().property("aDone").toBool() && results().property("bDone").toBool());
        QVERIFY(!results().property("a").toBool()); QVERIFY(results().property("b").toBool());
        auto *paused=qobject_cast<ViewAdBlock *>(root->property("pausedState").value<QObject *>()); QVERIFY(paused);
        paused->setPaused(false); QMetaObject::invokeMethod(root,"reloadPaused"); QTest::qWait(700);
        QCOMPARE(scripts,1); QVERIFY(paused->blocked()>0);
        // Execute the production cosmetic collector in its isolated world.
        QFile script(":/adblock/adblock.js"); QVERIFY(script.open(QIODevice::ReadOnly));
        engine.rootContext()->setContextProperty("cosmeticSource",QString::fromUtf8(script.readAll()));
        QQmlExpression install(qmlContext(root),root,"protectedView.mainFrame.runJavaScript(cosmeticSource,1,function(){})"); install.evaluate(); QVERIFY(!install.hasError()); QTest::qWait(100);
        QQmlExpression hide(qmlContext(root),root,R"JS(protectedView.mainFrame.runJavaScript("globalThis.__arbiterAdblock.collect()",1,function(data) { const rules=blockManager.enabled ? ['.advert'] : []; protectedView.mainFrame.runJavaScript('globalThis.__arbiterAdblock.apply('+JSON.stringify(data.token)+','+JSON.stringify(rules)+')',1,function(){}) }))JS");
        hide.evaluate(); QVERIFY(!hide.hasError()); QTest::qWait(150);
        QQmlExpression inspect(qmlContext(root),root,R"JS(protectedView.mainFrame.runJavaScript("getComputedStyle(document.querySelector('.advert')).display",1,function(value) { results.display=value }))JS");
        inspect.evaluate(); QVERIFY(!inspect.hasError());
        QTRY_COMPARE(results().property("display").toString(),"none");
        manager.setEnabled(false); QMetaObject::invokeMethod(root,"reloadPaused"); QTRY_COMPARE(scripts,2);
    }
};
int main(int argc,char **argv) {
    QStandardPaths::setTestModeEnabled(true); QtWebEngineQuick::initialize(); QGuiApplication app(argc,argv);
    QTemporaryDir config; qputenv("XDG_CONFIG_HOME",config.path().toUtf8()); Settings::initialize();
    AdBlockTest test; return QTest::qExec(&test,argc,argv);
}
#include "adblock_test.moc"
