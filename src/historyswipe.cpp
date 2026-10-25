#include "historyswipe.h"
#include <QVariant>
#include <QDebug>
#include <algorithm>
#include <cmath>

namespace { constexpr auto bridge="_arbiterHistorySwipe"; }
HistorySwipe::HistorySwipe(QObject *parent) : QObject(parent) {
    m_timeout.setSingleShot(true);
    m_timeout.setInterval(1500);
    connect(&m_timeout,&QTimer::timeout,this,&HistorySwipe::cancel);
}
HistorySwipe::~HistorySwipe() {
    if(m_view && m_view->property(bridge).value<QObject *>()==this) m_view->setProperty(bridge,QVariant());
}
void HistorySwipe::setView(QQuickItem *view) {
    if(view==m_view) return;
    cancel();
    if(m_view && m_view->property(bridge).value<QObject *>()==this) m_view->setProperty(bridge,QVariant());
    m_view=view;
    if(view) view->setProperty(bridge,QVariant::fromValue(static_cast<QObject *>(this)));
    emit viewChanged();
}
void HistorySwipe::setEnabled(bool value) {
    if(m_enabled==value) return;
    m_enabled=value; if(!value) cancel(); emit enabledChanged();
}
void HistorySwipe::setCanGoBack(bool value) {
    if(m_back==value) return;
    m_back=value; if(!value && m_direction==-1) cancel(); emit availabilityChanged();
}
void HistorySwipe::setCanGoForward(bool value) {
    if(m_forward==value) return;
    m_forward=value; if(!value && m_direction==1) cancel(); emit availabilityChanged();
}
double HistorySwipe::threshold() const { return std::clamp(m_view ? m_view->width()*0.18 : 180.,120.,240.); }
double HistorySwipe::progress() const { return active() ? std::clamp(std::abs(m_x)/threshold(),0.,1.) : 0.; }
void HistorySwipe::cancel() {
    const bool wasActive=active();
    m_timeout.stop(); m_tracking=false; m_updated=false; m_direction=0; m_boundaryDirection=0;
    m_x=m_y=m_totalX=m_totalY=m_reverse=0;
    if(wasActive) emit changed();
}
void HistorySwipe::beginGesture(bool eligible) {
    if(qEnvironmentVariableIsSet("ARBITER_SWIPE_TRACE")) qInfo()<<"Swipe begin"<<eligible;
    cancel();
    m_tracking=eligible && m_enabled && m_view;
    if(m_tracking) m_timeout.start();
}
void HistorySwipe::scrollUpdate(double dx,double dy,bool consumed) {
    if(qEnvironmentVariableIsSet("ARBITER_SWIPE_TRACE")) qInfo()<<"Swipe scroll"<<dx<<dy<<consumed<<m_tracking;
    if(!m_tracking) return;
    // A swipe which scrolls content belongs to that content for its entire
    // lifetime. At the boundary the user starts a new swipe, as in Chrome.
    if(consumed || !std::isfinite(dx) || !std::isfinite(dy)) { cancel(); return; }
    m_updated=true; m_timeout.start();
    m_x+=dx; m_y+=dy;
    m_totalX+=std::abs(dx); m_totalY+=std::abs(dy);
    if(m_totalY>12 && m_totalY*1.5>m_totalX) { cancel(); return; }
    if(m_direction && dx*m_direction>0) {
        m_reverse+=std::abs(dx);
        if(m_reverse>12) { cancel(); return; }
    }
    updateFeedback();
}
void HistorySwipe::overscroll(double dx,double dy,bool allowed) {
    if(qEnvironmentVariableIsSet("ARBITER_SWIPE_TRACE")) qInfo()<<"Swipe overscroll"<<dx<<dy<<allowed<<m_tracking;
    if(!m_tracking) return;
    if(!allowed || !std::isfinite(dx) || !std::isfinite(dy)) { cancel(); return; }
    m_timeout.start();
    if(std::abs(dx)>std::abs(dy)*2.5 && dx!=0) {
        m_boundaryDirection=dx>0 ? -1 : 1;
        updateFeedback();
    }
}
void HistorySwipe::updateFeedback() {
    // Root overscroll authorizes the gesture. Progress uses ACK'd input DIPs,
    // not compositor offsets, whose units differ with scale/elasticity.
    if(!m_boundaryDirection || std::abs(m_x)<=24 || std::abs(m_x)<=std::abs(m_y)*2.5) return;
    const int direction=m_x>0 ? -1 : 1;
    if(direction!=m_boundaryDirection || (direction==-1 && !m_back) || (direction==1 && !m_forward)
        || (m_direction && m_direction!=direction)) { cancel(); return; }
    m_direction=direction; emit changed();
}
void HistorySwipe::endGesture() {
    if(qEnvironmentVariableIsSet("ARBITER_SWIPE_TRACE")) qInfo()<<"Swipe end"<<m_tracking<<m_updated<<m_direction<<progress();
    const bool commit=m_enabled && m_view && active() && m_updated && progress()>=1;
    const int direction=m_direction;
    cancel();
    if(commit) emit triggered(direction);
}
