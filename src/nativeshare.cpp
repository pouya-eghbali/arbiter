#include "nativeshare.h"
#include "browserpreferences.h"
#include <QTimer>
#include <utility>
NativeShare::~NativeShare() { cancel(); }
void NativeShare::setHost(QWindow *window) {
    if (m_host==window) return;
    cancel(); m_host=window; emit hostChanged();
}
bool NativeShare::canShare() const {
    return m_host && m_owner && !m_host->property("privateCovered").toBool()
        && m_host->property("webView").value<QObject *>()==m_owner;
}
bool NativeShare::show(QObject *owner,const QUrl &url,const QString &title,const QPointF &anchor) {
    cancel(); m_error.clear(); emit errorChanged();
    m_owner=owner; m_link=BrowserPreferences::normalizedShareUrl(url);
    m_title=title.isEmpty() ? (m_link.isLocalFile() ? m_link.fileName() : m_link.host()) : title;
    if (m_title.isEmpty()) m_title=tr("Shared page");
    if (!canShare() || m_link.isEmpty()) return false;
    m_state=createNativeShare(this,&m_error);
    if (!m_state) { emit errorChanged(); return false; }
    m_active=true; emit activeChanged();
    if (!presentNativeShare(m_state,anchor,&m_error)) { cancel(); emit errorChanged(); return false; }
    return true;
}
void NativeShare::cancel() {
    ++m_generation;
    auto *state=std::exchange(m_state,nullptr);
    m_owner=nullptr;
    if (state) destroyNativeShare(state);
    if (m_active) { m_active=false; emit activeChanged(); }
}
void NativeShare::cancelFor(QObject *owner) { if (m_owner==owner) cancel(); }
void NativeShare::copyLink(quint64 generation) { if (generation==m_generation && m_state && canShare()) BrowserPreferences::copySharedLink(m_link); }
void NativeShare::finished(void *token,quint64 generation) {
    if (m_state!=token || generation!=m_generation) return;
    if (m_active) { m_active=false; emit activeChanged(); }
    // Native delegates can still be on the stack when a service is chosen.
    QTimer::singleShot(0,this,[this,token,generation] { if (m_state==token && generation==m_generation) cancel(); });
}
