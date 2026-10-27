#include "../src/sitepermissions.h"
#include "../src/settings.h"
#include <QApplication>
#include <QQuickWebEngineProfile>
#include <QTemporaryDir>
#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QWebEngineView>
#include <QTcpServer>
#include <QTcpSocket>
#include <QtWebEngineQuick/qtwebenginequickglobal.h>
#include <QtTest>

using Type=QWebEnginePermission::PermissionType;
using State=QWebEnginePermission::State;

class SitePermissionsTest : public QObject {
    Q_OBJECT
    QTemporaryDir config;
private slots:
    void initTestCase() {
        qputenv("XDG_CONFIG_HOME",config.path().toUtf8());
        qputenv("XDG_CONFIG_DIRS",config.path().toUtf8());
    }
    void init() { Settings().clear(); }
    void focusQueueAndStaleDecisions() {
        QQuickWebEngineProfile profile;
        QObject a,b;
        SitePermissions queue;
        queue.setFocusedView(&a);
        auto pa=profile.queryPermission(QUrl("https://a.example/"),Type::Geolocation);
        auto pb=profile.queryPermission(QUrl("https://b.example/"),Type::Notifications);
        QVERIFY(pa.isValid()); QVERIFY(pb.isValid());
        queue.request(&b,pb,QUrl("https://b.example/secret?token=private"));
        QVERIFY(!queue.active());
        queue.request(&a,pa,QUrl("https://user:password@a.example/account?token=private"));
        QVERIFY(queue.active());
        QCOMPARE(queue.origin(),QString("https://a.example"));
        QCOMPARE(queue.topOrigin(),QString("https://a.example"));
        const auto old=queue.requestId();
        queue.setFocusedView(&b);
        queue.decide(old,true);
        QCOMPARE(pa.state(),State::Ask);
        QCOMPARE(pb.state(),State::Ask);
        queue.decide(queue.requestId(),false);
        QCOMPARE(pb.state(),State::Denied);
        queue.setFocusedView(&a);
        queue.decide(queue.requestId(),true);
        QCOMPARE(pa.state(),State::Granted);
        QVERIFY(!queue.active());
    }
    void cancellationDoesNotSaveBlock() {
        QQuickWebEngineProfile profile;
        QObject view;
        SitePermissions queue;
        queue.setFocusedView(&view);
        auto p=profile.queryPermission(QUrl("https://cancel.example/"),Type::Geolocation);
        queue.request(&view,p,p.origin());
        queue.dismiss(queue.requestId());
        QCOMPARE(p.state(),State::Ask);
        queue.request(&view,p,p.origin());
        queue.cancelFor(&view);
        QCOMPARE(p.state(),State::Ask);
        queue.request(&view,p,p.origin());
        queue.clear();
        QCOMPARE(p.state(),State::Ask);
    }
    void disabledLocationAndDestroyedOwner() {
        QQuickWebEngineProfile profile;
        SitePermissions queue;
        auto *view=new QObject;
        queue.setFocusedView(view);
        auto p=profile.queryPermission(QUrl("https://location.example/"),Type::Geolocation);
        queue.request(view,p,p.origin());
        queue.setLocationEnabled(false);
        QVERIFY(!queue.active()); QCOMPARE(p.state(),State::Ask);
        queue.request(view,p,p.origin());
        QVERIFY(!queue.active()); QCOMPARE(p.state(),State::Ask);
        queue.setLocationEnabled(true);
        queue.request(view,p,p.origin());
        delete view;
        QVERIFY(!queue.active()); QCOMPARE(p.state(),State::Ask);
    }
    void cancellingAnotherTabPreservesSharedDecision() {
        QQuickWebEngineProfile profile;
        QObject a,b;
        SitePermissions queue;
        queue.setFocusedView(&a);
        auto p=profile.queryPermission(QUrl("https://shared.example/"),Type::Notifications);
        queue.request(&a,p,p.origin());
        queue.request(&b,p,p.origin());
        queue.decide(queue.requestId(),true);
        QCOMPARE(p.state(),State::Granted);
        queue.cancelFor(&b);
        QCOMPARE(p.state(),State::Granted);
    }
    void foreignOriginAndVisibility() {
        QQuickWebEngineProfile profile;
        SitePermissions queue;
        QObject view;
        queue.setFocusedView(&view);
        auto p=profile.queryPermission(QUrl("https://frame.example:8443/"),Type::Notifications);
        queue.request(&view,p,QUrl("https://top.example/page"));
        QCOMPARE(queue.origin(),QString("https://frame.example:8443"));
        QCOMPARE(queue.topOrigin(),QString("https://top.example"));
        const int id=queue.requestId();
        queue.setEnabled(false);
        queue.decide(id,true);
        QCOMPARE(p.state(),State::Ask);
        queue.setEnabled(true);
        QCOMPARE(queue.requestId(),id);
        queue.decide(id,true);
        QCOMPARE(p.state(),State::Granted);
    }
    void globalLocationRevocationAndPrivateIsolation() {
        SitePermissionSettings settings;
        QQuickWebEngineProfile privateProfile;
        settings.trackProfile(&privateProfile);
        auto geo=privateProfile.queryPermission(QUrl("https://private.example/"),Type::Geolocation);
        auto notification=privateProfile.queryPermission(QUrl("https://private.example/"),Type::Notifications);
        geo.grant(); notification.grant();
        QCOMPARE(geo.state(),State::Granted);
        QVERIFY(settings.savedPermissions().isEmpty());
        settings.setGeolocationEnabled(false);
        QCOMPARE(geo.state(),State::Ask);
        QCOMPARE(notification.state(),State::Granted);
        SitePermissionSettings restored;
        QVERIFY(!restored.geolocationEnabled());
        settings.setGeolocationEnabled(true);
        QCOMPARE(geo.state(),State::Ask);
        QQuickWebEngineProfile lateProfile;
        auto late=lateProfile.queryPermission(QUrl("https://late.example/"),Type::Geolocation);
        late.grant();
        restored.trackProfile(&lateProfile);
        QCOMPARE(late.state(),State::Ask);
    }
    void resetSavedDecision() {
        QTemporaryDir storage;
        QQuickWebEngineProfile profile;
        profile.setPersistentStoragePath(storage.path()+"/storage");
        profile.setCachePath(storage.path()+"/cache");
        profile.setStorageName("permission-test");
        profile.setOffTheRecord(false);
        profile.setPersistentPermissionsPolicy(QQuickWebEngineProfile::PersistentPermissionsPolicy::StoreInMemory);
        SitePermissionSettings settings;
        settings.trackProfile(&profile);
        auto permission=profile.queryPermission(QUrl("https://saved.example/"),Type::Notifications);
        permission.deny();
        const auto saved=settings.savedPermissions();
        QCOMPARE(saved.size(),1);
        QVERIFY(!saved[0].toMap()["allowed"].toBool());
        settings.resetPermission("permission-test",permission.origin(),int(Type::Notifications));
        QCOMPARE(permission.state(),State::Ask);
        QVERIFY(settings.savedPermissions().isEmpty());
    }
    void actualBrowserLocationDenial() {
        QWebEngineProfile profile;
        QWebEnginePage page(&profile);
        QWebEngineView view;
        view.setPage(&page);
        view.resize(400,300); view.show();
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        const QByteArray html="<script>window.result=0;navigator.geolocation.getCurrentPosition(()=>window.result=99,e=>window.result=e.code,{timeout:5000});</script>";
        connect(&server,&QTcpServer::newConnection,&server,[&] {
            while(auto *socket=server.nextPendingConnection()) {
                connect(socket,&QTcpSocket::disconnected,socket,&QObject::deleteLater);
                connect(socket,&QTcpSocket::readyRead,socket,[socket,html] {
                    socket->readAll();
                    socket->write("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: close\r\nContent-Length: "+QByteArray::number(html.size())+"\r\n\r\n"+html);
                    socket->disconnectFromHost();
                });
            }
        });
        SitePermissions queue;
        queue.setFocusedView(&page);
        queue.setLocationEnabled(false);
        QSignalSpy requests(&page,&QWebEnginePage::permissionRequested);
        connect(&page,&QWebEnginePage::permissionRequested,&queue,[&](QWebEnginePermission permission) {
            queue.request(&page,permission,page.url());
        });
        QSignalSpy loaded(&page,&QWebEnginePage::loadFinished);
        page.load(QUrl(QString("http://127.0.0.1:%1/").arg(server.serverPort())));
        QTRY_VERIFY_WITH_TIMEOUT(!loaded.isEmpty(),10000);
        QTRY_VERIFY_WITH_TIMEOUT(!requests.isEmpty(),10000);
        auto result=std::make_shared<QPair<int,bool>>(0,false);
        auto poll=[&] {
            if (result->first==0 && !result->second) {
                result->second=true;
                page.runJavaScript("window.result",[result](const QVariant &value) { result->first=value.toInt(); result->second=false; });
            }
            return result->first;
        };
        QTRY_COMPARE_WITH_TIMEOUT(poll(),1,10000);
        QVERIFY(!queue.active());
    }
    void iframeLocationRevocation() {
#ifndef ARBITER_PATCHED_WEBENGINE
        QSKIP("Requires Arbiter's cross-frame permission revocation patch; exercised by the patched-engine suite.");
#endif
        QTcpServer topServer,frameServer;
        QVERIFY(topServer.listen(QHostAddress::LocalHost));
        QVERIFY(frameServer.listen(QHostAddress::LocalHost));
        const QUrl top(QString("http://127.0.0.1:%1/").arg(topServer.serverPort()));
        const QUrl frame(QString("http://127.0.0.1:%1/").arg(frameServer.serverPort()));
        auto serve=[](QTcpServer &server,const QByteArray &html) {
            connect(&server,&QTcpServer::newConnection,&server,[&server,html] {
                while (auto *socket=server.nextPendingConnection()) {
                    connect(socket,&QTcpSocket::disconnected,socket,&QObject::deleteLater);
                    connect(socket,&QTcpSocket::readyRead,socket,[socket,html] {
                        socket->readAll();
                        socket->write("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: close\r\nContent-Length: "+QByteArray::number(html.size())+"\r\n\r\n"+html);
                        socket->disconnectFromHost();
                    });
                }
            });
        };
        serve(topServer,QString("<script>window.state='';addEventListener('message',e=>window.state=e.data);</script><iframe allow='geolocation' src='%1'></iframe>").arg(frame.toString()).toUtf8());
        serve(frameServer,"<script>navigator.permissions.query({name:'geolocation'}).then(p=>{parent.postMessage(p.state,'*');p.onchange=()=>parent.postMessage(p.state,'*');});</script>");
        QWebEngineProfile profile;
        auto permission=profile.queryPermission(frame,Type::Geolocation);
        permission.grant();
        QWebEnginePage page(&profile);
        QWebEngineView view;
        view.setPage(&page); view.resize(400,300); view.show();
        page.load(top);
        auto state=std::make_shared<QString>();
        auto pending=std::make_shared<bool>(false);
        auto read=[&] {
            if (!*pending) {
                *pending=true;
                page.runJavaScript("window.state",[state,pending](const QVariant &result) { *state=result.toString(); *pending=false; });
            }
            return *state;
        };
        QTRY_COMPARE_WITH_TIMEOUT(read(),QString("granted"),10000);
        permission.reset();
        QTRY_COMPARE_WITH_TIMEOUT(read(),QString("prompt"),10000);
    }
};
int main(int argc,char **argv) {
    QtWebEngineQuick::initialize();
    QApplication app(argc,argv);
    SitePermissionsTest test;
    return QTest::qExec(&test,argc,argv);
}
#include "sitepermissions_test.moc"
