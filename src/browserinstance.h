#pragma once
#include <QLocalServer>
#include <QLockFile>
#include <QUrl>
#include <memory>

// Serialize access to the browser profile and forward external links to its owner.
class BrowserInstance final : public QObject {
    Q_OBJECT
public:
    enum Result { Primary, Forwarded, Failed };
    explicit BrowserInstance(const QString &directory,QObject *parent=nullptr);
    ~BrowserInstance() override;
    Result start(const QStringList &arguments);
    QString error() const { return m_error; }
signals:
    void urlReceived(const QUrl &url);
    void activationRequested();
private:
    QLocalServer m_server;
    std::unique_ptr<QLockFile> m_lock;
    QString m_directory,m_name,m_error;
};
