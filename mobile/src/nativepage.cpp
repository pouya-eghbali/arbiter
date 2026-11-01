#include "nativepage.h"
#include "mobileplatform.h"
QHash<QString, QPointer<NativePage>> NativePage::pages;
NativePage::~NativePage() {
    pages.remove(tabId);
    delete m_window;
    MobilePlatform::destroyWebView(this);
}
void NativePage::setOverlayRect(const QRectF &rect) {
    if (m_overlayRect == rect)
        return;
    m_overlayRect = rect;
    emit overlayRectChanged();
    if (m_window)
        MobilePlatform::setOverlayRect(this, rect);
}
void NativePage::setCoverProgress(qreal value) {
    value = qBound(0., value, 1.);
    if (qFuzzyCompare(m_coverProgress, value)) return;
    m_coverProgress = value;
    if (m_window) MobilePlatform::setCoverProgress(this, value);
    emit coverProgressChanged();
}
void NativePage::capture() {
    if (m_window)
        MobilePlatform::capture(this);
}
void NativePage::find(const QString &text, bool backwards) {
    if (m_window)
        MobilePlatform::find(this, text, backwards);
}
void NativePage::start() {
    if (m_window || m_starting)
        return;
    m_starting = true;
    pages[tabId] = this;
    QPointer<NativePage> guard(this);
    MobilePlatform::createWebViewAsync(this, profile, privateMode, [guard](QWindow *window) {
        if (!guard) { delete window; return; }
        guard->m_starting = false;
        guard->m_window = window;
        emit guard->nativeWindowChanged();
        if (!window) return;
        MobilePlatform::setOverlayRect(guard, guard->m_overlayRect);
        MobilePlatform::setCoverProgress(guard, guard->m_coverProgress);
        if (!guard->m_url.isEmpty()) MobilePlatform::navigate(guard, guard->m_url);
    });
}
void NativePage::setUrl(const QUrl &value) {
    if (m_url == value)
        return;
    m_url = value;
    emit changed();
    if (m_window)
        MobilePlatform::navigate(this, value);
}
void NativePage::action(const QString &name) {
    MobilePlatform::action(this, name);
}
void NativePage::configure() {
    MobilePlatform::configurePage(this);
}
void NativePage::evaluate(const QString &script, std::function<void(QVariant)> callback) {
    MobilePlatform::evaluate(this, script, std::move(callback));
}
void NativePage::event(const QString &type, const QVariantMap &data) {
    if (type == "location") {
        m_url = data.value("url").toUrl();
        canBack = data.value("back").toBool();
        canForward = data.value("forward").toBool();
    }
    if (type == "title")
        m_title = data.value("title").toString();
    if (type == "progress")
        progress = data.value("progress").toInt();
    if (type == "start")
        loading = true;
    if (type == "finish" || type == "error")
        loading = false;
    emit changed();
    emit browserEvent(tabId, type, data);
}
