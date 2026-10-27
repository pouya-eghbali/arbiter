#pragma once
#include <QByteArray>
#include <QString>
#include <QUrl>

namespace SiteUserAgent {
inline QByteArray overrideFor(const QUrl &url) {
    if (!url.isValid() || (url.scheme() != QStringLiteral("https") && url.scheme() != QStringLiteral("http"))) return {};
    auto host = url.host().toLower();
    if (host.endsWith('.')) host.chop(1);
    if (host != QStringLiteral("google.com") && !host.endsWith(QStringLiteral(".google.com"))) return {};

    // Google rejects some Qt-based browsers during sign-in. Keep this request
    // quirk domain-scoped; changing a shared profile's UA would affect other
    // tabs and split views. Workaround:
    // https://github.com/qutebrowser/qutebrowser/issues/5182
    // Firefox stable version verified on 2026-10-27:
    // https://product-details.mozilla.org/1.0/firefox_versions.json
#if defined(Q_OS_WIN)
    constexpr auto platform = "Windows NT 10.0; Win64; x64";
#elif defined(Q_OS_MACOS)
    constexpr auto platform = "Macintosh; Intel Mac OS X 10.15";
#elif defined(Q_PROCESSOR_ARM_64)
    constexpr auto platform = "X11; Linux aarch64";
#else
    constexpr auto platform = "X11; Linux x86_64";
#endif
    return QByteArray("Mozilla/5.0 (") + platform + "; rv:159.0) Gecko/20100101 Firefox/159.0";
}
}
