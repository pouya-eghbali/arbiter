#include "../src/siteuseragent.h"
#include <QtTest>

class SiteUserAgentTest : public QObject {
    Q_OBJECT
private slots:
    void domainScope_data() {
        QTest::addColumn<QString>("address");
        QTest::addColumn<bool>("overridden");
        QTest::newRow("root") << QStringLiteral("https://google.com/") << true;
        QTest::newRow("accounts") << QStringLiteral("https://accounts.google.com/signin") << true;
        QTest::newRow("mail") << QStringLiteral("https://mail.google.com/mail/") << true;
        QTest::newRow("nested") << QStringLiteral("https://a.b.google.com/") << true;
        QTest::newRow("http") << QStringLiteral("http://www.google.com/") << true;
        QTest::newRow("case-port-dot") << QStringLiteral("https://ACCOUNTS.GOOGLE.COM.:8443/") << true;
        QTest::newRow("suffix-spoof") << QStringLiteral("https://accounts.google.com.evil.example/") << false;
        QTest::newRow("prefix-spoof") << QStringLiteral("https://notgoogle.com/") << false;
        QTest::newRow("userinfo-spoof") << QStringLiteral("https://google.com@evil.example/") << false;
        QTest::newRow("query-spoof") << QStringLiteral("https://example.com/?next=https://accounts.google.com") << false;
        QTest::newRow("unrelated") << QStringLiteral("https://github.com/") << false;
        QTest::newRow("country-domain") << QStringLiteral("https://google.co.uk/") << false;
        QTest::newRow("file") << QStringLiteral("file://google.com/path") << false;
        QTest::newRow("extension") << QStringLiteral("chrome-extension://google.com/page.html") << false;
        QTest::newRow("invalid-port") << QStringLiteral("https://google.com:99999/") << false;
    }
    void domainScope() {
        QFETCH(QString, address); QFETCH(bool, overridden);
        const auto agent = SiteUserAgent::overrideFor(QUrl(address));
        QCOMPARE(!agent.isEmpty(), overridden);
        if (overridden) {
            QVERIFY(agent.contains("Firefox/159.0"));
            QVERIFY(agent.contains("Gecko/20100101"));
            QVERIFY(!agent.contains("Chrome/"));
            QVERIFY(!agent.contains("QtWebEngine/"));
        }
    }
};
QTEST_GUILESS_MAIN(SiteUserAgentTest)
#include "siteuseragent_test.moc"
