#pragma once
#include "secretstore.h"
#include <QLockFile>
#include <QTimer>
#include <QUrl>
#include <QVariantList>
#include <memory>

class PasswordManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList entries READ entries NOTIFY entriesChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY stateChanged)
    Q_PROPERTY(QString error READ error NOTIFY stateChanged)
    Q_PROPERTY(QString message READ message NOTIFY stateChanged)
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY stateChanged)
    Q_PROPERTY(bool pending READ pending NOTIFY pendingChanged)
    Q_PROPERTY(QString pendingOrigin READ pendingOrigin NOTIFY pendingChanged)
    Q_PROPERTY(QString pendingUsername READ pendingUsername NOTIFY pendingChanged)
    Q_PROPERTY(bool pendingUpdate READ pendingUpdate NOTIFY pendingChanged)
    Q_PROPERTY(QString captureScript READ captureScript CONSTANT)
public:
    explicit PasswordManager(QObject *parent = nullptr);
    PasswordManager(const QString &directory, const QString &profile, SecretStore *store,
                    QObject *parent = nullptr);
    QVariantList entries() const { return m_entries; }
    bool busy() const { return m_busy; }
    QString error() const { return m_error; }
    QString message() const { return m_message; }
    bool enabled() const { return m_enabled; }
    void setEnabled(bool enabled);
    bool pending() const { return !m_pendingOrigin.isEmpty(); }
    QString pendingOrigin() const { return m_pendingOrigin; }
    QString pendingUsername() const { return m_pendingUsername; }
    bool pendingUpdate() const;
    QString captureScript() const;
    static QString originFor(const QUrl &url);
    static bool canStore(const QUrl &url,const QString &username,const QString &password);
    using ImportCallback=std::function<void(bool imported,const QString &error)>;
    void importAccount(const QUrl &url,const QString &username,const QString &password,ImportCallback callback);
    static QString fillScript(const QString &origin, const QString &document,
                              const QString &form, const QString &username, const QString &password);
    Q_INVOKABLE QVariantList accountsFor(const QUrl &url) const;
    Q_INVOKABLE void save(const QUrl &url, const QString &username, const QString &password);
    Q_INVOKABLE void remove(const QString &id);
    Q_INVOKABLE void acceptPending(const QString &username);
    Q_INVOKABLE void dismissPending();
    Q_INVOKABLE QString generatePassword() const;
    Q_INVOKABLE void clearMessage();
    Q_INVOKABLE void reportFillResult(bool filled);
    void offer(const QUrl &committedUrl, const QString &reportedOrigin,
               const QString &username, const QString &password, bool privateBrowsing);
    void readForFill(const QString &id, const QString &origin, SecretStore::Callback callback);
signals:
    void entriesChanged();
    void stateChanged();
    void pendingChanged();
    void credentialSaved();
private:
    void initialize();
    void saveLogin(const QUrl &url,const QString &username,const QString &password,ImportCallback callback);
    bool persist(const QVariantList &entries);
    void finish(const QString &error, const QString &message = {});
    int find(const QString &origin, const QString &username) const;
    QString secretKey(const QString &id) const;
    QString m_directory;
    QString m_profile;
    SecretStore *m_store;
    std::unique_ptr<QLockFile> m_lock;
    QVariantList m_entries;
    bool m_busy = false;
    bool m_enabled = true;
    bool m_writable = true;
    QString m_error, m_message;
    QString m_pendingOrigin, m_pendingUsername, m_pendingPassword;
    QTimer m_pendingTimer;
};

// Only this write-only object is published to the isolated WebChannel. It has
// no vault queries, password-read methods, or secret-bearing signals/properties.
class PasswordCapture : public QObject
{
    Q_OBJECT
public:
    using QObject::QObject;
    std::function<void(const QString &, const QString &, const QString &)> receiver;
    Q_INVOKABLE void submit(const QString &origin, const QString &username, const QString &password);
};

class PasswordSession : public QObject
{
    Q_OBJECT
    Q_PROPERTY(PasswordManager *manager MEMBER m_manager)
    Q_PROPERTY(QObject *capture READ capture CONSTANT)
public:
    explicit PasswordSession(QObject *parent = nullptr);
    QObject *capture() { return &m_capture; }
    Q_INVOKABLE void validateCapture(const QUrl &committedUrl, bool privateBrowsing);
    Q_INVOKABLE void prepare(const QUrl &committedUrl, const QVariantMap &document);
    Q_INVOKABLE void fill(const QString &id, const QUrl &committedUrl);
    Q_INVOKABLE void invalidate();
signals:
    void captureNeedsValidation();
    void prepared(const QString &origin);
    void runFillScript(const QString &script);
private:
    PasswordManager *m_manager = nullptr;
    PasswordCapture m_capture;
    QString m_captureOrigin, m_captureUsername, m_capturePassword;
    QString m_origin, m_document, m_form;
    quint64 m_generation = 0;
};
