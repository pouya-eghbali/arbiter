#pragma once
#include <QQuickItem>
#include <QQuickWindow>
#include <QPointer>
#include <QTimer>
#include <QtQml/qqmlregistration.h>

class NativeGlassHost : public QQuickItem {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(bool requested READ requested WRITE setRequested NOTIFY requestedChanged)
    Q_PROPERTY(bool available READ available CONSTANT)
    Q_PROPERTY(bool reducedTransparency READ reducedTransparency NOTIFY accessibilityChanged)
    Q_PROPERTY(bool active READ active NOTIFY activeChanged)
    Q_PROPERTY(QWindow *containerWindow READ containerWindow NOTIFY containerWindowChanged)
    Q_PROPERTY(QColor foregroundColor READ foregroundColor NOTIFY foregroundColorChanged)
    Q_PROPERTY(qreal radius READ radius WRITE setRadius NOTIFY radiusChanged)
    Q_PROPERTY(QQuickItem *content READ content WRITE setContent NOTIFY contentChanged)
public:
    explicit NativeGlassHost(QQuickItem *parent = nullptr);
    ~NativeGlassHost() override;
    bool requested() const { return m_requested; }
    bool available() const;
    bool reducedTransparency() const { return m_reduced; }
    bool active() const { return m_active; }
    QWindow *containerWindow() const { return m_container; }
    QColor foregroundColor() const { return m_color; }
    qreal radius() const { return m_radius; }
    QQuickItem *content() const { return m_content; }
    void setRequested(bool);
    void setRadius(qreal);
    void setContent(QQuickItem *);
    Q_INVOKABLE void focusControl(QQuickItem *);
    Q_INVOKABLE bool geometryAligned() const;
signals:
    void requestedChanged();
    void accessibilityChanged();
    void activeChanged();
    void containerWindowChanged();
    void foregroundColorChanged();
    void radiusChanged();
    void contentChanged();
protected:
    void itemChange(ItemChange, const ItemChangeData &) override;
private:
    void schedule();
    void synchronize();
    void detach();
    void release();
    void watchGeometry();
    bool m_requested = false, m_active = false, m_queued = false, m_reduced = false;
    qreal m_radius = 28;
    QColor m_color = Qt::black;
    QPointer<QQuickItem> m_content, m_originalParent;
    QPointer<QQuickWindow> m_foreground;
    QPointer<QWindow> m_container;
    void *m_native = nullptr;
    QList<QMetaObject::Connection> m_connections;
    QTimer m_accessibilityPoll;
};

namespace NativeGlass {
bool supported();
bool reducedTransparency();
void *create(QQuickWindow *owner, QQuickWindow *foreground);
QWindow *container(void *);
void update(void *, const QRectF &, qreal radius);
void destroy(void *);
void setVisible(void *, bool);
bool geometryAligned(void *);
QColor foregroundColor(void *);
}
