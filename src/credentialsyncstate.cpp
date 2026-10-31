#include "credentialsyncstate.h"
#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QUrl>
#include <algorithm>
#include <cmath>

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
    if(v.contains("author") && (!v.value("author").isString() || !clock.contains(v.value("author").toString()))) return false;
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
    const auto devices=json.value("devices").toObject();
    if((json.contains("devices") && !json.value("devices").isObject()) || devices.size()>64) return fail(error,"Invalid device directory.");
    for(auto i=devices.begin();i!=devices.end();++i) {
        const auto d=i.value().toObject(); const auto revision=d.value("revision").toDouble();
        if(i.key().isEmpty() || i.key().size()>80 || !i.value().isObject() || d.value("name").toString().isEmpty() || d.value("name").toString().size()>80 || revision<1 || revision>9007199254740991.0 || std::floor(revision)!=revision)
            return fail(error,"Invalid device directory entry.");
    }
    out->m_records=records; out->m_devices=devices; return true;
}
QJsonObject State::json() const { return {{"v",1},{"records",m_records},{"devices",m_devices}}; }
QJsonObject State::deviceNames() const {
    QJsonObject names; for(auto i=m_devices.begin();i!=m_devices.end();++i) names[i.key()]=i.value().toObject().value("name"); return names;
}
void State::rememberDevice(const QString &id,const QString &name) {
    if(id.isEmpty() || id.size()>80 || name.isEmpty() || name.size()>80 || (!m_devices.contains(id) && m_devices.size()>=64)) return;
    const auto previous=m_devices.value(id).toObject();
    if(previous.value("name").toString()==name) return;
    const auto revision=previous.value("revision").toDouble()+1;
    if(revision>9007199254740991.0) return;
    m_devices[id]=QJsonObject{{"name",name},{"revision",revision}};
}
QString State::digest() const { return QString::fromLatin1(QCryptographicHash::hash(QJsonDocument(json()).toJson(QJsonDocument::Compact),QCryptographicHash::Sha256).toHex()); }
bool State::merge(const State &other,QString *error) {
    auto next=m_records;
    auto devices=m_devices;
    for(auto i=other.m_devices.begin();i!=other.m_devices.end();++i) {
        const auto remote=i.value().toObject(),local=devices.value(i.key()).toObject();
        if(remote.value("revision").toDouble()>local.value("revision").toDouble() ||
            (remote.value("revision")==local.value("revision") && remote.value("name").toString()>local.value("name").toString())) devices[i.key()]=remote;
    }
    if(devices.size()>64) return fail(error,"Too many device identities.");
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
        // Identical edits from independent imports are not a user conflict.
        // Retain their combined causal history, including for repeated deletes.
        QMap<QByteArray,QJsonObject> equivalent;
        for (const auto &entry:retained) {
            const auto v=entry.toObject();
            const auto identity=canonical({{"data",v.value("data")},{"deleted",v.value("deleted")}});
            auto combined=equivalent.value(identity,v);
            auto clock=combined.value("clock").toObject();
            const auto other=v.value("clock").toObject();
            for(auto c=other.begin();c!=other.end();++c) clock[c.key()]=std::max(clock.value(c.key()).toDouble(),c.value().toDouble());
            if(clock.size()>32) return fail(error,"Too many devices in the credential history.");
            combined["clock"]=clock; equivalent[identity]=combined;
        }
        retained={}; for(const auto &v:equivalent) retained.append(v);
        if(retained.size()>32) return fail(error,"Too many conflicting credential edits.");
        next.insert(i.key(),retained);
    }
    if(next.size()>10000 || canonical({{"records",next}}).size()>MaximumBytes) return fail(error,"Credential sync data is too large.");
    m_records=next; m_devices=devices; return true;
}
bool State::update(const QString &id,const QString &device,const QJsonObject &data,bool deleted,QString *error) {
    QJsonObject clock;
    for(const auto &v:m_records.value(id).toArray()) {
        const auto c=v.toObject().value("clock").toObject();
        for(auto i=c.begin();i!=c.end();++i) clock.insert(i.key(),std::max(clock.value(i.key()).toDouble(),i.value().toDouble()));
    }
    clock.insert(device,clock.value(device).toDouble()+1);
    QJsonObject version{{"clock",clock},{"data",data},{"deleted",deleted},{"author",device}};
    if(id.isEmpty() || id.size()>1024 || !validVersion(version)) return fail(error,"Invalid credential edit.");
    auto next=m_records; next.insert(id,QJsonArray{version});
    State candidate; if(!parse({{"v",1},{"records",next}},&candidate,error) || canonical(candidate.json()).size()>MaximumBytes) return fail(error,"Credential sync data is too large.");
    m_records=next; return true;
}
int State::conflicts() const { int n=0; for(const auto &v:m_records) if(v.toArray().size()>1) ++n; return n; }
QJsonArray State::conflictChoices() const {
    QJsonArray result;
    for (auto i=m_records.begin(); i!=m_records.end(); ++i) {
        const auto versions=i.value().toArray(); if (versions.size()<2) continue;
        const auto generation=QString::fromLatin1(QCryptographicHash::hash(QJsonDocument(versions).toJson(QJsonDocument::Compact),QCryptographicHash::Sha256).toHex());
        QJsonObject description;
        for (const auto &v:versions) if (!v.toObject().value("deleted").toBool()) description=v.toObject().value("data").toObject();
        for (const auto &v:versions) {
            const auto version=v.toObject(); auto data=version.value("data").toObject();
            if (version.value("deleted").toBool()) data=description;
            data.remove("password"); data.remove("privateKey");
            data["id"]=i.key(); data["deleted"]=version.value("deleted");
            data["generation"]=generation;
            data["version"]=QString::fromLatin1(QCryptographicHash::hash(QJsonDocument(version).toJson(QJsonDocument::Compact),QCryptographicHash::Sha256).toHex());
            data["device"]=version.value("author").toString(version.value("clock").toObject().keys().join(", "));
            result.append(data);
        }
    }
    return result;
}
bool State::resolve(const QString &id,const QString &version,const QString &generation,const QString &device,QString *error) {
    const auto versions=m_records.value(id).toArray();
    if (versions.size()<2 || generation!=QString::fromLatin1(QCryptographicHash::hash(QJsonDocument(versions).toJson(QJsonDocument::Compact),QCryptographicHash::Sha256).toHex()))
        return fail(error,"Credential changes were updated. Review them again.");
    for (const auto &v:versions) if (QString::fromLatin1(QCryptographicHash::hash(QJsonDocument(v.toObject()).toJson(QJsonDocument::Compact),QCryptographicHash::Sha256).toHex())==version) {
        const auto selected=v.toObject(); return update(id,device,selected.value("data").toObject(),selected.value("deleted").toBool(),error);
    }
    return fail(error,"This conflict choice is no longer available.");
}
}
