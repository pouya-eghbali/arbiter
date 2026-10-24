#pragma once
#include <QObject>
#include <QMutex>
#include <QNetworkAccessManager>
#include <QWebEngineUrlRequestInterceptor>
#include <QtQml/qqmlregistration.h>
#include <memory>
#include <atomic>

class AdBlockEngine {
public:
    explicit AdBlockEngine(const QByteArray &rules);
    ~AdBlockEngine();
    bool valid() const { return m_engine; }
    bool blocks(const QUrl &url,const QUrl &source,const QByteArray &type,const QByteArray &method) const;
    QStringList cosmetic(const QUrl &url,const QStringList &classes,const QStringList &ids) const;
private:
    void *m_engine=nullptr;
    mutable QMutex m_mutex;
};
class AdBlocker final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)
    Q_PROPERTY(bool ready READ ready NOTIFY listsChanged)
    Q_PROPERTY(bool updating READ updating NOTIFY listsChanged)
    Q_PROPERTY(QString status READ status NOTIFY listsChanged)
    Q_PROPERTY(QString cosmeticScript READ cosmeticScript CONSTANT)
public:
    explicit AdBlocker(QObject *parent=nullptr);
    AdBlocker(const QByteArray &rules,QObject *parent);
    bool enabled() const { return m_enabled.load(); }
    void setEnabled(bool value);
    bool ready() const;
    bool updating() const { return m_updating; }
    QString status() const { return m_status; }
    QString cosmeticScript() const;
    Q_INVOKABLE void updateLists();
    std::shared_ptr<AdBlockEngine> engine() const;
    static QByteArray bundledRules();
signals:
    void enabledChanged();
    void listsChanged();
private:
    mutable QMutex m_mutex;
    std::shared_ptr<AdBlockEngine> m_engine;
    std::atomic_bool m_enabled{true};
    QNetworkAccessManager m_network;
    bool m_updating=false;
    QString m_status;
    QString cacheFile() const;
};
class ViewAdBlock : public QWebEngineUrlRequestInterceptor {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QObject *manager READ manager WRITE setManager NOTIFY managerChanged)
    Q_PROPERTY(bool paused READ paused WRITE setPaused NOTIFY stateChanged)
    Q_PROPERTY(bool enabled READ enabled NOTIFY stateChanged)
    Q_PROPERTY(int blocked READ blocked NOTIFY blockedChanged)
public:
    explicit ViewAdBlock(QObject *parent=nullptr) : QWebEngineUrlRequestInterceptor(parent) {}
    QObject *manager() const { return m_manager; }
    void setManager(QObject *manager);
    bool paused() const { return m_paused.load(); }
    void setPaused(bool paused);
    bool enabled() const;
    int blocked() const { return m_blocked.load(); }
    void interceptRequest(QWebEngineUrlRequestInfo &info) override;
    Q_INVOKABLE void resetCount();
    Q_INVOKABLE QStringList cosmetic(const QUrl &url,const QStringList &classes,const QStringList &ids) const;
signals:
    void managerChanged();
    void stateChanged();
    void blockedChanged();
private:
    AdBlocker *m_manager=nullptr;
    std::atomic_bool m_paused{false};
    std::atomic_int m_blocked{0};
    std::atomic_bool m_notificationPending{false};
};
