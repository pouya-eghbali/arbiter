#include "../src/siteuseragentinterceptor.h"
#include <QApplication>
#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QtTest>

class ObservedUserAgent final : public QWebEngineUrlRequestInterceptor {
    Q_OBJECT
    SiteUserAgentInterceptor delegate;
public:
    void interceptRequest(QWebEngineUrlRequestInfo &info) override {
        if (info.requestUrl().scheme() != "https" && info.requestUrl().scheme() != "http") return;
        const auto before = info.httpHeaders().value("User-Agent");
        delegate.interceptRequest(info);
        const auto after = info.httpHeaders().value("User-Agent");
        // Capture real Qt request headers, then block before DNS/network access.
        info.block(true);
        emit observed(info.requestUrl(), before, after);
    }
signals:
    void observed(const QUrl &url, const QByteArray &before, const QByteArray &after);
};

class SiteUserAgentWebEngineTest : public QObject {
    Q_OBJECT
private slots:
    void sharedProfileKeepsPerDestinationHeaders() {
        QWebEngineProfile profile;
        profile.setHttpUserAgent("Arbiter baseline test agent");
        ObservedUserAgent interceptor;
        profile.setUrlRequestInterceptor(&interceptor);
        QSignalSpy requests(&interceptor, &ObservedUserAgent::observed);
        QWebEnginePage google(&profile), other(&profile);
        google.load(QUrl("https://accounts.google.com/arbiter-user-agent-test"));
        other.load(QUrl("https://google.com.evil.example/arbiter-user-agent-test"));
        QTRY_VERIFY_WITH_TIMEOUT(requests.size() >= 2, 10000);
        bool googleSeen = false, otherSeen = false;
        for (const auto &request : requests) {
            const auto url = request[0].toUrl();
            if (url.host() == "accounts.google.com") {
                QCOMPARE(request[2].toByteArray(), SiteUserAgent::overrideFor(url));
                googleSeen = true;
            } else if (url.host() == "google.com.evil.example") {
                QCOMPARE(request[2].toByteArray(), request[1].toByteArray());
                otherSeen = true;
            }
        }
        QVERIFY(googleSeen); QVERIFY(otherSeen);
        QCOMPARE(profile.httpUserAgent(), QString("Arbiter baseline test agent"));
    }
    void googleSubframeOnAnotherSite() {
        QWebEngineProfile profile;
        ObservedUserAgent interceptor;
        profile.setUrlRequestInterceptor(&interceptor);
        QSignalSpy requests(&interceptor, &ObservedUserAgent::observed);
        QWebEnginePage page(&profile);
        page.setHtml("<iframe src='https://accounts.google.com/arbiter-frame-test'></iframe>", QUrl("https://unrelated.example/"));
        QTRY_VERIFY_WITH_TIMEOUT(!requests.isEmpty(), 10000);
        const auto request = requests.first();
        QCOMPARE(request[0].toUrl().host(), QString("accounts.google.com"));
        QCOMPARE(request[2].toByteArray(), SiteUserAgent::overrideFor(request[0].toUrl()));
    }
};
QTEST_MAIN(SiteUserAgentWebEngineTest)
#include "siteuseragent_webengine_test.moc"
