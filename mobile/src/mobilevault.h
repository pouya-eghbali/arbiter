#pragma once
#include "credentialsyncstate.h"
#include <QElapsedTimer>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QObject>
#include <QTimer>
#include <QVariantList>
#include <functional>
class MobileVault : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList entries READ entries NOTIFY changed)
    Q_PROPERTY(bool unlocked READ unlocked NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(QString peerName READ peerName NOTIFY changed)
    Q_PROPERTY(int conflicts READ conflicts NOTIFY changed)
    Q_PROPERTY(QVariantList conflictChoices READ conflictChoices NOTIFY changed)
  public:
    explicit MobileVault(QObject *parent = nullptr);
    QVariantList entries() const;
    QVariantList conflictChoices() const;
    Q_INVOKABLE void resolveConflict(const QString &id, const QString &version);
    bool unlocked() const {
        return m_verified.isValid();
    }
    bool busy() const {
        return m_busy;
    }
    QString error() const {
        return m_error;
    }
    QString peerName() const {
        return m_document.value("peer").toObject().value("name").toString();
    }
    int conflicts() const {
        return m_state.conflicts();
    }
    Q_INVOKABLE void unlock();
    Q_INVOKABLE void lock();
    Q_INVOKABLE void remove(const QString &id);
    Q_INVOKABLE void pair(const QString &invitation);
    Q_INVOKABLE void confirmPair();
    Q_INVOKABLE void sync();
    Q_INVOKABLE void disconnectPeer();
    void readPassword(const QString &id, const QString &origin,
                      std::function<void(QString, QString)> callback);
    void savePassword(const QString &origin, const QString &username, const QString &password);
    void setCooldown(int seconds) {
        m_cooldown = seconds;
    }
    static bool localEndpoint(const QUrl &);
    static QJsonObject pairingInfo(const QString &invitation);
    static QString generatePassword(int length = 20, bool symbols = true);
    // Native background scheduling invokes this without exposing vault data to QML.
    void backgroundSync(std::function<void(bool)> done);
    void cancel();
  signals:
    void changed();
    void pairingReview(QString desktop, QString pin);
    void completed(bool success);

  private:
    bool load(bool create);
    bool persist(bool changePeer = false);
    void authenticated(std::function<void()> action);
    bool transaction(const std::function<bool(QJsonObject &, ArbiterSync::State &)> &edit, bool create);
    void exchange(const QString &path, const QJsonObject &peer, const QByteArray &token,
                  const QJsonObject &payload, std::function<void(QJsonObject)> callback);
    void fail(const QString &error);
    QJsonObject m_document, m_pairing;
    ArbiterSync::State m_state;
    QElapsedTimer m_verified;
    QTimer m_expiry;
    bool m_busy = false;
    QString m_error;
    int m_cooldown = 300;
    quint64 m_epoch = 0;
};
