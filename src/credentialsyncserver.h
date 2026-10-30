#pragma once
#include <QObject>
#include <QSslServer>
#include <QSslCertificate>
#include <QSslKey>
#include <functional>

// A deliberately narrow LAN-only endpoint, not a general-purpose HTTP server.
// Authentication and the encrypted payload are handled by CredentialSync.
class CredentialSyncServer : public QObject {
    Q_OBJECT
public:
    using Reply=std::function<void(int,const QByteArray &)>;
    using Handler=std::function<void(const QString &,const QByteArray &,const QByteArray &,Reply)>;
    explicit CredentialSyncServer(QObject *parent=nullptr);
    static bool createIdentity(QByteArray *certificatePem,QByteArray *privateKeyPem);
    static bool localAddress(const QHostAddress &address);
    bool start(const QByteArray &certificatePem,const QByteArray &privateKeyPem,quint16 port,Handler handler);
    void stop();
    quint16 port() const { return m_server.serverPort(); }
    QString error() const { return m_server.errorString(); }
private:
    void acceptConnections();
    QSslServer m_server;
    Handler m_handler;
    int m_active=0;
};
