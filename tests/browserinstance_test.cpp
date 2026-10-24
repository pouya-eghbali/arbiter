#include <QtTest>
#include <QtConcurrent>
#include <QTemporaryDir>
#include "../src/browserinstance.h"
class BrowserInstanceTest : public QObject {
    Q_OBJECT
private slots:
    void forwardsOnlyWebLinksAndReleasesProfile() {
        QTemporaryDir profile; QVERIFY(profile.isValid());
        {
            BrowserInstance owner(profile.path());
            QCOMPARE(owner.start({}),BrowserInstance::Primary);
            QSignalSpy links(&owner,&BrowserInstance::urlReceived),activation(&owner,&BrowserInstance::activationRequested);
            auto forwarded=QtConcurrent::run([&] {
                BrowserInstance next(profile.path());
                return next.start({"https://example.test/hello?q=a%20b","file:///private/test","javascript:alert(1)","--flag"});
            });
            QTRY_VERIFY_WITH_TIMEOUT(forwarded.isFinished(),5000);
            QCOMPARE(forwarded.result(),BrowserInstance::Forwarded);
            QCOMPARE(links.size(),1); QCOMPARE(links.first().first().toUrl(),QUrl("https://example.test/hello?q=a%20b"));
            QCOMPARE(activation.size(),1);
            auto activateOnly=QtConcurrent::run([&] { BrowserInstance next(profile.path()); return next.start({}); });
            QTRY_VERIFY_WITH_TIMEOUT(activateOnly.isFinished(),5000);
            QCOMPARE(activateOnly.result(),BrowserInstance::Forwarded); QCOMPARE(links.size(),1); QCOMPARE(activation.size(),2);
        }
        BrowserInstance reopened(profile.path()); QCOMPARE(reopened.start({}),BrowserInstance::Primary);
    }
};
QTEST_GUILESS_MAIN(BrowserInstanceTest)
#include "browserinstance_test.moc"
