#include "../src/updatetracker.h"
#include "../src/settings.h"
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QtTest>
#include <memory>

class UpdateTrackerTest : public QObject {
    Q_OBJECT
    QTcpServer server;
    QByteArray body, request;
    int status=200, requests=0;
    QUrl endpoint() const { return QUrl(QString("http://127.0.0.1:%1/latest").arg(server.serverPort())); }
    void release(const QString &tag,bool draft=false,bool prerelease=false) {
        body=QJsonDocument(QJsonObject{{"tag_name",tag},{"draft",draft},{"prerelease",prerelease},
            {"html_url","https://untrusted.example/download"}}).toJson();
    }
private slots:
    void initTestCase() {
        QVERIFY(server.listen(QHostAddress::LocalHost));
        connect(&server,&QTcpServer::newConnection,this,[this] {
            while(server.hasPendingConnections()) {
                auto *socket=server.nextPendingConnection();
                connect(socket,&QTcpSocket::readyRead,socket,[this,socket] {
                    auto data=socket->property("request").toByteArray()+socket->readAll();
                    socket->setProperty("request",data);
                    if(!data.contains("\r\n\r\n") || socket->property("answered").toBool()) return;
                    socket->setProperty("answered",true); request=data; ++requests;
                    if(status==0) return; // A server that never responds.
                    socket->write("HTTP/1.1 "+QByteArray::number(status)+" Response\r\nContent-Type: application/json\r\nConnection: close\r\nContent-Length: "+QByteArray::number(body.size())+"\r\n\r\n"+body);
                    socket->disconnectFromHost();
                });
                connect(socket,&QTcpSocket::disconnected,socket,&QObject::deleteLater);
            }
        });
    }
    void init() { Settings().clear(); requests=0; request.clear(); status=200; release("v1.2.0"); }
    void semanticVersions() {
        const QStringList ordered{"0.9.9","1.0.0-alpha","1.0.0-alpha.1","1.0.0-alpha.beta","1.0.0-beta","1.0.0-beta.2","1.0.0-beta.11","1.0.0-rc.1","1.0.0","1.0.1","1.10.0","10.0.0"};
        for(int i=0;i<ordered.size();++i) for(int j=0;j<ordered.size();++j) {
            auto order=UpdateTracker::compareVersions(ordered[i],ordered[j]); QVERIFY(order);
            QCOMPARE(*order<0,i<j); QCOMPARE(*order==0,i==j);
        }
        QCOMPARE(*UpdateTracker::compareVersions("v1.0.0+local","1.0.0+release"),0);
        for(const auto &invalid:{"1.0","01.0.0","1.0.0-01","1.0.0-","1.0.0/evil","garbage","1.0.0\n"})
            QVERIFY(!UpdateTracker::compareVersions(invalid,"1.0.0"));
    }
    void checkAndCache() {
        UpdateTracker tracker("1.0.0",endpoint());
        QCOMPARE(requests,0); QCOMPARE(tracker.state(),"idle");
        tracker.check(); tracker.check();
        QTRY_COMPARE(tracker.state(),"available"); QCOMPARE(requests,1);
        QCOMPARE(tracker.latestVersion(),"1.2.0"); QVERIFY(tracker.lastChecked().isValid());
        QCOMPARE(tracker.releaseUrl(),QUrl("https://github.com/pouya-eghbali/arbiter/releases/tag/v1.2.0"));
        QVERIFY(request.contains("User-Agent: Arbiter-Update-Check\r\n"));
        QVERIFY(!request.contains("Cookie:")); QVERIFY(!request.contains("Authorization:")); QVERIFY(!request.contains("1.0.0"));
        UpdateTracker restored("1.0.0",endpoint()); QCOMPARE(restored.state(),"available");
        restored.checkAutomatically(); QTest::qWait(30); QCOMPARE(requests,1);
    }
    void automaticPreferenceAndDailyLimit() {
        UpdateTracker tracker("1.0.0",endpoint()); tracker.setAutomatic(false);
        UpdateTracker disabled("1.0.0",endpoint()); QVERIFY(!disabled.automatic());
        disabled.checkAutomatically(); QTest::qWait(30); QCOMPARE(requests,0);
        disabled.check(); QTRY_COMPARE(disabled.state(),"available"); QCOMPARE(requests,1);
        disabled.setAutomatic(true); disabled.checkAutomatically(); QTest::qWait(30); QCOMPARE(requests,1);
        Settings().setValue("Updates/lastAttempt",QDateTime::currentDateTimeUtc().addDays(-2).toString(Qt::ISODate));
        UpdateTracker due("1.0.0",endpoint()); due.checkAutomatically();
        QTRY_COMPARE(due.state(),"available"); QCOMPARE(requests,2);
    }
    void sameAndNewerInstalledVersions() {
        UpdateTracker same("1.2.0",endpoint()); same.check(); QTRY_COMPARE(same.state(),"current");
        UpdateTracker newer("2.0.0",endpoint()); newer.check(); QTRY_COMPARE(newer.state(),"ahead");
        release("1.3.0"); newer.check(); QTRY_COMPARE(newer.state(),"ahead");
        QCOMPARE(newer.releaseUrl(),QUrl("https://github.com/pouya-eghbali/arbiter/releases/tag/1.3.0"));
    }
    void shutdownDuringRequest() {
        status=0;
        auto tracker=std::make_unique<UpdateTracker>("1.0.0",endpoint());
        tracker->check(); QTRY_COMPARE(requests,1);
        tracker.reset();
        QTest::qWait(30); // Cancellation must not call back into a destroyed tracker.
    }
    void invalidResponses_data() {
        QTest::addColumn<int>("httpStatus"); QTest::addColumn<QByteArray>("payload");
        QTest::newRow("malformed")<<200<<QByteArray("{broken");
        QTest::newRow("no-metadata")<<200<<QByteArray("{}");
        QTest::newRow("draft")<<200<<QByteArray(R"({"tag_name":"v2.0.0","draft":true,"prerelease":false})");
        QTest::newRow("preview")<<200<<QByteArray(R"({"tag_name":"v2.0.0-beta","draft":false,"prerelease":true})");
        QTest::newRow("mislabeled-preview")<<200<<QByteArray(R"({"tag_name":"v2.0.0-beta","draft":false,"prerelease":false})");
        QTest::newRow("invalid-tag")<<200<<QByteArray(R"({"tag_name":"../../evil","draft":false,"prerelease":false})");
        QTest::newRow("oversized")<<200<<QByteArray(300*1024,'x');
        QTest::newRow("rate-limit")<<429<<QByteArray("{}");
        QTest::newRow("no-release")<<404<<QByteArray("{}");
        QTest::newRow("server-error")<<500<<QByteArray("{}");
        QTest::newRow("redirect")<<302<<QByteArray("{}");
    }
    void invalidResponses() {
        QFETCH(int,httpStatus); QFETCH(QByteArray,payload); status=httpStatus; body=payload;
        UpdateTracker tracker("1.0.0",endpoint()); tracker.check(); QTRY_COMPARE(tracker.state(),"error");
        QVERIFY(!tracker.error().isEmpty()); QVERIFY(!tracker.lastChecked().isValid()); QVERIFY(tracker.releaseUrl().isEmpty());
        tracker.checkAutomatically(); QTest::qWait(30); QCOMPARE(requests,1);
        status=200; release("v1.1.0"); tracker.check(); QTRY_COMPARE(tracker.state(),"available"); QVERIFY(tracker.error().isEmpty());
    }
};
int main(int argc,char **argv) {
    QCoreApplication app(argc,argv); QStandardPaths::setTestModeEnabled(true);
    QTemporaryDir directory; qputenv("XDG_CONFIG_HOME",directory.path().toUtf8()); Settings::initialize();
    UpdateTrackerTest test; return QTest::qExec(&test,argc,argv);
}
#include "updatetracker_test.moc"
