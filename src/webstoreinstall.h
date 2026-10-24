#pragma once
#include <QObject>
#include <QNetworkAccessManager>
#include <QPointer>
#include <QSharedPointer>
#include <QTemporaryDir>
#include <QTimer>

// Downloads without profile cookies and prepares a verified extension on a
// worker thread. The directory lives until Qt finishes copying the install.
class WebStoreInstall final : public QObject {
    Q_OBJECT
public:
    explicit WebStoreInstall(QObject *parent = nullptr);
    ~WebStoreInstall() override;
    void start(const QString &id);
    void cancel();
    void release();
    bool active() const { return m_active; }
    double progress() const { return m_progress; }
    QString status() const { return m_status; }
signals:
    void changed();
    void failed(const QString &error);
    void ready(const QString &directory, const QString &id);
private:
    void request(const QUrl &url);
    void fail(const QString &error);
    void prepare(QByteArray bytes);
    QNetworkAccessManager m_network;
    QPointer<QNetworkReply> m_reply;
    QTimer m_deadline;
    QByteArray m_bytes;
    QSharedPointer<QTemporaryDir> m_directory;
    QString m_id, m_status;
    double m_progress = -1;
    quint64 m_generation = 0;
    int m_redirects = 0;
    bool m_active = false;
};
