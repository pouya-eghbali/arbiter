#pragma once
#include <QObject>
#include <QTimer>
#include <QDeadlineTimer>
#include <functional>

// Authentication authorizes use of a short-lived in-process vault key. No
// platform backend receives a passphrase or persists vault key material.
class BiometricAuth : public QObject {
    Q_OBJECT
public:
    using Callback = std::function<void(bool, const QString &)>;
    explicit BiometricAuth(QObject *parent = nullptr);
    bool available() const { return m_available; }
    bool busy() const { return bool(m_callback); }
    QString status() const { return m_status; }
    virtual QString name() const = 0;
    virtual bool foregroundRequired() const { return true; }
    virtual bool nativePromptCanTakeFocus() const { return false; }
    virtual void refresh() = 0;
    void authenticate(const QString &reason, Callback callback);
    void cancel();
signals:
    void changed();
    void sessionLocked();
protected:
    virtual void startNative(const QString &reason, quint64 token) = 0;
    virtual void cancelNative() = 0;
    void complete(quint64 token, bool success, const QString &error = {});
    void availability(bool available, const QString &status);
    void message(const QString &status);
    bool current(quint64 token) const { return token == m_token && busy(); }
private:
    bool m_available = false;
    QString m_status = tr("Checking biometric support…");
    QString m_availableStatus = m_status;
    Callback m_callback;
    quint64 m_token = 0;
    QTimer m_timeout;
    QTimer m_focusWait;
    QDeadlineTimer m_focusDeadline;
    quint64 m_focusToken = 0;
};
BiometricAuth *createBiometricAuth(QObject *parent);
