#pragma once
#include <QObject>
#include <QUrl>
#include <QNetworkAccessManager>
#include <QSet>

class BrowserPreferences final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString searchEngine READ searchEngine WRITE setSearchEngine NOTIFY searchEngineChanged)
public:
    explicit BrowserPreferences(QObject *parent=nullptr);
    QString searchEngine() const { return m_engine; }
    void setSearchEngine(const QString &engine);
    static QUrl normalizedShareUrl(const QUrl &url);
    static bool copySharedLink(const QUrl &url);
    Q_INVOKABLE QUrl shareableUrl(const QUrl &url) const { return normalizedShareUrl(url); }
    Q_INVOKABLE bool copyLink(const QUrl &url) const { return copySharedLink(url); }
    Q_INVOKABLE QUrl searchUrl(const QString &query) const;
    Q_INVOKABLE QString siteOrigin(const QUrl &url) const;
    Q_INVOKABLE QUrl favicon(const QUrl &url, bool allowNetwork=true);
signals:
    void searchEngineChanged();
    void faviconReady(const QString &origin);
private:
    QString m_engine;
    QNetworkAccessManager m_network;
    QSet<QString> m_pending, m_failed;
};
