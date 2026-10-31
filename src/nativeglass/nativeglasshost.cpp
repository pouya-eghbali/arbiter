#include "nativeglasshost.h"
#include "surfacepalette.h"
#include <QGuiApplication>
#include <QKeyEvent>

NativeGlassHost::NativeGlassHost(QQuickItem *parent) : QQuickItem(parent) {
    connect(this, &QQuickItem::windowChanged, this, [this] { release(); watchGeometry(); schedule(); });
    connect(this, &QQuickItem::parentChanged, this, [this] { watchGeometry(); schedule(); });
    m_accessibilityPoll.setInterval(250);
    connect(&m_accessibilityPoll, &QTimer::timeout, this, &NativeGlassHost::schedule);
    connect(qGuiApp, &QGuiApplication::applicationStateChanged, this, &NativeGlassHost::schedule);
}
NativeGlassHost::~NativeGlassHost() { release(); }
bool NativeGlassHost::geometryAligned() const { return m_active && m_native && NativeGlass::geometryAligned(m_native); }
bool NativeGlassHost::available() const { return NativeGlass::supported(); }
void NativeGlassHost::setWindowSurface(bool value) {
    if (m_windowSurface == value) return;
    release(); m_windowSurface = value; emit windowSurfaceChanged(); schedule();
}
void NativeGlassHost::setRequested(bool value) {
    if (m_requested == value) return;
    m_requested = value;
    if (value && isVisible()) m_accessibilityPoll.start(); else m_accessibilityPoll.stop();
    emit requestedChanged(); schedule();
}
void NativeGlassHost::setRadius(qreal value) {
    if (m_radius == value) return;
    m_radius = qMax(0., value); emit radiusChanged(); schedule();
}
void NativeGlassHost::setContent(QQuickItem *value) {
    if (m_content == value) return;
    release(); m_content = value; m_originalParent = value ? value->parentItem() : nullptr;
    emit contentChanged(); schedule();
}
void NativeGlassHost::focusControl(QQuickItem *item) {
    if (m_foreground && m_active) m_foreground->requestActivate();
    if (item) item->forceActiveFocus(Qt::ShortcutFocusReason);
}
bool NativeGlassHost::eventFilter(QObject *object, QEvent *event) {
    if (object == m_foreground && (event->type() == QEvent::TouchBegin || event->type() == QEvent::MouseButtonPress)) m_foreground->requestActivate();
    if (object == m_foreground && event->type() == QEvent::KeyPress && static_cast<QKeyEvent *>(event)->key() == Qt::Key_Escape) { emit escapeRequested(); return true; }
    return QQuickItem::eventFilter(object, event);
}
void NativeGlassHost::itemChange(ItemChange change, const ItemChangeData &data) {
    QQuickItem::itemChange(change, data);
    if (change == ItemVisibleHasChanged) {
        if (isVisible() && m_requested) m_accessibilityPoll.start(); else m_accessibilityPoll.stop();
    }
    if ((change == ItemVisibleHasChanged || change == ItemEnabledHasChanged) &&
        (!isVisible() || !isEnabled()) && m_native)
        NativeGlass::setVisible(m_native, false);
    if (change == ItemVisibleHasChanged || change == ItemEnabledHasChanged || change == ItemSceneChange)
        schedule();
}
void NativeGlassHost::watchGeometry() {
    for (auto connection : m_connections) disconnect(connection);
    m_connections.clear();
    for (QQuickItem *item = this; item; item = item->parentItem()) {
        for (auto signal : {&QQuickItem::xChanged, &QQuickItem::yChanged, &QQuickItem::widthChanged,
                            &QQuickItem::heightChanged, &QQuickItem::scaleChanged, &QQuickItem::rotationChanged,
                            &QQuickItem::visibleChanged, &QQuickItem::opacityChanged})
            m_connections.append(connect(item, signal, this, &NativeGlassHost::schedule));
    }
    if (window()) {
        m_connections.append(connect(window(), &QWindow::visibleChanged, this, &NativeGlassHost::schedule));
        m_connections.append(connect(window(), &QWindow::widthChanged, this, &NativeGlassHost::schedule));
        m_connections.append(connect(window(), &QWindow::heightChanged, this, &NativeGlassHost::schedule));
    }
}
void NativeGlassHost::schedule() {
    if (m_queued) return;
    m_queued = true;
    QMetaObject::invokeMethod(this, [this] { m_queued = false; synchronize(); }, Qt::QueuedConnection);
}
void NativeGlassHost::synchronize() {
    const bool reduced = NativeGlass::reducedTransparency() || qEnvironmentVariableIsSet("ARBITER_GLASS_FORCE_OPAQUE");
    if (m_reduced != reduced) { m_reduced = reduced; emit accessibilityChanged(); }
    bool opaque = true;
    for (QQuickItem *item = this; item; item = item->parentItem())
        opaque &= item->opacity() >= .999;
    if (!m_requested || !available() || m_reduced || !isVisible() || !isEnabled() ||
        !window() || !window()->isVisible() || (!m_windowSurface && !m_content) || width() < 1 || height() < 1 || !opaque) {
        detach(); return;
    }
    if (m_windowSurface) {
        if (!m_native) { m_native = NativeGlass::createWindow(window()); if (m_native) m_surfaceWindow = window(); }
        if (!m_native) return;
        NativeGlass::update(m_native, QRectF(QPointF(), window()->size()), m_radius);
        const auto color = NativeGlass::foregroundColor(m_native);
        SurfacePalette::setForeground(window(), color);
        if (m_color != color) { m_color = color; emit foregroundColorChanged(); }
        if (!m_active) { m_active = true; emit activeChanged(); }
        return;
    }
    if (!m_foreground) {
        m_foreground = new QQuickWindow;
        m_foreground->installEventFilter(this);
        m_foreground->setColor(Qt::transparent);
        m_foreground->setFlags(Qt::FramelessWindowHint);
        m_foreground->setParent(window());
        m_foreground->setProperty("nativeGlassForeground", true);
        m_foreground->setGeometry(mapRectToScene(boundingRect()).toAlignedRect());
        m_foreground->create();
        m_native = NativeGlass::create(window(), m_foreground);
        if (!m_native) { delete m_foreground.data(); m_foreground = nullptr; return; }
        m_container = NativeGlass::container(m_native);
        emit containerWindowChanged();
    }
    if (!m_active) {
        m_originalParent = m_content->parentItem();
        m_originalPosition = m_content->position();
        QPointer<QQuickItem> focused = window()->activeFocusItem();
        const bool restoreFocus = focused && (focused == m_content || m_content->isAncestorOf(focused));
        m_content->setParentItem(m_foreground->contentItem());
        if (m_container) m_container->show();
        m_foreground->show();
        if (restoreFocus && focused) { m_foreground->requestActivate(); focused->forceActiveFocus(); }
        m_active = true;
        if (qEnvironmentVariableIsSet("ARBITER_DIAGNOSTICS")) qInfo("Native Liquid Glass attached");
        emit activeChanged();
    }
    const auto rect = mapRectToScene(boundingRect());
    if (m_container) {
        // Qt and UIKit must agree: the material carries the scene offset,
        // while its foreground has local (0,0) geometry inside contentView.
        m_container->setGeometry(rect.toAlignedRect());
        m_foreground->setGeometry(QRect(QPoint(), rect.size().toSize()));
    } else m_foreground->setGeometry(rect.toAlignedRect());
    m_content->setPosition(m_contentRect.isValid() ? m_contentRect.topLeft() : QPointF());
    m_content->setSize(m_contentRect.isValid() ? m_contentRect.size() : size());
    NativeGlass::update(m_native, rect, m_radius);
    NativeGlass::setVisible(m_native, true);
    const auto color = NativeGlass::foregroundColor(m_native);
    SurfacePalette::setForeground(m_foreground, color);
    if (m_color != color) { m_color = color; emit foregroundColorChanged(); }
}
void NativeGlassHost::detach() {
    if (m_windowSurface) {
        if (m_native) { SurfacePalette::setForeground(m_surfaceWindow, QColor()); NativeGlass::destroy(m_native); }
        m_surfaceWindow = nullptr;
        m_native = nullptr;
        if (m_active) { m_active = false; emit activeChanged(); }
        return;
    }
    SurfacePalette::setForeground(m_foreground, QColor());
    const QPointer<QWindow> owner = window();
    const bool refocus = m_active && m_foreground && qGuiApp->focusWindow() == m_foreground && qGuiApp->applicationState() == Qt::ApplicationActive;
    if (m_content && m_active && (!m_foreground || m_content->parentItem() == m_foreground->contentItem())) {
        m_content->setParentItem(m_originalParent);
        m_content->setPosition(m_originalPosition);
    }
    if (m_foreground) m_foreground->hide();
    if (m_container) m_container->hide();
    if (m_native) NativeGlass::setVisible(m_native, false);
    if (refocus && owner && owner->isVisible()) owner->requestActivate();
    if (m_active) { m_active = false; emit activeChanged(); }
}
void NativeGlassHost::release() {
    detach();
    // Keep native windows alive across ordinary panel transitions. Destroying
    // the input window after a tap races UIKit's remaining touch callbacks.
    if (m_native) NativeGlass::destroy(m_native);
    m_native = nullptr;
    if (m_foreground) { m_foreground->setParent(nullptr); m_foreground->deleteLater(); }
    m_foreground = nullptr;
    if (m_container) { m_container->setParent(nullptr); m_container->deleteLater(); }
    m_container = nullptr;
    emit containerWindowChanged();
}
#ifndef Q_OS_DARWIN
namespace NativeGlass {
bool supported() { return false; }
bool reducedTransparency() { return false; }
void *create(QQuickWindow *, QQuickWindow *) { return nullptr; }
void *createWindow(QQuickWindow *) { return nullptr; }
QWindow *container(void *) { return nullptr; }
void update(void *, const QRectF &, qreal) {}
void destroy(void *) {}
void setVisible(void *, bool) {}
bool geometryAligned(void *) { return false; }
void activateForTest(QQuickWindow *window) { window->requestActivate(); }
QColor foregroundColor(void *) { return Qt::black; }
}
#endif
