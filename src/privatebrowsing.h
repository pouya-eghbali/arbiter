#pragma once
#include "biometricauth.h"
#include <QElapsedTimer>
#include <QPointer>
#include <QWindow>

class PrivateBrowsing final : public QObject {
    Q_OBJECT
    Q_PROPERTY(int timeoutSeconds READ timeoutSeconds WRITE setTimeoutSeconds NOTIFY changed)
public:
    explicit PrivateBrowsing(QObject *parent=nullptr);
    int timeoutSeconds() const { return m_timeout; }
    void setTimeoutSeconds(int seconds);
    Q_INVOKABLE void open() { emit openRequested(); }
signals:
    void changed();
    void openRequested();
private:
    int m_timeout=300;
};

// One guard per private window. Only a salted password verifier lives in RAM;
// private-session passwords and approval never enter preferences or Keychain.
class PrivateSession final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool locked READ locked NOTIFY changed)
    Q_PROPERTY(bool configured READ configured NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool biometricAvailable READ biometricAvailable NOTIFY changed)
    Q_PROPERTY(QString biometricName READ biometricName NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
public:
    PrivateSession(PrivateBrowsing *settings, BiometricAuth *auth, QObject *parent=nullptr);
    ~PrivateSession() override;
    bool locked() const { return m_locked; }
    bool configured() const { return !m_verifier.isEmpty(); }
    bool busy() const { return m_busy; }
    bool biometricAvailable() const { return m_auth->available(); }
    QString biometricName() const { return m_auth->name(); }
    QString status() const { return m_status; }
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
    void verifyPassword(const QString &password,bool setup);
    PrivateBrowsing *m_settings;
    BiometricAuth *m_auth;
    QPointer<QWindow> m_window;
    QTimer m_timer;
    QElapsedTimer m_idle, m_retry;
    QByteArray m_salt,m_verifier;
    QString m_status;
    bool m_locked=true,m_busy=false;
    quint64 m_generation=0;
};
