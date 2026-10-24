#include "../src/browserpreferences.h"
#include "../src/settings.h"
#include <QtTest>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QStandardPaths>
#include <QImage>
#include <QBuffer>
#include <QUrlQuery>
#include <QGuiApplication>
#include <QClipboard>
#include <QMimeData>
class BrowserPreferencesTest : public QObject {
    Q_OBJECT
private slots:
    void search() {
        BrowserPreferences preferences;
        for (const QString engine : {"duckduckgo","google","bing","brave","ecosia","startpage"}) {
            preferences.setSearchEngine(engine);
            const QString input="snow & ice + café #1?";
            auto url=preferences.searchUrl(input);
            QCOMPARE(url.scheme(),"https");
            QVERIFY(url.toEncoded().contains("%2B"));
            QVERIFY(url.toEncoded().contains("%26"));
            QCOMPARE(QUrlQuery(url).queryItemValue(engine=="startpage" ? "query" : "q",QUrl::FullyDecoded),input);
            BrowserPreferences restored; QCOMPARE(restored.searchEngine(),engine);
        }
        preferences.setSearchEngine("invalid"); QCOMPARE(preferences.searchEngine(),"startpage");
        QCOMPARE(preferences.siteOrigin(QUrl("https://user:secret@example.com:8443/path?q=secret#fragment")),"https://example.com:8443");
        QVERIFY(preferences.siteOrigin(QUrl("file:///tmp/test")).isEmpty());
    }
    void shareLinks() {
        BrowserPreferences preferences;
        const QUrl original("https://user:secret@example.com/a?q=one%20two#section");
        const QUrl shared("https://example.com/a?q=one%20two#section");
        QCOMPARE(preferences.shareableUrl(original),shared);
        QVERIFY(preferences.copyLink(original));
        QCOMPARE(QGuiApplication::clipboard()->text(),shared.toString(QUrl::FullyEncoded));
        QCOMPARE(QGuiApplication::clipboard()->mimeData()->urls(),QList<QUrl>{shared});
        for(const auto &input:{"about:blank","arbiter://settings","javascript:alert(1)","data:text/plain,test","https:///"}) {
            QVERIFY(preferences.shareableUrl(QUrl(input)).isEmpty());
            QVERIFY(!preferences.copyLink(QUrl(input)));
        }
        QCOMPARE(QGuiApplication::clipboard()->text(),shared.toString(QUrl::FullyEncoded));
        QVERIFY(preferences.shareableUrl(QUrl("file:///tmp/document.pdf")).isLocalFile());
    }
    void cacheFirst() {
        QTcpServer server; QVERIFY(server.listen(QHostAddress::LocalHost));
        QImage image(16,16,QImage::Format_ARGB32); image.fill(Qt::red);
        QByteArray png; QBuffer buffer(&png); buffer.open(QIODevice::WriteOnly); QVERIFY(image.save(&buffer,"PNG"));
        int requests=0;
        connect(&server,&QTcpServer::newConnection,this,[&] {
            auto *socket=server.nextPendingConnection();
            connect(socket,&QTcpSocket::readyRead,socket,[&,socket] {
                const auto request=socket->readAll(); if (!request.contains("\r\n\r\n")) return;
                ++requests;
                socket->write("HTTP/1.1 200 OK\r\nContent-Type: image/png\r\nConnection: close\r\nContent-Length: "+QByteArray::number(png.size())+"\r\n\r\n"+png);
                socket->disconnectFromHost();
            });
            connect(socket,&QTcpSocket::disconnected,socket,&QObject::deleteLater);
        });
        const QUrl page(QString("http://127.0.0.1:%1/a").arg(server.serverPort()));
        BrowserPreferences preferences;
        QVERIFY(preferences.favicon(page,false).isEmpty()); QCOMPARE(requests,0);
        QSignalSpy ready(&preferences,&BrowserPreferences::faviconReady);
        preferences.favicon(page); preferences.favicon(page);
        QTRY_COMPARE(ready.count(),1); QCOMPARE(requests,1);
        const QUrl cached=preferences.favicon(page); QVERIFY(cached.isLocalFile());
        server.close();
        BrowserPreferences restored;
        QCOMPARE(restored.favicon(page),cached);
        QVERIFY(!QImage(cached.toLocalFile()).isNull());
        QFile::remove(cached.toLocalFile());
    }
};
int main(int argc,char **argv) {
    QGuiApplication app(argc,argv);
    QStandardPaths::setTestModeEnabled(true);
    QTemporaryDir config; qputenv("XDG_CONFIG_HOME",config.path().toUtf8());
    Settings::initialize();
    BrowserPreferencesTest test; return QTest::qExec(&test,argc,argv);
}
#include "browserpreferences_test.moc"
