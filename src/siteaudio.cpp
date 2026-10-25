#include "siteaudio.h"
#include "settings.h"

SiteAudio::SiteAudio(bool persistent, QObject *parent) : QObject(parent), m_persistent(persistent) {
    const auto hosts = Settings().value("Audio/mutedSites").toStringList();
    for (const auto &host : hosts) {
        QUrl url; url.setScheme("https"); url.setHost(host);
        const auto normalized = siteHost(url);
        if (!normalized.isEmpty()) m_muted.insert(normalized);
    }
}
QString SiteAudio::siteHost(const QUrl &url) {
    if (!url.isValid() || (url.scheme() != "https" && url.scheme() != "http")) return {};
    auto host = url.host().toLower();
    if (host.endsWith('.')) host.chop(1);
    return host;
}
bool SiteAudio::isMuted(const QUrl &url) const {
    const auto host = siteHost(url);
    return !host.isEmpty() && m_muted.contains(host);
}
bool SiteAudio::setMuted(const QUrl &url, bool muted) {
    const auto host = siteHost(url);
    if (host.isEmpty()) return false;
    if (m_muted.contains(host) == muted) return true;
    auto next = m_muted;
    if (muted) next.insert(host); else next.remove(host);
    if (m_persistent) {
        QStringList hosts(next.begin(), next.end()); hosts.sort();
        Settings settings; settings.setValue("Audio/mutedSites", hosts);
        if (settings.status() != QSettings::NoError) return false;
    }
    m_muted = next; ++m_revision; emit changed(); return true;
}
