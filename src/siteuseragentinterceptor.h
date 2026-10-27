#pragma once
#include "siteuseragent.h"
#include <QHash>
#include <QWebEngineUrlRequestInterceptor>
#include <QWebEngineUrlRequestInfo>

class SiteUserAgentInterceptor final : public QWebEngineUrlRequestInterceptor {
public:
    using QWebEngineUrlRequestInterceptor::QWebEngineUrlRequestInterceptor;
    void interceptRequest(QWebEngineUrlRequestInfo &info) override {
        const auto userAgent = SiteUserAgent::overrideFor(info.requestUrl());
        if (userAgent.isEmpty()) return;
        info.setHttpHeader(QByteArrayLiteral("User-Agent"), userAgent);
        // Do not advertise Chromium/Qt brands alongside a Firefox identity.
        const auto headers = info.httpHeaders();
        for (auto it = headers.cbegin(); it != headers.cend(); ++it)
            if (it.key().toLower().startsWith("sec-ch-ua"))
                info.setHttpHeader(it.key(), QByteArray());
    }
};
