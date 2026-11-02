#pragma once
#include <QQuickItem>
#include <QQuickWindow>
#include <QPointer>
#include <QTimer>
#include <QtQml/qqmlregistration.h>

class SurfacePalette;

class NativeGlassHost : public QQuickItem {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QSizeF minimumRenderSize READ minimumRenderSize WRITE setMinimumRenderSize NOTIFY minimumRenderSizeChanged)
    Q_PROPERTY(bool blockOutsideInput MEMBER m_blockOutsideInput)
    Q_PROPERTY(QRectF contentRect READ contentRect WRITE setContentRect NOTIFY contentRectChanged)
    Q_PROPERTY(bool windowSurface READ windowSurface WRITE setWindowSurface NOTIFY windowSurfaceChanged)
    Q_PROPERTY(bool requested READ requested WRITE setRequested NOTIFY requestedChanged)
    Q_PROPERTY(bool available READ available CONSTANT)
    Q_PROPERTY(bool reducedTransparency READ reducedTransparency NOTIFY accessibilityChanged)
    Q_PROPERTY(bool active READ active NOTIFY activeChanged)
    Q_PROPERTY(bool preferred READ preferred NOTIFY preferredChanged)
    Q_PROPERTY(QWindow *containerWindow READ containerWindow NOTIFY containerWindowChanged)
    Q_PROPERTY(QColor foregroundColor READ foregroundColor NOTIFY foregroundColorChanged)
    Q_PROPERTY(qreal radius READ radius WRITE setRadius NOTIFY radiusChanged)
    Q_PROPERTY(QQuickItem *content READ content WRITE setContent NOTIFY contentChanged)
public:
    explicit NativeGlassHost(QQuickItem *parent = nullptr);
    ~NativeGlassHost() override;
    QSizeF minimumRenderSize() const { return m_minimumRenderSize; }
    void setMinimumRenderSize(const QSizeF &value) { if (m_minimumRenderSize == value) return; m_minimumRenderSize = value; emit minimumRenderSizeChanged(); schedule(); }
    QRectF contentRect() const { return m_contentRect; }
    void setContentRect(const QRectF &value) { if (m_contentRect == value) return; m_contentRect = value; emit contentRectChanged(); schedule(); }
    bool windowSurface() const { return m_windowSurface; }
    void setWindowSurface(bool);
    bool requested() const { return m_requested; }
    bool available() const;
    bool reducedTransparency() const { return m_reduced; }
    bool active() const { return m_active; }
    bool preferred() const { return m_requested && available() && !m_reduced; }
    QWindow *containerWindow() const { return m_container; }
    QColor foregroundColor() const { return m_color; }
    qreal radius() const { return m_radius; }
    QQuickItem *content() const { return m_content; }
    void setRequested(bool);
    void setRadius(qreal);
    void setContent(QQuickItem *);
    Q_INVOKABLE void focusControl(QQuickItem *);
    Q_INVOKABLE bool geometryAligned() const;
    bool handleOutsidePointer(const QPointF &, bool pressed);
signals:
    void minimumRenderSizeChanged();
    void windowSurfaceChanged();
    void contentRectChanged();
    void escapeRequested();
    void requestedChanged();
    void accessibilityChanged();
    void activeChanged();
    void preferredChanged();
    void outsidePointer(const QPointF &globalPosition, bool pressed);
    void containerWindowChanged();
    void foregroundColorChanged();
    void radiusChanged();
    void contentChanged();
protected:
    bool eventFilter(QObject *, QEvent *) override;
    void itemChange(ItemChange, const ItemChangeData &) override;
private:
    void schedule();
    void synchronize();
    void detach();
    void release();
    void watchGeometry();
    bool m_windowSurface = false;
    bool m_blockOutsideInput = false;
    bool m_requested = false, m_active = false, m_queued = false, m_reduced = false;
    qreal m_radius = 28;
    QRectF m_contentRect;
    QSizeF m_minimumRenderSize;
    QPointF m_originalPosition;
    QColor m_color = Qt::black;
    QPointer<QQuickItem> m_content, m_originalParent;
    QPointer<QQuickWindow> m_foreground;
    QPointer<QWindow> m_container, m_surfaceWindow;
    void *m_native = nullptr;
    QList<QMetaObject::Connection> m_connections;
    QTimer m_accessibilityPoll;
};

namespace NativeGlass {
bool supported();
bool nativeKeyboardAvailable();
void watchKeyboard(SurfacePalette *);
qreal keyboardInset(QQuickItem *, const QRectF &);
bool reducedTransparency();
void *create(QQuickWindow *owner, QQuickWindow *foreground);
void *createWindow(QQuickWindow *, NativeGlassHost *);
QWindow *container(void *);
void update(void *, const QRectF &, qreal radius, const QSizeF &minimumRenderSize = {});
void destroy(void *);
void setVisible(void *, bool);
void setOpacity(void *, qreal);
bool geometryAligned(void *);
// Manual native UI diagnostic; never called by page content.
void activateForTest(QQuickWindow *);
// Posts an AppKit mouse sequence, exercising native event monitors too.
bool clickForTest(QWindow *, const QPointF &globalPosition);
QColor foregroundColor(void *);
}
