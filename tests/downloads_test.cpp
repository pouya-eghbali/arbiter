#include "downloads_test.h"
#include "../src/downloads.h"
#include "../src/settings.h"
#include <QtTest>
#include <QtWebEngineQuick>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QTemporaryDir>
#include <QStandardPaths>
#include <QTcpServer>
#include <QTcpSocket>
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonArray>
#include <QApplication>
#include <QImage>
#ifdef Q_OS_MACOS
#include <sys/xattr.h>
#endif

void DownloadsTest::namesAndReservations() {
        QTemporaryDir directory; QVERIFY(directory.isValid());
        QCOMPARE(Downloads::safeFileName("../../report.txt"),QString("report.txt"));
        QCOMPARE(Downloads::safeFileName("C:\\folder\\report.txt"),QString("report.txt"));
        QCOMPARE(Downloads::safeFileName("CON.txt"),QString("_CON.txt"));
        QVERIFY(Downloads::safeFileName(QString(300,QChar(0x00e9)) + ".pdf").toUtf8().size() < 200);
        const auto first=Downloads::reservePath(directory.path(),"report.txt"); QVERIFY(!first.isEmpty());
        QFile existing(first); QVERIFY(existing.open(QIODevice::WriteOnly)); existing.write("keep me"); existing.close();
        const auto second=Downloads::reservePath(directory.path(),"report.txt"); QVERIFY(second.endsWith("report (1).txt"));
        QVERIFY(existing.open(QIODevice::ReadOnly)); QCOMPARE(existing.readAll(),QByteArray("keep me"));
    }
void DownloadsTest::transferRates() {
    DownloadEntry entry(nullptr);
    entry.state = QWebEngineDownloadRequest::DownloadInProgress;
    entry.total = 10000;
    entry.sampleTransfer(0);
    QCOMPARE(entry.secondsRemaining(), -1);
    entry.received = 1000; entry.sampleTransfer(1000);
    QCOMPARE(entry.bytesPerSecond(), 1000.0); QCOMPARE(entry.secondsRemaining(), 9);
    entry.paused = true; entry.sampleTransfer(1100);
    QCOMPARE(entry.bytesPerSecond(), 0.0); QCOMPARE(entry.secondsRemaining(), -1);
    entry.paused = false; entry.sampleTransfer(10000);
    entry.received = 2000; entry.sampleTransfer(11000);
    QCOMPARE(entry.bytesPerSecond(), 1000.0); QCOMPARE(entry.secondsRemaining(), 8);
    entry.total = -1; QCOMPARE(entry.secondsRemaining(), -1);
    entry.total = 10000;
    entry.sampleTransfer(15000); QCOMPARE(entry.bytesPerSecond(), 0.0); QCOMPARE(entry.secondsRemaining(), -1);
    entry.received = 4000; entry.sampleTransfer(16000); QVERIFY(entry.bytesPerSecond() > 0);
    entry.received = 10; entry.sampleTransfer(17000); QCOMPARE(entry.bytesPerSecond(), 0.0);
    entry.state = QWebEngineDownloadRequest::DownloadCompleted; entry.sampleTransfer(18000);
    QCOMPARE(entry.secondsRemaining(), -1); QCOMPARE(entry.bytesPerSecond(), 0.0);
}
void DownloadsTest::fileTypeIcons() {
    DownloadEntry entry(nullptr);
    for (const auto &name : {"archive.zip", "document.pdf", "photo.png", "unknown.arbiter-unknown-type", "file"}) {
        entry.fileName = QString::fromLatin1(name);
        const auto url = entry.iconSource().toString();
        QVERIFY(url.startsWith("data:image/png;base64,"));
        QVERIFY(!QImage::fromData(QByteArray::fromBase64(url.mid(url.indexOf(',')+1).toLatin1())).isNull());
        QCOMPARE(entry.iconSource().toString(), url);
    }
    entry.fileName = "report.PDF"; const auto upper = entry.iconSource();
    entry.fileName = "other.pdf"; QCOMPARE(entry.iconSource(), upper);
}
void DownloadsTest::actualTransfers() {
        QTemporaryDir directory; QVERIFY(directory.isValid());
        qputenv("XDG_CONFIG_HOME",directory.path().toUtf8()); qputenv("XDG_CONFIG_DIRS",directory.path().toUtf8());
        DownloadSettings preferences; preferences.setDirectory(QUrl::fromLocalFile(directory.path()));
        Downloads downloads(&preferences,directory.filePath("history.json"));
        QTcpServer server; QVERIFY(server.listen(QHostAddress::LocalHost));
        const QByteArray content(512*1024,'d'); int requests=0; bool allowRetry=false;
        connect(&server,&QTcpServer::newConnection,this,[&] {
            auto *socket=server.nextPendingConnection();
            connect(socket,&QTcpSocket::readyRead,socket,[&,socket] {
                const auto data=socket->property("request").toByteArray()+socket->readAll(); socket->setProperty("request",data);
                if(!data.contains("\r\n\r\n") || socket->property("answered").toBool()) return;
                socket->setProperty("answered",true); ++requests;
                if(data.startsWith("GET /page")) {
                    const QByteArray body("<!doctype html><a id='file' href='/file'>Download</a><a id='slow' href='/slow'>Slow</a><a id='retry' href='/retry'>Retry</a>");
                    socket->write("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: "+QByteArray::number(body.size())+"\r\nConnection: close\r\n\r\n"+body); socket->disconnectFromHost(); return;
                }
                if(data.startsWith("GET /retry")) {
                    const auto range=QRegularExpression("Range: bytes=(\\d+)-",QRegularExpression::CaseInsensitiveOption).match(QString::fromLatin1(data));
                    const int start=range.hasMatch() ? range.captured(1).toInt() : 0;
                    socket->write(QByteArray(range.hasMatch() ? "HTTP/1.1 206 Partial Content\r\n" : "HTTP/1.1 200 OK\r\n")
                        + "Content-Type: application/octet-stream\r\nContent-Disposition: attachment; filename=retry.bin\r\nAccept-Ranges: bytes\r\nETag: \"download-test\"\r\n"
                        + (range.hasMatch() ? "Content-Range: bytes "+QByteArray::number(start)+"-"+QByteArray::number(content.size()-1)+"/"+QByteArray::number(content.size())+"\r\n" : QByteArray())
                        + "Content-Length: "+QByteArray::number(content.size()-start)+"\r\nConnection: close\r\n\r\n");
                    socket->write(allowRetry ? content.mid(start) : content.mid(start,4096)); socket->disconnectFromHost(); return;
                }
                const bool slow=data.startsWith("GET /slow");
                socket->write("HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\nContent-Disposition: attachment; filename=report.bin\r\nAccept-Ranges: bytes\r\nContent-Length: "+QByteArray::number(slow ? 64*1024*1024 : content.size())+"\r\nConnection: close\r\n\r\n");
                if(!slow) { socket->write(content); socket->disconnectFromHost(); }
                else {
                    auto *timer=new QTimer(socket); timer->setInterval(30);
                    connect(timer,&QTimer::timeout,socket,[socket] { if(socket->state()==QAbstractSocket::ConnectedState) socket->write(QByteArray(4096,'s')); }); timer->start();
                }
            }); connect(socket,&QTcpSocket::disconnected,socket,&QObject::deleteLater);
        });
        QQmlApplicationEngine engine;
#ifdef ARBITER_WEBENGINE_QML_PATH
        engine.addImportPath(QString::fromUtf8(ARBITER_WEBENGINE_QML_PATH));
#endif
        engine.loadData(R"QML(
import QtQuick
import QtWebEngine
Window {
    visible: true; width: 800; height: 500
    property alias testProfile: profile
    property alias testView: view
    property alias privateProfile: privateProfile
    property alias privateView: privateView
    WebEngineProfile { id: profile; offTheRecord: true }
    WebEngineProfile { id: privateProfile; offTheRecord: true }
    WebEngineView { id: privateView; width: 100; height: 100; visible: false; profile: privateProfile }
    WebEngineView { id: view; anchors.fill: parent; profile: profile }
    function retryDownload() { view.mainFrame.runJavaScript("document.getElementById(\"retry\").click()",0,function(){}) }
    function savePage(path) { view.save(path, WebEngineView.MimeHtmlSaveFormat) }
    function clickDownload(slow) { view.mainFrame.runJavaScript("document.getElementById('"+(slow ? 'slow' : 'file')+"').click()",0,function(){}) }
})QML",QUrl("qrc:/downloads-test.qml"));
        QTRY_VERIFY(!engine.rootObjects().isEmpty()); auto *root=engine.rootObjects().first();
        auto *profile=qobject_cast<QQuickWebEngineProfile *>(root->property("testProfile").value<QObject *>()); QVERIFY(profile);
        auto *view=root->property("testView").value<QObject *>(); QVERIFY(view);
        downloads.trackProfile(profile); downloads.trackProfile(profile);
        view->setProperty("url",QUrl(QString("http://127.0.0.1:%1/page").arg(server.serverPort())));
        QTRY_VERIFY(requests>0 && !view->property("loading").toBool());
        QVERIFY(QMetaObject::invokeMethod(root,"clickDownload",Q_ARG(QVariant,false)));
        QTRY_COMPARE_WITH_TIMEOUT(downloads.items().size(),1,10000);
        auto *first=downloads.items().first().value<DownloadEntry *>(); QVERIFY(first);
        QTRY_VERIFY_WITH_TIMEOUT(first->completed(),15000);
        QCOMPARE(first->received,qint64(content.size())); QCOMPARE(downloads.activeCount(),0);
        QFile file(first->path); QVERIFY(file.open(QIODevice::ReadOnly)); QCOMPARE(file.readAll(),content); file.close();
#ifdef Q_OS_MACOS
        QVERIFY(getxattr(QFile::encodeName(first->path).constData(),"com.apple.quarantine",nullptr,0,0,0)>0);
#endif
        QVERIFY(QMetaObject::invokeMethod(root,"clickDownload",Q_ARG(QVariant,false)));
        QTRY_COMPARE_WITH_TIMEOUT(downloads.items().size(),2,10000);
        auto *second=downloads.items().first().value<DownloadEntry *>(); QTRY_VERIFY_WITH_TIMEOUT(second->completed(),15000);
        QVERIFY(first->path!=second->path); QCOMPARE(second->fileName,QString("report (1).bin"));
        QVERIFY(QMetaObject::invokeMethod(root,"clickDownload",Q_ARG(QVariant,true)));
        QTRY_COMPARE_WITH_TIMEOUT(downloads.items().size(),3,10000);
        auto *slow=downloads.items().first().value<DownloadEntry *>();
        QTRY_VERIFY_WITH_TIMEOUT(slow->received>0 && slow->active(),10000); QCOMPARE(downloads.activeCount(),1);
        QTRY_VERIFY_WITH_TIMEOUT(slow->bytesPerSecond()>0 && slow->secondsRemaining()>0,10000);
        downloads.pause(slow->id); QTRY_VERIFY(slow->paused); QCOMPARE(slow->bytesPerSecond(),0.0); QCOMPARE(slow->secondsRemaining(),-1);
        downloads.resume(slow->id); QTRY_VERIFY(!slow->paused);
        downloads.cancel(slow->id); QTRY_COMPARE(slow->state,QWebEngineDownloadRequest::DownloadCancelled);
        QTRY_COMPARE(downloads.activeCount(),0);
        QVERIFY(QMetaObject::invokeMethod(root,"retryDownload"));
        QTRY_COMPARE_WITH_TIMEOUT(downloads.items().size(),4,10000);
        auto *interrupted=downloads.items().first().value<DownloadEntry *>();
        QTRY_VERIFY_WITH_TIMEOUT(interrupted->state==QWebEngineDownloadRequest::DownloadInterrupted,20000);
        QVERIFY(!interrupted->error.isEmpty());
        QVERIFY2(interrupted->canRetry(),qPrintable(interrupted->error)); allowRetry=true;
        downloads.retry(interrupted->id); QTRY_VERIFY_WITH_TIMEOUT(interrupted->completed(),15000);
        QVERIFY(interrupted->error.isEmpty());
        QFile retried(interrupted->path); QVERIFY(retried.open(QIODevice::ReadOnly)); QCOMPARE(retried.readAll(),content); retried.close();
        const QString savePath=directory.filePath("saved-page.mhtml");
        QVERIFY(QMetaObject::invokeMethod(root,"savePage",Q_ARG(QVariant,savePath)));
        QTRY_COMPARE_WITH_TIMEOUT(downloads.items().size(),5,10000);
        auto *saved=downloads.items().first().value<DownloadEntry *>(); QTRY_VERIFY_WITH_TIMEOUT(saved->completed(),15000);
        QCOMPARE(saved->path,savePath); QVERIFY(QFileInfo(savePath).size()>0);
        downloads.setBlocked(true); QVERIFY(QMetaObject::invokeMethod(root,"clickDownload",Q_ARG(QVariant,false))); QTest::qWait(300);
        QCOMPARE(downloads.items().size(),5); downloads.setBlocked(false);
        QTRY_VERIFY(QFileInfo::exists(directory.filePath("history.json")));
        QTest::qWait(600);
        Downloads restored(&preferences,directory.filePath("history.json")); QCOMPARE(restored.items().size(),5);
        QCOMPARE(restored.items()[3].value<DownloadEntry *>()->fileName,second->fileName);
        Downloads privateDownloads(&preferences,QString()); QVERIFY(privateDownloads.privateMode()); QVERIFY(privateDownloads.items().isEmpty());
        auto *privateProfile=qobject_cast<QQuickWebEngineProfile *>(root->property("privateProfile").value<QObject *>()); QVERIFY(privateProfile);
        privateDownloads.trackProfile(privateProfile);
        root->property("privateView").value<QObject *>()->setProperty("url",QUrl(QString("http://127.0.0.1:%1/file").arg(server.serverPort())));
        QTRY_COMPARE_WITH_TIMEOUT(privateDownloads.items().size(),1,10000);
        QTRY_VERIFY_WITH_TIMEOUT(privateDownloads.items().first().value<DownloadEntry *>()->completed(),15000);
        QCOMPARE(downloads.items().size(),5);
        const auto completedPath=first->path;
        downloads.clearFinished(); QVERIFY(downloads.items().isEmpty()); QVERIFY(QFileInfo::exists(completedPath));
    }
int main(int argc,char **argv) {
    QtWebEngineQuick::initialize(); QApplication app(argc,argv); QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setOrganizationName("ArbiterTests"); QCoreApplication::setApplicationName("DownloadsTests");
    DownloadsTest test; return QTest::qExec(&test,argc,argv);
}

