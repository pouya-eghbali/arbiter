#include "linuxwindowframe.h"
#include <QPainterPath>
#include <QPlatformSurfaceEvent>
#ifdef Q_OS_LINUX
#include <qpa/qplatformwindow_p.h>
#include <QGuiApplication>
#include <QtGui/qguiapplication_platform.h>
#include <xcb/shape.h>
#include <xcb/xcb.h>
#endif

void LinuxWindowFrame::setWindow(QQuickWindow *window)
{
    if (m_window == window) return;
    if (m_window) {
        m_window->removeEventFilter(this);
        disconnect(m_window, nullptr, this, nullptr);
    }
    m_window = window;
    if (m_window) {
        m_window->installEventFilter(this);
        connect(m_window, &QWindow::widthChanged, this, &LinuxWindowFrame::updateFrame);
        connect(m_window, &QWindow::heightChanged, this, &LinuxWindowFrame::updateFrame);
        connect(m_window, &QWindow::visibleChanged, this, &LinuxWindowFrame::updateFrame);
    }
    updateFrame();
    emit windowChanged();
}

void LinuxWindowFrame::setShadowMargin(int margin)
{
    margin = qBound(0, margin, 48);
    if (m_margin == margin) return;
    m_margin = margin;
    updateFrame();
    emit shadowMarginChanged();
}

bool LinuxWindowFrame::eventFilter(QObject *object, QEvent *event)
{
    if (object == m_window && event->type() == QEvent::PlatformSurface
        && static_cast<QPlatformSurfaceEvent *>(event)->surfaceEventType() == QPlatformSurfaceEvent::SurfaceCreated) {
        m_window->setProperty("arbiterShadowMargins", QVariant());
        QMetaObject::invokeMethod(this, &LinuxWindowFrame::updateFrame, Qt::QueuedConnection);
    }
    return QObject::eventFilter(object, event);
}

void LinuxWindowFrame::updateFrame()
{
    if (!m_window || !m_window->handle()) return;
#ifdef Q_OS_LINUX
#if QT_CONFIG(wayland)
    if (auto *native = m_window->nativeInterface<QNativeInterface::Private::QWaylandWindow>()) {
        // Qt's API updates geometry too; only call when the margins change.
        const QMargins margins(m_margin, m_margin, m_margin, m_margin);
        if (m_window->property("arbiterShadowMargins").value<QMargins>() != margins) {
            m_window->setProperty("arbiterShadowMargins", QVariant::fromValue(margins));
            native->setCustomMargins(margins);
        }
    }
#endif
    if (m_margin > 0) {
        QPainterPath shape;
        shape.addRoundedRect(QRectF(QPointF(), m_window->size()).adjusted(m_margin, m_margin, -m_margin, -m_margin), 12, 12);
        const QRegion region(shape.toFillPolygon().toPolygon());
#if QT_CONFIG(xcb)
        if (auto *native = qGuiApp->nativeInterface<QNativeInterface::QX11Application>()) {
            auto *connection = native->connection();
            const auto id = static_cast<xcb_window_t>(m_window->winId());
            const auto atom = xcb_intern_atom_reply(connection, xcb_intern_atom(connection, false, 18, "_GTK_FRAME_EXTENTS"), nullptr);
            if (atom) {
                const uint32_t extent = qRound(m_margin * m_window->devicePixelRatio());
                const uint32_t extents[] = {extent, extent, extent, extent};
                xcb_change_property(connection, XCB_PROP_MODE_REPLACE, id, atom->atom, XCB_ATOM_CARDINAL, 32, 4, extents);
                free(atom);
            }
            QList<xcb_rectangle_t> rectangles;
            const qreal scale = m_window->devicePixelRatio();
            for (const QRect &rect : region)
                rectangles.append({int16_t(qRound(rect.x() * scale)), int16_t(qRound(rect.y() * scale)),
                    uint16_t(qRound(rect.width() * scale)), uint16_t(qRound(rect.height() * scale))});
            xcb_shape_rectangles(connection, XCB_SHAPE_SO_SET, XCB_SHAPE_SK_INPUT, XCB_CLIP_ORDERING_UNSORTED,
                id, 0, 0, rectangles.size(), rectangles.constData());
            xcb_flush(connection);
            return;
        }
#endif
        // Wayland's mask is the input region, not the visual bounding shape.
        m_window->setMask(region);
    } else {
        m_window->setMask({});
#if QT_CONFIG(xcb)
        if (auto *native = qGuiApp->nativeInterface<QNativeInterface::QX11Application>()) {
            auto *connection = native->connection();
            const auto id = static_cast<xcb_window_t>(m_window->winId());
            const auto atom = xcb_intern_atom_reply(connection, xcb_intern_atom(connection, false, 18, "_GTK_FRAME_EXTENTS"), nullptr);
            if (atom) { xcb_delete_property(connection, id, atom->atom); free(atom); }
            xcb_shape_mask(connection, XCB_SHAPE_SO_SET, XCB_SHAPE_SK_INPUT, id, 0, 0, XCB_PIXMAP_NONE);
            xcb_flush(connection);
        }
#endif
    }
#endif
}
