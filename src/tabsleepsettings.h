#pragma once
#include <QObject>

class TabSleepSettings final : public QObject {
    Q_OBJECT
    Q_PROPERTY(int timeoutSeconds READ timeoutSeconds WRITE setTimeoutSeconds NOTIFY timeoutChanged)
public:
    explicit TabSleepSettings(QObject *parent=nullptr);
    int timeoutSeconds() const { return m_timeout; }
    void setTimeoutSeconds(int seconds);
signals:
    void timeoutChanged();
private:
    int m_timeout=900;
};
