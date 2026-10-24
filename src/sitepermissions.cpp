#include "sitepermissions.h"
#include "settings.h"
#include <QQuickWebEngineProfile>
#include <algorithm>
#include <utility>

using Type=QWebEnginePermission::PermissionType;
using State=QWebEnginePermission::State;

SitePermissionSettings::SitePermissionSettings(QObject *parent) : QObject(parent) {
    m_geolocation=Settings().value(QStringLiteral("Privacy/geolocationEnabled"),true).toBool();
}
QString SitePermissionSettings::originText(const QUrl &url) {
    if ((url.scheme()!=QStringLiteral("https") && url.scheme()!=QStringLiteral("http")) || url.host().isEmpty()) return {};
    QUrl origin;
    origin.setScheme(url.scheme()); origin.setHost(url.host()); origin.setPort(url.port());
    return origin.toString(QUrl::FullyEncoded);
}
QString SitePermissionSettings::permissionLabel(Type type) {
    switch(type) {
    case Type::Geolocation: return tr("your location");
    case Type::MediaAudioCapture: return tr("your microphone");
    case Type::MediaVideoCapture: return tr("your camera");
    case Type::MediaAudioVideoCapture: return tr("your camera and microphone");
    case Type::DesktopVideoCapture: return tr("screen sharing");
    case Type::DesktopAudioVideoCapture: return tr("screen and audio sharing");
    case Type::MouseLock: return tr("pointer lock");
    case Type::Notifications: return tr("notifications");
    case Type::ClipboardReadWrite: return tr("your clipboard");
    case Type::LocalFontsAccess: return tr("installed fonts");
    default: return {};
    }
}
void SitePermissionSettings::revokeLocation(QQuickWebEngineProfile *profile) {
    for (const auto &permission:profile->listPermissionsForPermissionType(Type::Geolocation))
        if (permission.state()==State::Granted) permission.reset();
}
void SitePermissionSettings::trackProfile(QQuickWebEngineProfile *profile) {
    if (!profile) return;
    for (const auto &existing:m_profiles) if (existing==profile) return;
    m_profiles.append(profile);
    if (!m_geolocation) revokeLocation(profile);
    connect(profile,&QObject::destroyed,this,[this] { emit permissionsChanged(); });
    emit permissionsChanged();
}
void SitePermissionSettings::setGeolocationEnabled(bool enabled) {
    if (m_geolocation==enabled) return;
    m_geolocation=enabled;
    Settings().setValue(QStringLiteral("Privacy/geolocationEnabled"),enabled);
    if (!enabled) for (const auto &profile:m_profiles) if (profile) revokeLocation(profile);
    emit geolocationEnabledChanged();
    emit permissionsChanged();
}
QVariantList SitePermissionSettings::savedPermissions() const {
    QVariantList result;
    for (const auto &profile:m_profiles) {
        if (!profile || profile->isOffTheRecord()) continue;
        for (const auto &permission:profile->listAllPermissions()) {
            if (!permission.isValid() || permission.state()==State::Ask || permission.state()==State::Invalid) continue;
            result.append(QVariantMap{{"profile",profile->storageName()},
                {"origin",originText(permission.origin())}, {"type",int(permission.permissionType())},
                {"label",permissionLabel(permission.permissionType())},
                {"allowed",permission.state()==State::Granted}});
        }
    }
    std::sort(result.begin(),result.end(),[](const QVariant &a,const QVariant &b) {
        const auto x=a.toMap(),y=b.toMap();
        return x.value("origin").toString()+x.value("label").toString()+x.value("profile").toString()
             < y.value("origin").toString()+y.value("label").toString()+y.value("profile").toString();
    });
    return result;
}
void SitePermissionSettings::resetPermission(const QString &name,const QUrl &origin,int value) {
    const auto type=Type(value);
    if (!QWebEnginePermission::isPersistent(type) || originText(origin).isEmpty()) return;
    for (const auto &profile:m_profiles)
        if (profile && !profile->isOffTheRecord() && profile->storageName()==name)
            profile->queryPermission(origin,type).reset();
    emit permissionsChanged();
}

SitePermissions::~SitePermissions() { for(const auto &r:m_queue) discard(r.permission); }
const SitePermissions::Request *SitePermissions::current() const {
    if (!m_enabled || !m_focus) return nullptr;
    for (const auto &request:m_queue)
        if (request.owner==m_focus && request.permission.isValid()
                && (!QWebEnginePermission::isPersistent(request.permission.permissionType()) || request.permission.state()==State::Ask)) return &request;
    return nullptr;
}
void SitePermissions::setFocusedView(QObject *view) { if(m_focus==view) return; m_focus=view; emit changed(); }
void SitePermissions::setEnabled(bool value) { if(m_enabled==value) return; m_enabled=value; emit changed(); }
void SitePermissions::setLocationEnabled(bool value) {
    if (m_location==value) return;
    m_location=value;
    if (!value) {
        QList<QWebEnginePermission> discardList;
        for (auto it=m_queue.begin();it!=m_queue.end();) {
            if (it->permission.permissionType()==Type::Geolocation) { discardList.append(it->permission); it=m_queue.erase(it); }
            else ++it;
        }
        for (const auto &permission:discardList) discard(permission);
    }
    emit changed();
}
int SitePermissions::requestId() const { const auto *r=current(); return r ? r->id : 0; }
QString SitePermissions::origin() const { const auto *r=current(); return r ? SitePermissionSettings::originText(r->permission.origin()) : QString(); }
QString SitePermissions::topOrigin() const { const auto *r=current(); return r ? r->top : QString(); }
QString SitePermissions::label() const { const auto *r=current(); return r ? SitePermissionSettings::permissionLabel(r->permission.permissionType()) : QString(); }
bool SitePermissions::persistent() const { const auto *r=current(); return r && QWebEnginePermission::isPersistent(r->permission.permissionType()); }
void SitePermissions::discard(const QWebEnginePermission &permission) {
    if (!permission.isValid()) return;
    if (QWebEnginePermission::isPersistent(permission.permissionType()) && permission.state()!=State::Ask) return;
    permission.deny();
    // Dismissal/navigation/global policy must not create a remembered site block.
    if (QWebEnginePermission::isPersistent(permission.permissionType())) permission.reset();
}
void SitePermissions::request(QObject *view,QWebEnginePermission permission,const QUrl &pageUrl) {
    if (!view || !permission.isValid() || SitePermissionSettings::originText(permission.origin()).isEmpty()
            || SitePermissionSettings::originText(pageUrl).isEmpty()
            || SitePermissionSettings::permissionLabel(permission.permissionType()).isEmpty()
            || (!m_location && permission.permissionType()==Type::Geolocation) || m_queue.size()>=32) {
        discard(permission); return;
    }
    for (const auto &r:m_queue)
        if (r.owner==view && r.permission==permission) return;
    if (!m_observed.contains(view)) {
        m_observed.insert(view);
        connect(view,&QObject::destroyed,this,[this,view] { m_observed.remove(view); cancelFor(view); });
    }
    m_queue.append(Request{++m_nextId,view,view,permission,SitePermissionSettings::originText(pageUrl)});
    emit changed();
}
void SitePermissions::decide(int id,bool allow) {
    if (!current() || current()->id!=id) return;
    const auto permission=current()->permission;
    for (auto it=m_queue.begin();it!=m_queue.end();++it) if(it->id==id) { m_queue.erase(it); break; }
    if (allow) permission.grant(); else permission.deny();
    emit changed(); emit resolved();
}
void SitePermissions::dismiss(int id) {
    if (!current() || current()->id!=id) return;
    const auto permission=current()->permission;
    for (auto it=m_queue.begin();it!=m_queue.end();++it) if(it->id==id) { m_queue.erase(it); break; }
    discard(permission); emit changed(); emit resolved();
}
void SitePermissions::cancelFor(QObject *view) {
    QList<QWebEnginePermission> discarded;
    for (auto it=m_queue.begin();it!=m_queue.end();) {
        if (it->identity==view) { discarded.append(it->permission); it=m_queue.erase(it); }
        else ++it;
    }
    for(const auto &p:discarded) discard(p);
    emit changed();
}
void SitePermissions::clear() {
    const auto old=std::exchange(m_queue,{});
    for(const auto &r:old) discard(r.permission);
    emit changed();
}
