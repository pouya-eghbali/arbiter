#include "splitswapgesture.h"
#include <QGuiApplication>
#include <QQuickWindow>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QStyleHints>

namespace {
// Qt maps ControlModifier to Command on macOS, and to Control elsewhere.
constexpr auto swapModifiers=Qt::ControlModifier | Qt::ShiftModifier;
}
SplitSwapGesture::SplitSwapGesture(QQuickItem *parent) : QQuickItem(parent) {
    qGuiApp->installEventFilter(this);
    connect(this,&QQuickItem::visibleChanged,this,[this] { if(!isVisible()) cancel(); });
    connect(this,&QQuickItem::enabledChanged,this,[this] { if(!isEnabled()) cancel(); });
    connect(this,&QQuickItem::windowChanged,this,[this] { cancel(); });
}
SplitSwapGesture::~SplitSwapGesture() { qGuiApp->removeEventFilter(this); }
void SplitSwapGesture::setPanes(const QVariantList &panes) {
    if(m_panes==panes) return;
    m_panes=panes;
    // A layout mutation or workspace switch invalidates the gesture's target.
    cancel();
}
int SplitSwapGesture::paneAt(const QPointF &point) const {
    for(const auto &value:m_panes) {
        const auto p=value.toMap();
        if(QRectF(p.value("x").toReal(),p.value("y").toReal(),p.value("width").toReal(),p.value("height").toReal()).contains(point))
            return p.value("paneId").toInt();
    }
    return 0;
}
void SplitSwapGesture::cancel() {
    if(!m_source || m_cancelled) return;
    m_cancelled=true; m_dragging=false;
    emit finished(false,m_position);
}
bool SplitSwapGesture::eventFilter(QObject *watched,QEvent *event) {
    if(!window() || watched!=window()) return false;
    if(event->type()==QEvent::WindowDeactivate || event->type()==QEvent::Hide) {
        cancel(); m_source=0; return false;
    }
    if(m_source && (event->type()==QEvent::ShortcutOverride || event->type()==QEvent::KeyPress)
        && static_cast<QKeyEvent *>(event)->key()==Qt::Key_Escape) {
        event->accept(); if(event->type()==QEvent::KeyPress) cancel(); return true;
    }
    if(m_source && event->type()==QEvent::KeyRelease && static_cast<QKeyEvent *>(event)->modifiers()!=swapModifiers) cancel();
    if(event->type()!=QEvent::MouseButtonPress && event->type()!=QEvent::MouseButtonDblClick && event->type()!=QEvent::MouseMove && event->type()!=QEvent::MouseButtonRelease) return false;
    const auto *mouse=static_cast<QMouseEvent *>(event);
    const QPointF position=mapFromScene(mouse->position());
    if(event->type()==QEvent::MouseButtonPress || event->type()==QEvent::MouseButtonDblClick) {
        if(m_source) return true;
        if(!isVisible() || !isEnabled() || !contains(position)) return false;
        const int pane=paneAt(position); if(!pane) return false;
        // The signal focuses empty panes as well as pages, without consuming
        // normal page clicks or changing their selection/drag behavior.
        emit panePressed(pane);
        if(mouse->button()!=Qt::LeftButton || mouse->modifiers()!=swapModifiers) return false;
        m_source=pane; m_origin=m_position=position; m_dragging=m_cancelled=false;
        emit started(pane,position); return true;
    }
    if(!m_source) return false;
    m_position=position;
    if(mouse->modifiers()!=swapModifiers || !isVisible() || !isEnabled()) cancel();
    if(event->type()==QEvent::MouseMove) {
        if(!(mouse->buttons() & Qt::LeftButton)) { cancel(); m_source=0; return true; }
        if(!m_cancelled && (m_dragging || (position-m_origin).manhattanLength()>=qGuiApp->styleHints()->startDragDistance())) {
            m_dragging=true; emit moved(position);
        }
    } else if(mouse->button()==Qt::LeftButton) {
        const bool commit=m_dragging && !m_cancelled;
        m_source=0; m_dragging=false;
        emit finished(commit,position);
    }
    return true;
}
