#pragma once
#include "settings.h"
#include <QObject>
class Appearance;

// Small persisted setup state. Import and platform prompts stay in their own controllers.
class Onboarding final : public QObject {
    Q_OBJECT
    Q_PROPERTY(int step READ step NOTIFY changed)
    Q_PROPERTY(QString preset READ preset NOTIFY changed)
    Q_PROPERTY(bool completed READ completed NOTIFY changed)
    Q_PROPERTY(bool tutorialPending READ tutorialPending NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
public:
    explicit Onboarding(Appearance *appearance, QObject *parent=nullptr);
    int step() const { return m_step; }
    QString preset() const;
    bool completed() const { return m_completed; }
    bool tutorialPending() const;
    QString error() const { return m_error; }
    Q_INVOKABLE void next();
    Q_INVOKABLE void back();
    Q_INVOKABLE void choosePreset(const QString &preset);
    Q_INVOKABLE void finish(bool tutorial);
    Q_INVOKABLE void dismissTutorial();
    Q_INVOKABLE void replayTutorial();
    Q_INVOKABLE void abort() { emit aborted(); }
signals:
    void changed();
    void finished();
    void aborted();
private:
    bool save(const QString &key, const QVariant &value);
    Settings m_settings;
    Appearance *m_appearance;
    int m_step=0;
    bool m_completed=false,m_tutorialPending=false;
    QString m_error;
};
