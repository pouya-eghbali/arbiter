#include "credentialsyncserver.h"
#include "credentialsyncstate.h"
#include <QPointer>
#include <QSslSocket>
#include <QTimer>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <memory>

CredentialSyncServer::CredentialSyncServer(QObject *parent):QObject(parent),m_server(this) {
    m_server.setMaxPendingConnections(8);
    // TLS handshakes finish after QTcpServer::newConnection. Read only sockets
    // that QSslServer has actually added to the encrypted pending queue.
    connect(&m_server,&QSslServer::pendingConnectionAvailable,this,&CredentialSyncServer::acceptConnections);
}
bool CredentialSyncServer::createIdentity(QByteArray *certificatePem,QByteArray *privateKeyPem) {
    // Secure Transport cannot import the generated EC identity on macOS. RSA
    // identities also fit Windows Credential Manager (2048-bit PKCS#8 < 2560 B).
    std::unique_ptr<EVP_PKEY,decltype(&EVP_PKEY_free)> key(EVP_PKEY_Q_keygen(nullptr,nullptr,"RSA",size_t(2048)),EVP_PKEY_free);
    std::unique_ptr<X509,decltype(&X509_free)> cert(X509_new(),X509_free);
    if(!key || !cert || X509_set_version(cert.get(),2)!=1 || ASN1_INTEGER_set(X509_get_serialNumber(cert.get()),1)!=1) return false;
    X509_gmtime_adj(X509_getm_notBefore(cert.get()),-300);
    X509_gmtime_adj(X509_getm_notAfter(cert.get()),5L*365*24*60*60);
    if(X509_set_pubkey(cert.get(),key.get())!=1) return false;
    auto *subject=X509_get_subject_name(cert.get());
    if(X509_NAME_add_entry_by_txt(subject,"CN",MBSTRING_ASC,reinterpret_cast<const unsigned char *>("Arbiter Local Sync"),-1,-1,0)!=1 || X509_set_issuer_name(cert.get(),subject)!=1 || X509_sign(cert.get(),key.get(),EVP_sha256())<=0) return false;
    std::unique_ptr<BIO,decltype(&BIO_free)> certBio(BIO_new(BIO_s_mem()),BIO_free),keyBio(BIO_new(BIO_s_mem()),BIO_free);
    if(!certBio || !keyBio || PEM_write_bio_X509(certBio.get(),cert.get())!=1 || PEM_write_bio_PrivateKey(keyBio.get(),key.get(),nullptr,nullptr,0,nullptr,nullptr)!=1) return false;
    char *bytes=nullptr; auto size=BIO_get_mem_data(certBio.get(),&bytes); *certificatePem=QByteArray(bytes,size);
    size=BIO_get_mem_data(keyBio.get(),&bytes); *privateKeyPem=QByteArray(bytes,size);
    return !certificatePem->isEmpty() && !privateKeyPem->isEmpty();
}
bool CredentialSyncServer::localAddress(const QHostAddress &address) {
    if(address.isLoopback()) return true;
    bool valid=false; const auto ip=address.toIPv4Address(&valid);
    if(valid) return (ip&0xff000000U)==0x0a000000U || (ip&0xfff00000U)==0xac100000U || (ip&0xffff0000U)==0xc0a80000U;
    const auto bytes=address.toIPv6Address();
    return (bytes[0]&0xfe)==0xfc || (bytes[0]==0xfe && (bytes[1]&0xc0)==0x80);
}
bool CredentialSyncServer::start(const QByteArray &certPem,const QByteArray &keyPem,quint16 port,Handler handler) {
    stop();
    const QSslCertificate cert(certPem); const QSslKey key(keyPem,QSsl::Rsa);
    if(cert.isNull() || key.isNull()) return false;
    auto config=QSslConfiguration::defaultConfiguration();
    config.setLocalCertificate(cert); config.setPrivateKey(key); config.setPeerVerifyMode(QSslSocket::VerifyNone);
    config.setProtocol(QSsl::TlsV1_2OrLater); m_server.setSslConfiguration(config);
    m_handler=std::move(handler); return m_server.listen(QHostAddress::AnyIPv4,port);
}
void CredentialSyncServer::stop() {
    m_server.close();
    for(auto *socket:findChildren<QSslSocket *>()) socket->abort();
    m_handler={};
}
void CredentialSyncServer::acceptConnections() {
    while(m_server.hasPendingConnections()) {
        auto *socket=qobject_cast<QSslSocket *>(m_server.nextPendingConnection());
        if(!socket) continue;
        socket->setParent(this);
        if(m_active>=8 || !localAddress(socket->peerAddress())) { socket->abort(); socket->deleteLater(); continue; }
        ++m_active;
        connect(socket,&QSslSocket::destroyed,this,[this] { --m_active; });
        connect(socket,&QSslSocket::disconnected,socket,&QObject::deleteLater);
        socket->setReadBufferSize(ArbiterSync::MaximumBytes+16384);
        auto *timer=new QTimer(socket); timer->setSingleShot(true); timer->start(30000);
        connect(timer,&QTimer::timeout,socket,[socket] { socket->abort(); });
        const QPointer<QSslSocket> guard(socket);
        auto sent=std::make_shared<bool>(false);
        const auto reply=[guard,sent,timer](int status,const QByteArray &body) {
            if(!guard || *sent) return; *sent=true; timer->stop();
            const auto reason=status==200 ? "OK" : status==401 ? "Unauthorized" : status==409 ? "Conflict" : "Error";
            guard->write("HTTP/1.1 "+QByteArray::number(status)+" "+reason+"\r\nContent-Type: application/octet-stream\r\nCache-Control: no-store\r\nConnection: close\r\nContent-Length: "+QByteArray::number(body.size())+"\r\n\r\n"+body);
            guard->disconnectFromHost();
        };
        auto buffer=std::make_shared<QByteArray>();
        auto handled=std::make_shared<bool>(false);
        const auto read=[this,guard,buffer,sent,handled,reply] {
            if(!guard || *sent || *handled) return;
            buffer->append(guard->readAll());
            if(buffer->size()>ArbiterSync::MaximumBytes+8220) { reply(413,{}); return; }
            const auto split=buffer->indexOf("\r\n\r\n");
            if(split<0) { if(buffer->size()>8192) reply(431,{}); return; }
            if(split>8192) { reply(431,{}); return; }
            const auto lines=buffer->left(split).split('\n');
            if(lines.isEmpty() || lines.size()>32) { reply(400,{}); return; }
            const auto first=lines.first().trimmed().split(' ');
            if(first.size()!=3 || first[0]!="POST" || first[2]!="HTTP/1.1" || (first[1]!="/v1/pair" && first[1]!="/v1/sync")) { reply(404,{}); return; }
            QMap<QByteArray,QByteArray> headers;
            for(int i=1;i<lines.size();++i) {
                const auto line=lines[i].trimmed(); const auto colon=line.indexOf(':');
                if(colon<=0) { reply(400,{}); return; }
                const auto name=line.left(colon).toLower(),value=line.mid(colon+1).trimmed();
                if(headers.contains(name)) { reply(400,{}); return; }
                headers.insert(name,value);
            }
            if(headers.contains("origin") || headers.contains("transfer-encoding") || headers.contains("expect")) { reply(400,{}); return; }
            bool valid=false; const auto size=headers.value("content-length").toLongLong(&valid);
            if(!valid || size<28 || size>ArbiterSync::MaximumBytes+28 || (first[1]=="/v1/pair" && size>4096)) { reply(413,{}); return; }
            if(buffer->size()<split+4+size) return;
            if(buffer->size()!=split+4+size || !headers.value("authorization").startsWith("Bearer ")) { reply(400,{}); return; }
            if(!m_handler) { reply(409,{}); return; }
            // Stop reading before handing over to asynchronous credential storage.
            const auto path=QString::fromLatin1(first[1]);
            const auto token=headers.value("authorization").mid(7);
            const auto body=buffer->mid(split+4); buffer->clear();
            *handled=true;
            m_handler(path,token,body,reply);
        };
        connect(socket,&QSslSocket::readyRead,socket,read);
        if(socket->bytesAvailable()) read();
    }
}
