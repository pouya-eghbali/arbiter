#pragma once
#include <QObject>
#include <QPointer>
#include <QWindow>
#include <QUrl>
#include <QPointF>
#include <QtQml/qqmlregistration.h>

class NativeShare : public QObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QWindow *host READ host WRITE setHost NOTIFY hostChanged)
    Q_PROPERTY(bool active READ active NOTIFY activeChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
public:
    explicit NativeShare(QObject *parent=nullptr) : QObject(parent) {}
    ~NativeShare() override;
    QWindow *host() const { return m_host; }
    void setHost(QWindow *window);
    bool active() const { return m_active; }
    QString error() const { return m_error; }
    Q_INVOKABLE bool show(QObject *owner,const QUrl &url,const QString &title,const QPointF &anchor);
    Q_INVOKABLE void cancel();
    Q_INVOKABLE void cancelFor(QObject *owner);
    bool canShare() const;
    quint64 generation() const { return m_generation; }
    void copyLink(quint64 generation);
    void finished(void *token,quint64 generation);
    QUrl link() const { return m_link; }
    QString title() const { return m_title; }
signals:
    void hostChanged();
    void activeChanged();
    void errorChanged();
private:
    QPointer<QWindow> m_host;
    QPointer<QObject> m_owner;
    QUrl m_link;
    QString m_title,m_error;
    bool m_active=false;
    void *m_state=nullptr;
    quint64 m_generation=0;
};
void *createNativeShare(NativeShare *controller,QString *error);
bool presentNativeShare(void *state,const QPointF &anchor,QString *error);
void destroyNativeShare(void *state);
