#pragma once
#include <QObject>
#include <QDateTime>
#include <QNetworkAccessManager>
#include <QPointer>
#include <QTimer>
#include <QUrl>
#include <optional>

class QNetworkReply;
class UpdateTracker final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool automatic READ automatic WRITE setAutomatic NOTIFY changed)
    Q_PROPERTY(QString state READ state NOTIFY changed)
    Q_PROPERTY(QString currentVersion READ currentVersion CONSTANT)
    Q_PROPERTY(QString latestVersion READ latestVersion NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(QDateTime lastChecked READ lastChecked NOTIFY changed)
    Q_PROPERTY(QUrl releaseUrl READ releaseUrl NOTIFY changed)
public:
    explicit UpdateTracker(QObject *parent=nullptr);
    ~UpdateTracker() override;
    // Endpoint injection is for local HTTP fixtures; production always uses GitHub HTTPS.
    UpdateTracker(const QString &installedVersion, const QUrl &endpoint, QObject *parent=nullptr);
    bool automatic() const { return m_automatic; }
    void setAutomatic(bool enabled);
    QString state() const { return m_state; }
    QString currentVersion() const { return m_current; }
    QString latestVersion() const { return m_latest; }
    QString error() const { return m_error; }
    QDateTime lastChecked() const { return m_lastChecked; }
    QUrl releaseUrl() const;
    void start();
    void checkAutomatically();
    Q_INVOKABLE void check();
    static std::optional<int> compareVersions(const QString &left, const QString &right);
signals:
    void changed();
private:
    void fail(const QString &error);
    void updateState();
    bool m_automatic=true, m_started=false;
    QString m_current, m_latest, m_tag, m_state="idle", m_error;
    QDateTime m_lastChecked, m_lastAttempt;
    QUrl m_endpoint;
    QNetworkAccessManager m_network;
    QPointer<QNetworkReply> m_reply;
    QTimer m_schedule, m_deadline;
    QByteArray m_payload;
};
