#pragma once
#include "biometricauth.h"
#include "secretstore.h"
#include <QElapsedTimer>
#include <QPointer>
#include <QWindow>

class PrivateBrowsing final : public QObject {
    Q_OBJECT
    Q_PROPERTY(int timeoutSeconds READ timeoutSeconds WRITE setTimeoutSeconds NOTIFY changed)
    Q_PROPERTY(bool ready READ ready NOTIFY changed)
    Q_PROPERTY(bool configured READ configured NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
public:
    using Callback = std::function<void(bool,const QString &)>;
    explicit PrivateBrowsing(QObject *parent=nullptr);
    PrivateBrowsing(SecretStore *store,QObject *parent=nullptr);
    ~PrivateBrowsing() override;
    bool ready() const { return m_ready; }
    bool configured() const { return !m_verifier.isEmpty(); }
    bool busy() const { return m_busy; }
    QString status() const { return m_status; }
    Q_INVOKABLE void load();
    Q_INVOKABLE void setPassword(const QString &current,const QString &password,const QString &confirmation);
    void setup(const QString &password,const QString &confirmation,Callback callback);
    void verify(const QString &password,Callback callback);
    int timeoutSeconds() const { return m_timeout; }
    void setTimeoutSeconds(int seconds);
    Q_INVOKABLE void open() { load(); emit openRequested(); }
signals:
    void changed();
    void openRequested();
    void credentialChanged();
private:
    void savePassword(const QString &current,const QString &password,const QString &confirmation,Callback callback);
    bool retryAllowed();
    SecretStore *m_store;
    QByteArray m_salt,m_verifier;
    QElapsedTimer m_retry;
    QString m_status;
    bool m_ready=false,m_busy=false;
    int m_timeout=300;
};

// Per-window approval and locking, with one shared OS-store-backed credential.
class PrivateSession final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool locked READ locked NOTIFY changed)
    Q_PROPERTY(bool configured READ configured NOTIFY changed)
    Q_PROPERTY(bool credentialsReady READ credentialsReady NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool biometricAvailable READ biometricAvailable NOTIFY changed)
    Q_PROPERTY(QString biometricName READ biometricName NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
public:
    PrivateSession(PrivateBrowsing *settings, BiometricAuth *auth, QObject *parent=nullptr);
    ~PrivateSession() override;
    bool locked() const { return m_locked; }
    bool configured() const { return m_settings->configured(); }
    bool credentialsReady() const { return m_settings->ready(); }
    bool busy() const { return m_busy || m_settings->busy(); }
    bool biometricAvailable() const { return m_auth->available(); }
    QString biometricName() const { return m_auth->name(); }
    QString status() const { return m_status.isEmpty() ? m_settings->status() : m_status; }
    Q_INVOKABLE void retryLoading() { m_settings->load(); }
    void setWindow(QWindow *window);
    Q_INVOKABLE void lock();
    Q_INVOKABLE void setup(const QString &password, const QString &confirmation);
    Q_INVOKABLE void unlockPassword(const QString &password);
    Q_INVOKABLE void unlockBiometric();
    void sessionLocked();
signals:
    void changed();
protected:
    bool eventFilter(QObject *object,QEvent *event) override;
private:
    void checkIdle();
    void finishUnlock(quint64 generation);
    PrivateBrowsing *m_settings;
    BiometricAuth *m_auth;
    QPointer<QWindow> m_window;
    QTimer m_timer;
    QElapsedTimer m_idle;
    QString m_status;
    bool m_locked=true,m_busy=false;
    quint64 m_generation=0;
};
