#include "surfacepalette.h"
#include "nativeglasshost.h"
#include <QHash>
#include <QPointer>
#include <QGuiApplication>
#include <QEasingCurve>
namespace {
QHash<QWindow *, QColor> colors;
QList<QPointer<SurfacePalette>> palettes;
}
SurfacePalette::SurfacePalette(QObject *parent) : QObject(parent) {
    palettes.append(this);
    NativeGlass::watchKeyboard(this);
    connect(qGuiApp, &QGuiApplication::focusWindowChanged, this, [this] { ++m_revision; emit changed(); });
}
bool SurfacePalette::nativeKeyboardAvailable() const { return NativeGlass::nativeKeyboardAvailable(); }
qreal SurfacePalette::keyboardInset(QQuickItem *item) const { return NativeGlass::keyboardInset(item, m_keyboardFrame); }
void SurfacePalette::setKeyboardFrame(const QRectF &frame, int duration, int curve) {
    m_keyboardFrame = frame;
    m_keyboardDuration = qMax(0, duration);
    m_keyboardEasing = curve == 0 ? QEasingCurve::InOutCubic : curve == 1 ? QEasingCurve::InCubic
        : curve == 3 ? QEasingCurve::Linear : QEasingCurve::OutCubic;
    ++m_keyboardRevision;
    emit keyboardChanged();
}
qreal SurfacePalette::keyboardTop(QQuickItem *target, qreal y) const {
    auto focused = qGuiApp->focusWindow();
    return target && focused ? target->mapFromGlobal(focused->mapToGlobal(QPointF(0, y))).y() : y;
}
bool SurfacePalette::windowSupported() const {
#ifdef Q_OS_MACOS
    return NativeGlass::supported();
#else
    return false;
#endif
}
void SurfacePalette::conceal(QQuickItem *item) const {
    auto window = item ? item->window() : nullptr;
    if (!window) return;
    if (popupWindow(window)) window->hide();
    else if (colors.contains(window) && window->property("nativeGlassForeground").toBool()) {
        window->hide();
        if (window->parent()) window->parent()->hide();
    }
}
QWindow *SurfacePalette::owner(QQuickItem *item) const {
    QWindow *window = item ? item->window() : nullptr;
    for (int i = 0; window && i < 32; ++i) {
        auto parent = window->parent() ? window->parent() : window->transientParent();
        if (!parent || parent == window) break;
        window = parent;
    }
    return window;
}
bool SurfacePalette::popupWindow(QWindow *window) const { return window && !window->property("nativeGlassForeground").toBool()
    && (window->type() == Qt::Popup || window->type() == Qt::ToolTip); }
bool SurfacePalette::active(QQuickItem *item) const { return item && colors.contains(item->window()); }
QColor SurfacePalette::foreground(QQuickItem *item) const { return item ? colors.value(item->window(), Qt::black) : Qt::black; }
void SurfacePalette::setForeground(QWindow *window, const QColor &color) {
    if (!window || (color.isValid() ? colors.value(window) == color && colors.contains(window) : !colors.contains(window))) return;
    const bool first = !colors.contains(window);
    if (color.isValid()) colors.insert(window, color); else colors.remove(window);
    if (first && color.isValid()) QObject::connect(window, &QObject::destroyed, [window] { SurfacePalette::setForeground(window, QColor()); });
    for (auto it = palettes.begin(); it != palettes.end();) {
        if (!*it) { it = palettes.erase(it); continue; }
        ++(*it)->m_revision;
        emit (*it)->changed();
        ++it;
    }
}
