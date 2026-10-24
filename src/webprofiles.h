#pragma once
#include <QObject>
#include <QHash>
#include <QtWebEngineQuick/QQuickWebEngineProfile>

// One live Chromium profile per storage name, owned beyond the QML views.
class WebProfiles final : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    QString userAgent;
    bool privateMode = false;
    Q_INVOKABLE QQuickWebEngineProfile *profile(const QString &name) {
        if (m_profiles.contains(name)) return m_profiles.value(name);
        auto *profile = privateMode ? new QQuickWebEngineProfile(this) : new QQuickWebEngineProfile(name, this);
        profile->setHttpUserAgent(userAgent);
        m_profiles.insert(name, profile);
        return profile;
    }
private:
    QHash<QString,QQuickWebEngineProfile *> m_profiles;
};
