#pragma once
#include <QObject>
#include <QPointer>
#include <QWebEngineWebAuthUxRequest>

// UI for the authenticator requests supported by the current Qt engine. This
// does not claim that Qt has a local passkey provider or hybrid transport.
class WebAuthnController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool active READ active NOTIFY changed)
    Q_PROPERTY(int state READ state NOTIFY changed)
    Q_PROPERTY(QString relyingParty READ relyingParty NOTIFY changed)
    Q_PROPERTY(QStringList accounts READ accounts NOTIFY changed)
    Q_PROPERTY(QString description READ description NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(bool confirmPin READ confirmPin NOTIFY changed)
    Q_PROPERTY(bool canRetry READ canRetry NOTIFY changed)
public:
    using QObject::QObject;
    ~WebAuthnController() override;
    bool active() const { return !m_request.isNull(); }
    int state() const;
    QString relyingParty() const;
    QStringList accounts() const;
    QString description() const;
    QString error() const;
    bool confirmPin() const;
    bool canRetry() const;
    static QString validatePin(const QString &pin, const QString &confirmation,
                               int minimumLength, bool confirmationRequired);
    Q_INVOKABLE void handle(QWebEngineWebAuthUxRequest *request);
    Q_INVOKABLE void cancel();
    Q_INVOKABLE void retry();
    Q_INVOKABLE void selectAccount(const QString &account);
    Q_INVOKABLE void submitPin(const QString &pin, const QString &confirmation);
signals:
    void changed();
    void pinSubmitted();
private:
    void update();
    QPointer<QWebEngineWebAuthUxRequest> m_request;
    QString m_error;
};
