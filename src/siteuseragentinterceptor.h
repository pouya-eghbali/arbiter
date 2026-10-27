#pragma once
#include "siteuseragent.h"
#include <QWebEngineUrlRequestInterceptor>
#include <QWebEngineUrlRequestInfo>

class SiteUserAgentInterceptor final : public QWebEngineUrlRequestInterceptor {
public:
    using QWebEngineUrlRequestInterceptor::QWebEngineUrlRequestInterceptor;
    void interceptRequest(QWebEngineUrlRequestInfo &info) override {
        const auto userAgent = SiteUserAgent::overrideFor(info.requestUrl());
        if (!userAgent.isEmpty()) info.setHttpHeader(QByteArrayLiteral("User-Agent"), userAgent);
    }
};
