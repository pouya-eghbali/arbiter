#pragma once
#include <QObject>
#include <QPointer>
#include <QVariantList>
#include <QJsonObject>
#include "webstoreinstall.h"
#include <QtWebEngineQuick/QQuickWebEngineProfile>
#include <QtWebEngineCore/QWebEngineExtensionManager>

// A catalog belongs to one persistent Chromium profile. Approval is local to
// that profile and bound to the exact manifest reviewed by its user.
class ExtensionCatalog final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList items READ items NOTIFY changed)
    Q_PROPERTY(bool available READ available CONSTANT)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(double progress READ progress NOTIFY changed)
    Q_PROPERTY(bool canCancel READ canCancel NOTIFY changed)
    Q_PROPERTY(QVariantMap review READ review NOTIFY changed)
public:
    explicit ExtensionCatalog(QQuickWebEngineProfile *profile);
    QVariantList items() const { return m_items; }
    bool available() const { return m_manager; }
    bool busy() const { return m_busy; }
    QString status() const { return m_status; }
    QVariantMap review() const { return m_review; }
    double progress() const { return m_store ? m_store->progress() : -1; }
    bool canCancel() const { return m_store && m_store->active(); }
    Q_INVOKABLE QString storeItemId(const QString &linkOrId) const;
    Q_INVOKABLE void installFromStore(const QString &linkOrId);
    Q_INVOKABLE void cancelInstall();
    Q_INVOKABLE void install(const QUrl &source);
    Q_INVOKABLE void requestEnable(const QString &id);
    Q_INVOKABLE void approve();
    Q_INVOKABLE void cancelReview();
    Q_INVOKABLE void disable(const QString &id);
    Q_INVOKABLE void remove(const QString &id);
    Q_INVOKABLE QUrl popupUrl(const QString &id) const;
    Q_INVOKABLE QUrl optionsUrl(const QString &id) const;
    Q_INVOKABLE void refresh();
signals:
    void changed();
private:
    QWebEngineExtensionInfo find(const QString &id) const;
    void loaded(const QWebEngineExtensionInfo &info, bool installed);
    bool saveApprovals(const QJsonObject &approvals);
    QString fingerprint(const QWebEngineExtensionInfo &info) const;
    QPointer<QQuickWebEngineProfile> m_profile;
    QWebEngineExtensionManager *m_manager = nullptr;
    QVariantList m_items;
    QVariantMap m_review;
    QJsonObject m_approvals;
    QString m_statePath, m_status, m_reviewHash;
    WebStoreInstall *m_store = nullptr;
    QString m_pendingStoreId;
    bool m_busy = false;
};
