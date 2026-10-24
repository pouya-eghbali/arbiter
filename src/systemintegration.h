#pragma once
#include <QObject>
#include <functional>
class SystemIntegration final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool reducedMotion READ reducedMotion CONSTANT)
    Q_PROPERTY(bool isDefault READ isDefault NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QString message READ message NOTIFY changed)
public:
    explicit SystemIntegration(QObject *parent=nullptr);
    bool reducedMotion() const;
    bool isDefault() const { return m_default; }
    bool busy() const { return m_busy; }
    QString message() const { return m_message; }
    Q_INVOKABLE void requestDefault();
    Q_INVOKABLE void refreshDefault();
signals:
    void changed();
private:
    void finishRequest(const QString &error);
    bool m_default=false,m_busy=false,m_refreshingDefault=false,m_refreshAgain=false;
    QString m_message;
};
#ifdef Q_OS_MACOS
bool macReducedMotion();
bool macIsDefaultBrowser();
void macRequestDefaultBrowser(std::function<void(QString)> completion);
#endif
