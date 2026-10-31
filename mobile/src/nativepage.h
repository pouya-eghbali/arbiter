#pragma once
#include <QObject>
#include <QPointer>
#include <QRectF>
#include <QUrl>
#include <QVariantMap>
#include <QWindow>
#include <QtQml/qqmlregistration.h>
class NativePage : public QObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QWindow *nativeWindow READ nativeWindow NOTIFY nativeWindowChanged)
    Q_PROPERTY(QString tabId MEMBER tabId)
    Q_PROPERTY(QString profile MEMBER profile)
    Q_PROPERTY(bool privateMode MEMBER privateMode)
    Q_PROPERTY(QUrl url READ url WRITE setUrl NOTIFY changed)
    Q_PROPERTY(QRectF overlayRect READ overlayRect WRITE setOverlayRect NOTIFY overlayRectChanged)
    Q_PROPERTY(QString title READ title NOTIFY changed)
    Q_PROPERTY(bool canBack MEMBER canBack NOTIFY changed)
    Q_PROPERTY(bool canForward MEMBER canForward NOTIFY changed)
    Q_PROPERTY(bool loading MEMBER loading NOTIFY changed)
    Q_PROPERTY(int progress MEMBER progress NOTIFY changed)
  public:
    using QObject::QObject;
    ~NativePage();
    QWindow *nativeWindow() const {
        return m_window;
    }
    QUrl url() const {
        return m_url;
    }
    QString title() const {
        return m_title;
    }
    void setUrl(const QUrl &);
    QRectF overlayRect() const {
        return m_overlayRect;
    }
    void setOverlayRect(const QRectF &rect);
    Q_INVOKABLE void capture();
    Q_INVOKABLE void find(const QString &text, bool backwards = false);
    Q_INVOKABLE void start();
    Q_INVOKABLE void action(const QString &name);
    Q_INVOKABLE void configure();
    void event(const QString &type, const QVariantMap &data = {});
    void *handle = nullptr;
    QString tabId, profile;
    bool privateMode = false, canBack = false, canForward = false, loading = false;
    int progress = 0;
    void evaluate(const QString &, std::function<void(QVariant)> = {});
    static QHash<QString, QPointer<NativePage>> pages;
  signals:
    void changed();
    void nativeWindowChanged();
    void overlayRectChanged();
    void browserEvent(QString tabId, QString type, QVariantMap data);

  private:
    QWindow *m_window = nullptr;
    QUrl m_url;
    QRectF m_overlayRect;
    QString m_title;
};
