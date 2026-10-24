#include "browserinstance.h"
#include <QLocalSocket>
#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QDir>
#include <QFileInfo>
#include <QThread>
#include <QTimer>
namespace {
bool webUrl(const QUrl &url) { return url.isValid() && !url.host().isEmpty() && (url.scheme()=="http" || url.scheme()=="https"); }
}
BrowserInstance::BrowserInstance(const QString &directory,QObject *parent):QObject(parent),m_directory(directory) {
    connect(&m_server,&QLocalServer::newConnection,this,[this] {
        while(auto *socket=m_server.nextPendingConnection()) {
            socket->setReadBufferSize(65537);
            auto *timer=new QTimer(socket); timer->setSingleShot(true); timer->start(3000);
            connect(timer,&QTimer::timeout,socket,&QLocalSocket::disconnectFromServer);
            connect(socket,&QLocalSocket::disconnected,socket,&QObject::deleteLater);
            connect(socket,&QLocalSocket::readyRead,this,[this,socket] {
                auto packet=socket->property("packet").toByteArray()+socket->readAll();
                if(packet.size()>65536) { socket->disconnectFromServer(); return; }
                if(!packet.endsWith('\n')) { socket->setProperty("packet",packet); return; }
                QJsonParseError error;
                const auto data=QJsonDocument::fromJson(packet,&error);
                if(error.error!=QJsonParseError::NoError || !data.isArray() || data.array().size()>32) { socket->disconnectFromServer(); return; }
                for(const auto &value:data.array()) { const QUrl url(value.toString()); if(webUrl(url)) emit urlReceived(url); }
                emit activationRequested();
                socket->write("ok\n"); socket->flush(); socket->disconnectFromServer();
            });
        }
    });
}
BrowserInstance::~BrowserInstance() { m_server.close(); if(m_lock) m_lock->unlock(); }
BrowserInstance::Result BrowserInstance::start(const QStringList &arguments) {
    if(!QDir().mkpath(m_directory)) { m_error=tr("Could not create the browser profile directory."); return Failed; }
    const auto directory=QFileInfo(m_directory).canonicalFilePath();
    m_name="arbiter-"+QString::fromLatin1(QCryptographicHash::hash(directory.toUtf8(),QCryptographicHash::Sha256).toHex().left(32));
    m_lock=std::make_unique<QLockFile>(directory+"/instance.lock"); m_lock->setStaleLockTime(0);
    if(!m_lock->tryLock(0)) {
        QJsonArray urls;
        for(const auto &argument:arguments) { const QUrl url(argument); if(webUrl(url) && urls.size()<32) urls.append(url.toString(QUrl::FullyEncoded)); }
        const QByteArray packet=QJsonDocument(urls).toJson(QJsonDocument::Compact)+'\n';
        if(packet.size()>65536) { m_error=tr("The external links are too long."); return Failed; }
        // The profile owner may still be starting its local server.
        QLocalSocket socket;
        for(int attempt=0;attempt<20;++attempt) {
            socket.connectToServer(m_name);
            if(socket.waitForConnected(100)) break;
            socket.abort(); QThread::msleep(50);
        }
        if(socket.state()==QLocalSocket::ConnectedState) {
            socket.write(packet);
            if((socket.bytesToWrite()==0 || socket.waitForBytesWritten(1000)) && (socket.bytesAvailable()>0 || socket.waitForReadyRead(3000)) && socket.readLine()=="ok\n") return Forwarded;
        }
        m_error=tr("Arbiter's profile is already in use, but its window could not be reached. Close the other instance and try again.");
        return Failed;
    }
    QLocalServer::removeServer(m_name);
    m_server.setSocketOptions(QLocalServer::UserAccessOption);
    if(!m_server.listen(m_name)) { m_error=tr("Could not start Arbiter's local link handler."); m_lock->unlock(); return Failed; }
    return Primary;
}
