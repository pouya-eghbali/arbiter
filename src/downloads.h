#pragma once
#include <QObject>
#include <QPointer>
#include <QTimer>
#include <QUrl>
#include <QVariantList>
#include <QtWebEngineCore/QWebEngineDownloadRequest>
#include <QtWebEngineQuick/QQuickWebEngineProfile>

class DownloadSettings final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QUrl directory READ directory WRITE setDirectory NOTIFY changed)
    Q_PROPERTY(QString directoryPath READ path NOTIFY changed)
public:
    explicit DownloadSettings(QObject *parent = nullptr);
    QUrl directory() const { return QUrl::fromLocalFile(m_directory); }
    QString path() const { return m_directory; }
    void setDirectory(const QUrl &directory);
signals:
    void changed();
private:
    QString m_directory;
};

class DownloadEntry final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString id MEMBER id CONSTANT)
    Q_PROPERTY(QString fileName MEMBER fileName NOTIFY changed)
    Q_PROPERTY(QString path MEMBER path NOTIFY changed)
    Q_PROPERTY(QString source READ source NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(QString error MEMBER error NOTIFY changed)
    Q_PROPERTY(QString warning MEMBER warning NOTIFY changed)
    Q_PROPERTY(qint64 received MEMBER received NOTIFY changed)
    Q_PROPERTY(qint64 total MEMBER total NOTIFY changed)
    Q_PROPERTY(bool active READ active NOTIFY changed)
    Q_PROPERTY(bool paused MEMBER paused NOTIFY changed)
    Q_PROPERTY(bool completed READ completed NOTIFY changed)
    Q_PROPERTY(bool canRetry READ canRetry NOTIFY changed)
public:
    explicit DownloadEntry(QObject *parent) : QObject(parent) {}
    QString id, fileName, path, error, warning;
    QUrl url;
    QString started;
    qint64 received = 0, total = -1;
    QWebEngineDownloadRequest::DownloadState state = QWebEngineDownloadRequest::DownloadRequested;
    bool paused = false, reserved = false;
    QPointer<QWebEngineDownloadRequest> request;
    bool active() const { return state == QWebEngineDownloadRequest::DownloadInProgress || state == QWebEngineDownloadRequest::DownloadRequested; }
    bool completed() const { return state == QWebEngineDownloadRequest::DownloadCompleted; }
    bool canRetry() const { return state == QWebEngineDownloadRequest::DownloadInterrupted && request && !request->isFinished(); }
    QString source() const;
    QString status() const;
signals:
    void changed();
};

class Downloads final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList items READ items NOTIFY itemsChanged)
    Q_PROPERTY(int activeCount READ activeCount NOTIFY changed)
    Q_PROPERTY(DownloadSettings *preferences READ preferences CONSTANT)
    Q_PROPERTY(bool privateMode READ privateMode CONSTANT)
    Q_PROPERTY(QString error READ error NOTIFY changed)
public:
    explicit Downloads(DownloadSettings *preferences, QObject *parent = nullptr);
    Downloads(DownloadSettings *preferences, const QString &historyPath, QObject *parent = nullptr);
    ~Downloads() override;
    QVariantList items() const;
    int activeCount() const;
    DownloadSettings *preferences() const { return m_preferences; }
    bool privateMode() const { return m_historyPath.isEmpty(); }
    QString error() const { return m_error; }
    void trackProfile(QQuickWebEngineProfile *profile);
    Q_INVOKABLE void setBlocked(bool value) { m_blocked = value; }
    Q_INVOKABLE void cancel(const QString &id);
    Q_INVOKABLE void pause(const QString &id);
    Q_INVOKABLE void resume(const QString &id);
    Q_INVOKABLE void retry(const QString &id);
    Q_INVOKABLE void open(const QString &id);
    Q_INVOKABLE void showFolder(const QString &id);
    Q_INVOKABLE void clearFinished();
    static QString safeFileName(QString name);
    static QString reservePath(const QString &directory, const QString &name);
signals:
    void changed();
    void itemsChanged();
    void started();
private:
    DownloadEntry *find(const QString &id) const;
    void receive(QWebEngineDownloadRequest *request);
    void update(DownloadEntry *entry);
    void save();
    DownloadSettings *m_preferences;
    QString m_historyPath, m_error;
    QList<DownloadEntry *> m_items;
    QTimer m_saveTimer;
    bool m_blocked = false;
};
