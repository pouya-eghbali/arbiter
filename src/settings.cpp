#include "settings.h"
#include <toml.hpp>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLockFile>
#include <QSaveFile>
#include <sstream>
#include <vector>

namespace {
constexpr auto permissions=QFileDevice::ReadOwner|QFileDevice::WriteOwner|QFileDevice::ExeOwner;
QSettings::Status load(const QString &path,toml::table &table) {
    if(!QFileInfo::exists(path)) return QSettings::NoError;
    QFile file(path);
    if(!file.open(QIODevice::ReadOnly)) return QSettings::AccessError;
    if(file.size()>1024*1024) return QSettings::FormatError;
    const auto bytes=file.readAll();
    if(file.error()!=QFileDevice::NoError) return QSettings::AccessError;
    try { table=toml::parse(std::string_view(bytes.constData(),size_t(bytes.size()))); }
    catch(const toml::parse_error &) { return QSettings::FormatError; }
    return QSettings::NoError;
}
const toml::node *find(const toml::table &table,const QString &key) {
    const toml::node *node=&table;
    for(const auto &part:key.split('/')) {
        if(!node || !node->is_table()) return nullptr;
        node=node->as_table()->get(part.toStdString());
    }
    return node;
}
QVariant variant(const toml::node &node) {
    if(const auto *v=node.as_string()) return QString::fromStdString(v->get());
    if(const auto *v=node.as_boolean()) return v->get();
    if(const auto *v=node.as_integer()) return QVariant::fromValue<qlonglong>(v->get());
    if(const auto *v=node.as_floating_point()) return v->get();
    if(const auto *v=node.as_array()) {
        QVariantList list; for(const auto &item:*v) list.append(variant(item)); return list;
    }
    if(const auto *v=node.as_table()) {
        QVariantMap map; for(const auto &[key,value]:*v) map.insert(QString::fromStdString(std::string(key.str())),variant(value)); return map;
    }
    return {}; // Date/time and unknown values remain intact in the TOML tree.
}
bool assign(toml::table &table,const QString &key,const QVariant &value,bool remove) {
    auto parts=key.split('/');
    if(parts.isEmpty() || parts.contains(QString())) return false;
    toml::table *parent=&table;
    for(int i=0;i<parts.size()-1;++i) {
        const auto part=parts[i].toStdString();
        auto *node=parent->get(part);
        if(!node) { if(remove) return true; parent->insert(part,toml::table{}); node=parent->get(part); }
        if(!node->is_table()) return false;
        parent=node->as_table();
    }
    const auto leaf=parts.last().toStdString();
    if(remove) { parent->erase(leaf); return true; }
    switch(value.metaType().id()) {
    case QMetaType::Bool: parent->insert_or_assign(leaf,value.toBool()); break;
    case QMetaType::Int: case QMetaType::UInt: case QMetaType::LongLong: case QMetaType::ULongLong:
        parent->insert_or_assign(leaf,int64_t(value.toLongLong())); break;
    case QMetaType::Double: case QMetaType::Float: parent->insert_or_assign(leaf,value.toDouble()); break;
    case QMetaType::QString: parent->insert_or_assign(leaf,value.toString().toStdString()); break;
    case QMetaType::QStringList: {
        toml::array array;
        for (const auto &item : value.toStringList()) array.push_back(item.toStdString());
        parent->insert_or_assign(leaf,std::move(array)); break;
    }
    default: return false;
    }
    return true;
}
}
struct Settings::Impl {
    QString path=Settings::configFile();
    toml::table user;
    std::vector<toml::table> defaults;
    QSettings::Status status=QSettings::NoError;
    bool readable=true;
};
QString Settings::configFile() {
    const auto home=qEnvironmentVariable("XDG_CONFIG_HOME");
    const auto base=!home.isEmpty() && QDir::isAbsolutePath(home) ? home : QDir::homePath()+"/.config";
    return QDir(base).filePath("arbiter/settings.toml");
}
QStringList Settings::systemFiles() {
    auto directories=qEnvironmentVariable("XDG_CONFIG_DIRS");
    if(directories.isEmpty()) directories=QStringLiteral("/etc/xdg");
    QStringList files;
    for(const auto &directory:directories.split(QDir::listSeparator(),Qt::SkipEmptyParts))
        if(QDir::isAbsolutePath(directory)) files.append(QDir(directory).filePath("arbiter/settings.toml"));
    return files;
}
Settings::Settings() : m(std::make_unique<Impl>()) {
    m->status=load(m->path,m->user); m->readable=m->status==QSettings::NoError;
    for(const auto &file:systemFiles()) {
        toml::table defaults;
        if(load(file,defaults)==QSettings::NoError) m->defaults.push_back(std::move(defaults));
    }
}
Settings::~Settings()=default;
QVariant Settings::value(const QString &key,const QVariant &fallback) const {
    if(m->readable) if(const auto *node=find(m->user,key)) { const auto v=variant(*node); return v.isValid() ? v : fallback; }
    for(const auto &defaults:m->defaults) if(const auto *node=find(defaults,key)) { const auto v=variant(*node); return v.isValid() ? v : fallback; }
    return fallback;
}
bool Settings::contains(const QString &key) const { return m->readable && find(m->user,key); }
void Settings::setValue(const QString &key,const QVariant &value) { update(key,value,false,false); }
void Settings::remove(const QString &key) { update(key,{},true,false); }
void Settings::clear() { update({}, {},false,true); }
void Settings::sync() { /* Every mutation is committed synchronously. */ }
QSettings::Status Settings::status() const { return m->status; }
QString Settings::fileName() const { return m->path; }
void Settings::update(const QString &key,const QVariant &value,bool remove,bool clear) {
    if(!m->readable) { qWarning()<<"Settings could not be read; changes were not saved:"<<m->path; return; }
    const auto fail=[this](QSettings::Status status) { m->status=status; qWarning()<<"Could not save settings:"<<m->path; };
    if(!QDir().mkpath(QFileInfo(m->path).absolutePath(),permissions)) { fail(QSettings::AccessError); return; }
    const QFileInfo info(m->path);
    // Resolve the target before locking and replacing it, preserving dotfile symlinks.
    const QString target=info.isSymLink() ? info.canonicalFilePath() : m->path;
    if(target.isEmpty()) { fail(QSettings::AccessError); return; }
    QLockFile lock(target+".lock");
    if(!lock.tryLock(1000)) { fail(QSettings::AccessError); return; }
    toml::table latest;
    const auto loaded=load(target,latest);
    if(loaded!=QSettings::NoError) { m->readable=false; fail(loaded); return; }
    if(clear) latest=toml::table{};
    else if(!assign(latest,key,value,remove)) { fail(QSettings::FormatError); return; }
    std::ostringstream output; output<<toml::toml_formatter(latest);
    const auto bytes=QByteArray::fromStdString(output.str())+'\n';
    QSaveFile file(target);
    if(!file.open(QIODevice::WriteOnly) || file.write(bytes)!=bytes.size() || !file.commit()) { fail(QSettings::AccessError); return; }
    if(!info.exists()) QFile::setPermissions(target,QFileDevice::ReadOwner|QFileDevice::WriteOwner);
    m->user=std::move(latest); m->status=QSettings::NoError;
}
QString Settings::initialize() {
    const auto path=configFile();
    if(!QDir().mkpath(QFileInfo(path).absolutePath(),permissions))
        return QStringLiteral("Could not create the settings directory. Changes cannot be saved.");
    Settings destination;
    if(QFileInfo::exists(path)) {
        if(!destination.m->readable) return QStringLiteral("The settings file could not be read. It has been preserved; changes will not be saved.");
        if(!QFileInfo(path).isWritable()) return QStringLiteral("The settings file is read-only. Changes cannot be saved.");
        return {};
    }
    // Import original native Qt preferences only. settings.ini is never loaded.
    QSettings legacy; legacy.setFallbacksEnabled(false);
    for(const auto &key:legacy.allKeys()) {
        if(key.startsWith("appearance/") || key.startsWith("KeyboardBindings/") || key=="History/enabled" || key=="Passwords/enabled") {
            const auto value=legacy.value(key);
            if(key=="appearance/navigationPill" || key=="History/enabled" || key=="Passwords/enabled") destination.setValue(key,value.toBool());
            else destination.setValue(key,value.toString());
        }
    }
    destination.setValue("configuration/version",1);
    return destination.status()==QSettings::NoError ? QString() : QStringLiteral("Could not create the settings file. Existing preferences have been preserved.");
}
