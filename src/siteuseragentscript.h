#pragma once
#include "siteuseragent.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QWebEngineScript>

namespace SiteUserAgent {
inline QWebEngineScript documentScript() {
    const auto agent = QString::fromLatin1(overrideFor(QUrl(QStringLiteral("https://google.com"))));
    const auto encoded = QString::fromUtf8(QJsonDocument(QJsonArray{agent}).toJson(QJsonDocument::Compact));
    QWebEngineScript script;
    script.setName(QStringLiteral("Arbiter Google browser identity"));
    script.setInjectionPoint(QWebEngineScript::DocumentCreation);
    script.setWorldId(QWebEngineScript::MainWorld);
    script.setRunsOnSubFrames(true);
    script.setSourceCode(QStringLiteral(R"JS((() => {
        if (location.protocol !== 'https:' && location.protocol !== 'http:') return;
        const host = location.hostname.toLowerCase().replace(/\.$/, '');
        if (host !== 'google.com' && !host.endsWith('.google.com')) return;
        const ua = %1[0];
        const appVersion = ua.includes('Macintosh') ? '5.0 (Macintosh)'
            : ua.includes('Windows') ? '5.0 (Windows)' : '5.0 (X11)';
        for (const [key, value] of Object.entries({userAgent: ua, appVersion,
                vendor: '', productSub: '20100101'})) {
            Object.defineProperty(Navigator.prototype, key, {get: () => value, configurable: true});
        }
        // Firefox does not expose Chromium's User-Agent Client Hints API.
        Reflect.deleteProperty(Navigator.prototype, 'userAgentData');
    })(); )JS").arg(encoded));
    return script;
}
}
