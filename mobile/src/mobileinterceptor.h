#pragma once
#include "mobilemodel.h"
#include <QWebEngineUrlRequestInterceptor>
#include <QtQml/qqmlregistration.h>
class MobileRequestFilter : public QWebEngineUrlRequestInterceptor {
    Q_OBJECT
    QML_ELEMENT
  public:
    using QWebEngineUrlRequestInterceptor::QWebEngineUrlRequestInterceptor;
    void interceptRequest(QWebEngineUrlRequestInfo &info) override {
        if (info.resourceType() == QWebEngineUrlRequestInfo::ResourceTypeMainFrame)
            return;
        const auto type = info.resourceType();
        QByteArray kind = type == QWebEngineUrlRequestInfo::ResourceTypeScript       ? "script"
                          : type == QWebEngineUrlRequestInfo::ResourceTypeImage      ? "image"
                          : type == QWebEngineUrlRequestInfo::ResourceTypeStylesheet ? "stylesheet"
                          : type == QWebEngineUrlRequestInfo::ResourceTypeSubFrame   ? "sub_frame"
                          : type == QWebEngineUrlRequestInfo::ResourceTypeMedia      ? "media"
                          : type == QWebEngineUrlRequestInfo::ResourceTypeXhr        ? "xmlhttprequest"
                                                                                     : "other";
        info.block(
            MobileModel::filterRequest(info.requestUrl(), info.firstPartyUrl(), kind, info.requestMethod()));
    }
};
#include <QtWebEngineQuick/QQuickWebEngineProfile>
class MobileEngineProfiles : public QObject {
    Q_OBJECT
    QML_ELEMENT
  public:
    using QObject::QObject;
    Q_INVOKABLE QQuickWebEngineProfile *profile(const QVariantMap &record) {
        bool isolated = false;
        for (const auto &w : MobileModel::instance->workspaces())
            if (w.toMap().value("id") == record.value("workspace"))
                isolated = w.toMap().value("isolated").toBool();
        const bool privateMode = record.value("private").toBool();
        const auto key =
            (privateMode ? "private:" : "") + (isolated ? record.value("workspace").toString() : "shared");
        if (!m_profiles.contains(key)) {
            auto profile = privateMode ? new QQuickWebEngineProfile(this)
                                       : new QQuickWebEngineProfile("mobile-" + key, this);
            profile->setUrlRequestInterceptor(new MobileRequestFilter(profile));
            m_profiles[key] = profile;
        }
        return m_profiles.value(key);
    }

  private:
    QHash<QString, QQuickWebEngineProfile *> m_profiles;
};
