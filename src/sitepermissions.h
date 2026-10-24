#pragma once
#include <QObject>
#include <QPointer>
#include <QSet>
#include <QVariantList>
#include <QWebEnginePermission>
#include <QtQml/qqmlregistration.h>

class QQuickWebEngineProfile;

class SitePermissionSettings final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool geolocationEnabled READ geolocationEnabled WRITE setGeolocationEnabled NOTIFY geolocationEnabledChanged)
    Q_PROPERTY(QVariantList savedPermissions READ savedPermissions NOTIFY permissionsChanged)
public:
    explicit SitePermissionSettings(QObject *parent=nullptr);
    bool geolocationEnabled() const { return m_geolocation; }
    void setGeolocationEnabled(bool enabled);
    void trackProfile(QQuickWebEngineProfile *profile);
    QVariantList savedPermissions() const;
    Q_INVOKABLE void refresh() { emit permissionsChanged(); }
    Q_INVOKABLE void resetPermission(const QString &profile, const QUrl &origin, int type);
    static QString originText(const QUrl &url);
    Q_INVOKABLE QString displayOrigin(const QUrl &url) const { return originText(url); }
    static QString permissionLabel(QWebEnginePermission::PermissionType type);
signals:
    void geolocationEnabledChanged();
    void permissionsChanged();
private:
    void revokeLocation(QQuickWebEngineProfile *profile);
    bool m_geolocation=true;
    QList<QPointer<QQuickWebEngineProfile>> m_profiles;
};

// Each window owns its queue. Background tabs wait until focused; navigation,
// destruction, private locking, and shutdown discard their pending requests.
class SitePermissions : public QObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QObject *focusedView READ focusedView WRITE setFocusedView NOTIFY changed)
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY changed)
    Q_PROPERTY(bool locationEnabled READ locationEnabled WRITE setLocationEnabled NOTIFY changed)
    Q_PROPERTY(bool active READ active NOTIFY changed)
    Q_PROPERTY(int requestId READ requestId NOTIFY changed)
    Q_PROPERTY(QString origin READ origin NOTIFY changed)
    Q_PROPERTY(QString topOrigin READ topOrigin NOTIFY changed)
    Q_PROPERTY(QString label READ label NOTIFY changed)
    Q_PROPERTY(bool persistent READ persistent NOTIFY changed)
public:
    using QObject::QObject;
    ~SitePermissions() override;
    QObject *focusedView() const { return m_focus; }
    void setFocusedView(QObject *view);
    bool enabled() const { return m_enabled; }
    void setEnabled(bool enabled);
    bool locationEnabled() const { return m_location; }
    void setLocationEnabled(bool enabled);
    bool active() const { return current()!=nullptr; }
    int requestId() const;
    QString origin() const;
    QString topOrigin() const;
    QString label() const;
    bool persistent() const;
    Q_INVOKABLE void request(QObject *view, QWebEnginePermission permission, const QUrl &pageUrl);
    Q_INVOKABLE void decide(int requestId, bool allow);
    Q_INVOKABLE void dismiss(int requestId);
    Q_INVOKABLE void cancelFor(QObject *view);
    Q_INVOKABLE void clear();
    Q_INVOKABLE void reject(QWebEnginePermission permission) { discard(permission); }
signals:
    void changed();
    void resolved();
private:
    struct Request {
        int id;
        QPointer<QObject> owner;
        QObject *identity;
        QWebEnginePermission permission;
        QString top;
    };
    const Request *current() const;
    static void discard(const QWebEnginePermission &permission);
    QList<Request> m_queue;
    QSet<QObject *> m_observed;
    QPointer<QObject> m_focus;
    int m_nextId=0;
    bool m_enabled=true, m_location=true;
};
