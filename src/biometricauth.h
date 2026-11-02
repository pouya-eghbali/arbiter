#pragma once
#include <QObject>
#include <QByteArray>
#include <QTimer>
#include <QDeadlineTimer>
#include <functional>

// Native verification authorizes a vault operation. Supported platforms can
// also store a device-bound key protected by OS-enforced biometric access.
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
    using KeyCallback = std::function<void(const QString &, const QByteArray &)>;
    virtual bool persistentKeysSupported() const { return false; }
    virtual void storeProtectedKey(const QString &, const QByteArray &, KeyCallback callback) { callback(tr("Persistent biometric unlocking is unavailable."), {}); }
    virtual void readProtectedKey(const QString &, const QString &, KeyCallback callback) { callback(tr("Persistent biometric unlocking is unavailable."), {}); }
    virtual void removeProtectedKey(const QString &, KeyCallback callback) { callback(tr("Persistent biometric unlocking is unavailable."), {}); }
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
