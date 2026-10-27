#include "../src/siteuseragentinterceptor.h"
#include "../src/siteuseragentscript.h"
#include <QWebEngineScriptCollection>
#include <QJsonDocument>
#include <QJsonObject>
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
        info.setHttpHeader("Sec-CH-UA", "\"Chromium\";v=\"140\"");
        delegate.interceptRequest(info);
        const auto after = info.httpHeaders().value("User-Agent");
        const auto hints = info.httpHeaders().value("Sec-CH-UA");
        // Capture real Qt request headers, then block before DNS/network access.
        info.block(true);
        emit observed(info.requestUrl(), before, after, hints);
    }
signals:
    void observed(const QUrl &url, const QByteArray &before, const QByteArray &after, const QByteArray &hints);
};

class SiteUserAgentWebEngineTest : public QObject {
    Q_OBJECT
private slots:
    void javascriptIdentityBeforePageScripts_data() {
        QTest::addColumn<QUrl>("url");
        QTest::addColumn<bool>("google");
        QTest::newRow("accounts") << QUrl("https://accounts.google.com/") << true;
        QTest::newRow("mail") << QUrl("https://mail.google.com/") << true;
        QTest::newRow("trailing-dot") << QUrl("https://google.com./") << true;
        QTest::newRow("lookalike") << QUrl("https://google.com.evil.example/") << false;
        QTest::newRow("normal-site") << QUrl("https://example.com/") << false;
    }
    void javascriptIdentityBeforePageScripts() {
        QFETCH(QUrl, url); QFETCH(bool, google);
        QWebEngineProfile profile;
        profile.setHttpUserAgent("Arbiter baseline test agent");
        profile.scripts()->insert(SiteUserAgent::documentScript());
        ObservedUserAgent interceptor;
        profile.setUrlRequestInterceptor(&interceptor);
        QWebEnginePage page(&profile);
        QSignalSpy loaded(&page, &QWebEnginePage::loadFinished);
        page.setHtml("<script>window.earlyAgent=navigator.userAgent;</script>", url);
        QTRY_VERIFY_WITH_TIMEOUT(!loaded.isEmpty(), 10000);
        QVariant result; bool done = false;
        page.runJavaScript("JSON.stringify({ua:navigator.userAgent,early:window.earlyAgent,vendor:navigator.vendor,hints:typeof navigator.userAgentData})",
            [&](const QVariant &value) { result=value; done=true; });
        QTRY_VERIFY(done);
        const auto identity=QJsonDocument::fromJson(result.toString().toUtf8()).object();
        const auto expected=google ? QString::fromLatin1(SiteUserAgent::overrideFor(url)) : profile.httpUserAgent();
        QCOMPARE(identity["ua"].toString(), expected);
        QCOMPARE(identity["early"].toString(), expected);
        if (google) {
            QCOMPARE(identity["hints"].toString(), QString("undefined"));
            QCOMPARE(identity["vendor"].toString(), QString());
        }
        QCOMPARE(profile.httpUserAgent(), QString("Arbiter baseline test agent"));
    }
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
                QVERIFY(request[3].toByteArray().isEmpty());
                googleSeen = true;
            } else if (url.host() == "google.com.evil.example") {
                QCOMPARE(request[2].toByteArray(), request[1].toByteArray());
                QCOMPARE(request[3].toByteArray(), QByteArray("\"Chromium\";v=\"140\""));
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
