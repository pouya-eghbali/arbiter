#include "credentialsyncstate.h"
#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QUrl>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <algorithm>
#include <cmath>
#include <memory>

namespace ArbiterSync {
namespace {
QByteArray canonical(const QJsonObject &object) { return QJsonDocument(object).toJson(QJsonDocument::Compact); }
bool fail(QString *error, const char *message) { if(error) *error=QString::fromLatin1(message); return false; }
bool validVersion(const QJsonObject &v) {
    const auto clock=v.value("clock").toObject();
    if(clock.isEmpty() || clock.size()>32 || !v.value("data").isObject() || !v.value("deleted").isBool()) return false;
    for(auto i=clock.begin();i!=clock.end();++i) {
        const auto n=i.value().toDouble(-1);
        if(i.key().isEmpty() || i.key().size()>80 || !i.value().isDouble() || n<1 || n>9007199254740991.0 || std::floor(n)!=n) return false;
    }
    const auto data=v.value("data").toObject();
    if(canonical(data).size()>131072) return false;
    if(v.value("deleted").toBool()) return true;
    const auto kind=data.value("kind").toString();
    if(kind=="password") {
        const QUrl url(data.value("origin").toString(),QUrl::StrictMode);
        return url.isValid() && (url.scheme()=="https" || url.scheme()=="http") && !url.host().isEmpty()
            && url.userInfo().isEmpty() && data.value("username").isString() && data.value("username").toString().size()<=4096
            && data.value("password").isString() && data.value("password").toString().size()<=65536;
    }
    if(kind=="passkey") {
        auto decode=[](const QJsonValue &s) { return QByteArray::fromBase64Encoding(s.toString().toLatin1(),QByteArray::Base64UrlEncoding|QByteArray::AbortOnBase64DecodingErrors); };
        const auto id=decode(data.value("credentialId")), key=decode(data.value("privateKey")), user=decode(data.value("userId"));
        const auto rp=data.value("rpId").toString();
        return !rp.isEmpty() && rp.size()<=253 && id && !id.decoded.isEmpty() && id.decoded.size()<=1024
            && key && !key.decoded.isEmpty() && key.decoded.size()<=16384 && user && user.decoded.size()<=64;
    }
    return false;
}
}
QByteArray seal(const QByteArray &plain,const QByteArray &key,const QByteArray &context,const QByteArray &nonce) {
    if(key.size()!=32 || plain.size()>MaximumBytes || (!nonce.isEmpty() && nonce.size()!=12)) return {};
    QByteArray iv=nonce;
    if(iv.isEmpty()) { iv.resize(12); if(RAND_bytes(reinterpret_cast<unsigned char *>(iv.data()),iv.size())!=1) return {}; }
    std::unique_ptr<EVP_CIPHER_CTX,decltype(&EVP_CIPHER_CTX_free)> c(EVP_CIPHER_CTX_new(),EVP_CIPHER_CTX_free);
    if(!c || EVP_EncryptInit_ex(c.get(),EVP_aes_256_gcm(),nullptr,reinterpret_cast<const unsigned char *>(key.constData()),reinterpret_cast<const unsigned char *>(iv.constData()))!=1) return {};
    int count=0,total=0;
    if(EVP_EncryptUpdate(c.get(),nullptr,&count,reinterpret_cast<const unsigned char *>(context.constData()),context.size())!=1) return {};
    QByteArray cipher(plain.size()+16,Qt::Uninitialized), tag(16,Qt::Uninitialized);
    if(EVP_EncryptUpdate(c.get(),reinterpret_cast<unsigned char *>(cipher.data()),&count,reinterpret_cast<const unsigned char *>(plain.constData()),plain.size())!=1) return {};
    total=count;
    if(EVP_EncryptFinal_ex(c.get(),reinterpret_cast<unsigned char *>(cipher.data())+total,&count)!=1) return {};
    cipher.resize(total+count);
    if(EVP_CIPHER_CTX_ctrl(c.get(),EVP_CTRL_GCM_GET_TAG,16,tag.data())!=1) return {};
    return iv+cipher+tag;
}
QByteArray open(const QByteArray &sealed,const QByteArray &key,const QByteArray &context,bool *ok) {
    *ok=false;
    if(key.size()!=32 || sealed.size()<28 || sealed.size()>MaximumBytes+28) return {};
    std::unique_ptr<EVP_CIPHER_CTX,decltype(&EVP_CIPHER_CTX_free)> c(EVP_CIPHER_CTX_new(),EVP_CIPHER_CTX_free);
    if(!c || EVP_DecryptInit_ex(c.get(),EVP_aes_256_gcm(),nullptr,reinterpret_cast<const unsigned char *>(key.constData()),reinterpret_cast<const unsigned char *>(sealed.constData()))!=1) return {};
    int count=0,total=0;
    if(EVP_DecryptUpdate(c.get(),nullptr,&count,reinterpret_cast<const unsigned char *>(context.constData()),context.size())!=1) return {};
    QByteArray plain(sealed.size()-28+16,Qt::Uninitialized);
    if(EVP_DecryptUpdate(c.get(),reinterpret_cast<unsigned char *>(plain.data()),&count,reinterpret_cast<const unsigned char *>(sealed.constData()+12),sealed.size()-28)!=1) return {};
    total=count;
    QByteArray tag=sealed.right(16);
    if(EVP_CIPHER_CTX_ctrl(c.get(),EVP_CTRL_GCM_SET_TAG,16,tag.data())!=1 || EVP_DecryptFinal_ex(c.get(),reinterpret_cast<unsigned char *>(plain.data())+total,&count)!=1) {
        OPENSSL_cleanse(plain.data(),plain.size()); return {};
    }
    plain.resize(total+count); *ok=true; return plain;
}
bool dominates(const QJsonObject &a,const QJsonObject &b) {
    bool greater=false;
    for(auto i=b.begin();i!=b.end();++i) if(a.value(i.key()).toDouble()<i.value().toDouble()) return false;
    for(auto i=a.begin();i!=a.end();++i) if(i.value().toDouble()>b.value(i.key()).toDouble()) greater=true;
    return greater;
}
QString passwordId(const QString &origin,const QString &username) {
    return "password:"+QString::fromLatin1(QCryptographicHash::hash(origin.toUtf8()+QByteArray(1,'\0')+username.toUtf8(),QCryptographicHash::Sha256).toHex());
}
QString passkeyId(const QString &rpId,const QByteArray &id) {
    return "passkey:"+QString::fromLatin1(QCryptographicHash::hash(rpId.toUtf8()+QByteArray(1,'\0')+id,QCryptographicHash::Sha256).toHex());
}
bool State::parse(const QJsonObject &json,State *out,QString *error) {
    if(json.value("v")!=1 || !json.value("records").isObject()) return fail(error,"Unsupported credential sync version.");
    const auto records=json.value("records").toObject();
    if(records.size()>10000) return fail(error,"Too many credential records.");
    for(auto i=records.begin();i!=records.end();++i) {
        if(i.key().isEmpty() || i.key().size()>1024 || !i.value().isArray() || i.value().toArray().size()>32) return fail(error,"Invalid credential register.");
        for(const auto &v:i.value().toArray()) if(!v.isObject() || !validVersion(v.toObject())) return fail(error,"Invalid credential version.");
    }
    out->m_records=records; return true;
}
QJsonObject State::json() const { return {{"v",1},{"records",m_records}}; }
bool State::merge(const State &other,QString *error) {
    auto next=m_records;
    for(auto i=other.m_records.begin();i!=other.m_records.end();++i) {
        QMap<QByteArray,QJsonObject> unique;
        auto versions=next.value(i.key()).toArray();
        for(const auto &v:i.value().toArray()) versions.append(v);
        for(const auto &v:versions) unique.insert(canonical(v.toObject()),v.toObject());
        QJsonArray retained;
        for(auto a=unique.begin();a!=unique.end();++a) {
            const bool obsolete=std::any_of(unique.begin(),unique.end(),[&a](const QJsonObject &b) { return dominates(b.value("clock").toObject(),a.value().value("clock").toObject()); });
            if(!obsolete) retained.append(a.value());
        }
        if(retained.size()>32) return fail(error,"Too many conflicting credential edits.");
        next.insert(i.key(),retained);
    }
    if(next.size()>10000 || canonical({{"records",next}}).size()>MaximumBytes) return fail(error,"Credential sync data is too large.");
    m_records=next; return true;
}
bool State::update(const QString &id,const QString &device,const QJsonObject &data,bool deleted,QString *error) {
    QJsonObject clock;
    for(const auto &v:m_records.value(id).toArray()) {
        const auto c=v.toObject().value("clock").toObject();
        for(auto i=c.begin();i!=c.end();++i) clock.insert(i.key(),std::max(clock.value(i.key()).toDouble(),i.value().toDouble()));
    }
    clock.insert(device,clock.value(device).toDouble()+1);
    QJsonObject version{{"clock",clock},{"data",data},{"deleted",deleted}};
    if(id.isEmpty() || id.size()>1024 || !validVersion(version)) return fail(error,"Invalid credential edit.");
    auto next=m_records; next.insert(id,QJsonArray{version});
    State candidate; if(!parse({{"v",1},{"records",next}},&candidate,error) || canonical(candidate.json()).size()>MaximumBytes) return fail(error,"Credential sync data is too large.");
    m_records=next; return true;
}
int State::conflicts() const { int n=0; for(const auto &v:m_records) if(v.toArray().size()>1) ++n; return n; }
}
