#pragma once
#include <QObject>
#include <QHash>
#include <functional>
#include "extensioncatalog.h"
#include "siteuseragentinterceptor.h"
#include <QtWebEngineQuick/QQuickWebEngineProfile>

// One live Chromium profile per storage name, owned beyond the QML views.
class WebProfiles final : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    QString userAgent;
    bool privateMode = false;
    std::function<void(QQuickWebEngineProfile *)> profileCreated;
    Q_INVOKABLE QQuickWebEngineProfile *profile(const QString &name) {
        if (m_profiles.contains(name)) return m_profiles.value(name);
        auto *profile = privateMode ? new QQuickWebEngineProfile(this) : new QQuickWebEngineProfile(name, this);
        profile->setHttpUserAgent(userAgent);
        profile->setUrlRequestInterceptor(new SiteUserAgentInterceptor(profile));
        profile->setPersistentPermissionsPolicy(privateMode ? QQuickWebEngineProfile::PersistentPermissionsPolicy::StoreInMemory
                                                          : QQuickWebEngineProfile::PersistentPermissionsPolicy::StoreOnDisk);
        m_profiles.insert(name, profile);
        m_extensions.insert(profile, new ExtensionCatalog(profile));
        if (profileCreated) profileCreated(profile);
        return profile;
    }
    Q_INVOKABLE QObject *extensions(QQuickWebEngineProfile *profile) const { return m_extensions.value(profile); }
private:
    QHash<QQuickWebEngineProfile *, ExtensionCatalog *> m_extensions;
    QHash<QString,QQuickWebEngineProfile *> m_profiles;
};
