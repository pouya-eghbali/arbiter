#pragma once
#include "credentialsyncserver.h"
#include "credentialsyncstate.h"
#include "lockedkey.h"
#include "secretstore.h"
#include <QJsonObject>
#include <QPointer>
#include <utility>
#include <memory>
#include <QTimer>
#include <QVariantList>
class PasswordManager;
class PasskeyVault;

// Trusted application UI only. Never register this object in a WebChannel.
class CredentialSync final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool unlocked READ unlocked NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool enabled READ enabled NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(QString pairingImage READ pairingImage NOTIFY changed)
    Q_PROPERTY(QString pairingCode READ pairingCode NOTIFY changed)
    Q_PROPERTY(QString address READ address NOTIFY changed)
    Q_PROPERTY(QVariantList devices READ devices NOTIFY changed)
    Q_PROPERTY(int conflicts READ conflicts NOTIFY changed)
    Q_PROPERTY(QVariantList conflictChoices READ conflictChoices NOTIFY changed)
    Q_PROPERTY(QString pendingDevice READ pendingDevice NOTIFY changed)
    Q_PROPERTY(QString pendingPin READ pendingPin NOTIFY changed)
public:
    CredentialSync(PasswordManager *passwords,PasskeyVault *passkeys,QObject *parent=nullptr);
    CredentialSync(PasswordManager *passwords,PasskeyVault *passkeys,const QString &directory,
                   SecretStore *store,const QString &loopbackAddress,QObject *parent=nullptr);
    ~CredentialSync() override { lock(); }
    bool unlocked() const { return m_key.present() && m_passkeysReady(); }
    bool busy() const { return m_busy; }
    bool enabled() const { return m_enabled; }
    Q_INVOKABLE void setEnabled(bool enabled, const QString &legacyPassphrase = {});
    QString error() const { return m_error; }
    QString pairingImage() const { return m_qr; }
    QString pairingCode() const { return m_code; }
    QString address() const { return m_address; }
    QVariantList devices() const;
    int conflicts() const { return m_state.conflicts(); }
    QVariantList conflictChoices() const;
    QString pendingDevice() const { return m_pending.value("name").toString(); }
    QString pendingPin() const { return m_pending.value("pin").toString(); }
    Q_INVOKABLE void unlock(const QString &passphrase,bool biometric=false);
    Q_INVOKABLE void lock();
    Q_INVOKABLE void pair();
    Q_INVOKABLE void cancelPairing();
    Q_INVOKABLE void revoke(const QString &deviceId);
    Q_INVOKABLE void reviewPairing(const QString &invitation);
    Q_INVOKABLE void confirmPairing();
    Q_INVOKABLE void cancelJoin();
    Q_INVOKABLE void copyPairingCode();
    Q_INVOKABLE void sync();
    Q_INVOKABLE void resolveConflict(const QString &id,const QString &version,const QString &generation);
signals:
    void changed();
private:
    // Native keychain jobs may finish after a lock or window teardown. Never
    // let an old completion restore keys or mutate a newly unlocked session.
    template<class F> auto protect(F function) {
        const QPointer<CredentialSync> guard(this); const auto epoch=m_epoch;
        auto action=std::make_shared<std::decay_t<F>>(std::move(function));
        return [guard,epoch,action](auto &&...arguments) {
            if(guard && guard->m_epoch==epoch) (*action)(std::forward<decltype(arguments)>(arguments)...);
        };
    }
    using Done=std::function<void(const QString &)>;
    bool m_passkeysReady() const;
    void finish(const QString &error={});
    void loadIdentity(Done callback);
    bool save();
    void refresh(Done callback, bool authorizeExisting = false);
    void resume();
    bool persistEnabled(bool enabled);
    void apply(Done callback);
    void syncNext(QStringList peers,QStringList errors);
    void request(const QString &path,const QByteArray &token,const QByteArray &body,CredentialSyncServer::Reply reply);
    QByteArray payload(const QJsonObject &data) const;
    PasswordManager *m_passwords;
    PasskeyVault *m_passkeys;
    KeychainSecretStore m_defaultStore;
    SecretStore *m_store;
    QString m_loopbackAddress;
    CredentialSyncServer m_server;
    LockedKey m_key;
    QString m_directory,m_error,m_qr,m_code,m_address;
    QByteArray m_pairCode,m_certificate;
    QTimer m_pairExpiry;
    QTimer m_poll;
    QJsonObject m_document,m_current,m_pending;
    ArbiterSync::State m_state;
    bool m_busy=false,m_locking=false,m_enabled=false,m_resuming=false,m_enrolling=false;
    quint64 m_epoch=0;
};
