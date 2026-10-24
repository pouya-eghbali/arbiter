#include "../src/privatebrowsing.h"
#include "../src/settings.h"
#include "../src/workspaces.h"
#include "../src/history.h"
#include <QApplication>
#include <QTemporaryDir>
#include <QDir>
#include <QtTest>
#ifdef ARBITER_TEST_PRIVATE_PROFILES
#include "../src/webprofiles.h"
#include <QWebEngineCookieStore>
#include <QNetworkCookie>
#include <QQmlEngine>
#include <QQmlComponent>
#include <QtWebEngineQuick/qtwebenginequickglobal.h>
#endif

class TestAuth final : public BiometricAuth {
public:
    TestAuth() { refresh(); }
    QString name() const override { return "Test biometric"; }
    bool foregroundRequired() const override { return false; }
    void refresh() override { availability(true,"Ready"); }
    void succeed() { complete(lastToken,true); }
    void suspend() { emit sessionLocked(); }
protected:
    void startNative(const QString &,quint64 token) override { lastToken=token; }
    void cancelNative() override {}
private:
    quint64 lastToken=0;
};
class PrivateBrowsingTest : public QObject {
    Q_OBJECT
    QTemporaryDir config;
    static void activate(QWindow &window) { window.show(); window.requestActivate(); QTRY_VERIFY(window.isActive()); }
    static void setup(PrivateSession &session) {
        session.setup("private-test-password","private-test-password");
        QTRY_VERIFY_WITH_TIMEOUT(!session.busy(),10000);
        QVERIFY(session.configured()); QVERIFY(!session.locked());
    }
private slots:
    void initTestCase() {
        qputenv("XDG_CONFIG_HOME",config.path().toUtf8());
        qputenv("XDG_CONFIG_DIRS",config.path().toUtf8());
    }
    void init() { Settings().clear(); }
    void timeoutPersistsAndClamps() {
        PrivateBrowsing settings; QCOMPARE(settings.timeoutSeconds(),300);
        settings.setTimeoutSeconds(120); PrivateBrowsing restored; QCOMPARE(restored.timeoutSeconds(),120);
        settings.setTimeoutSeconds(0); QCOMPARE(settings.timeoutSeconds(),1);
        settings.setTimeoutSeconds(999999); QCOMPARE(settings.timeoutSeconds(),86400);
    }
    void idleBackgroundPasswordAndBiometric() {
        PrivateBrowsing settings; TestAuth auth; PrivateSession session(&settings,&auth); QWindow window,other;
        session.setWindow(&window); activate(window);
        QVERIFY(session.locked());
        session.setup("short","short"); QVERIFY(!session.configured());
        setup(session);
        settings.setTimeoutSeconds(1);
        QTRY_VERIFY_WITH_TIMEOUT(session.locked(),2000);
        session.unlockPassword("wrong"); QTRY_VERIFY(!session.busy()); QVERIFY(session.locked());
        QTest::qWait(2001);
        session.unlockPassword("private-test-password"); QTRY_VERIFY_WITH_TIMEOUT(!session.busy(),10000); QVERIFY(!session.locked());
        settings.setTimeoutSeconds(300);
        activate(other); QVERIFY(session.locked());
        session.unlockPassword("private-test-password"); QVERIFY(session.locked());
        activate(window); session.unlockBiometric(); QVERIFY(session.busy()); auth.succeed();
        QTRY_VERIFY(!session.busy()); QVERIFY(!session.locked());
        window.showMinimized(); QTRY_VERIFY(session.locked());
    }
    void suspendInvalidatesPendingUnlockAndNeverAutoReveals() {
        PrivateBrowsing settings; TestAuth auth; PrivateSession session(&settings,&auth); QWindow window,other;
        session.setWindow(&window); activate(window); setup(session);
        session.lock(); session.unlockBiometric(); QVERIFY(session.busy());
        auth.suspend(); QVERIFY(session.locked()); QVERIFY(!session.busy());
        auth.succeed(); QTest::qWait(200); QVERIFY(session.locked());
        session.unlockBiometric(); activate(other); auth.succeed();
        QTRY_VERIFY(!session.busy()); QVERIFY(session.locked());
        activate(window); QVERIFY(session.locked());
        session.unlockBiometric(); auth.succeed(); QTRY_VERIFY(!session.busy()); QVERIFY(!session.locked());
        // A KDF completion from before sleep must not unlock after waking.
        session.lock(); session.unlockPassword("private-test-password"); QVERIFY(session.busy()); auth.suspend();
        QTest::qWait(600); QVERIFY(session.locked()); QVERIFY(!session.busy());
    }
    void inputRestartsOnlyTheFocusedWindowsIdleDeadline() {
        PrivateBrowsing settings; settings.setTimeoutSeconds(1); TestAuth auth;
        PrivateSession session(&settings,&auth); QWindow window;
        session.setWindow(&window); activate(window); setup(session);
        QTest::qWait(650); QTest::keyClick(&window,Qt::Key_A);
        QTest::qWait(650); QVERIFY(!session.locked());
        QTRY_VERIFY_WITH_TIMEOUT(session.locked(),1500);
    }
#ifdef ARBITER_TEST_PRIVATE_PROFILES
    void chromiumProfilesAreEphemeralAndIsolated() {
        WebProfiles first,second;
        first.privateMode=true; second.privateMode=true;
        auto *a=first.profile("Default"); auto *b=second.profile("Default");
        QVERIFY(a!=b); QCOMPARE(first.profile("Default"),a);
        QVERIFY(a->isOffTheRecord()); QVERIFY(b->isOffTheRecord());
        QVERIFY(a->storageName().isEmpty());
        QCOMPARE(a->httpCacheType(),QQuickWebEngineProfile::MemoryHttpCache);
        QCOMPARE(a->persistentCookiesPolicy(),QQuickWebEngineProfile::NoPersistentCookies);
        QCOMPARE(a->persistentPermissionsPolicy(),QQuickWebEngineProfile::PersistentPermissionsPolicy::StoreInMemory);
        QQmlEngine engine;
#ifdef ARBITER_WEBENGINE_QML_PATH
        engine.addImportPath(QString::fromUtf8(ARBITER_WEBENGINE_QML_PATH));
#endif
        QQmlComponent component(&engine);
        component.setData("import QtQuick; import QtWebEngine; WebEngineView { width: 320; height: 200; url: 'data:text/html,<title>Private test</title>' }",QUrl());
        std::unique_ptr<QObject> firstPage(component.createWithInitialProperties({{"profile",QVariant::fromValue(a)}}));
        std::unique_ptr<QObject> secondPage(component.createWithInitialProperties({{"profile",QVariant::fromValue(b)}}));
        QVERIFY2(firstPage && secondPage,qPrintable(component.errorString()));
        QTRY_VERIFY_WITH_TIMEOUT(!firstPage->property("loading").toBool(),10000);
        QTRY_VERIFY_WITH_TIMEOUT(!secondPage->property("loading").toBool(),10000);
        QSignalSpy added(a->cookieStore(),&QWebEngineCookieStore::cookieAdded);
        QSignalSpy unrelated(b->cookieStore(),&QWebEngineCookieStore::cookieAdded);
        QNetworkCookie cookie("private-test","secret"); cookie.setPath("/"); cookie.setSecure(true);
        a->cookieStore()->setCookie(cookie,QUrl("https://private.test/"));
        a->cookieStore()->loadAllCookies();
        QTRY_VERIFY_WITH_TIMEOUT(!added.isEmpty(),5000);
        b->cookieStore()->loadAllCookies(); QTest::qWait(100); QVERIFY(unrelated.isEmpty());
        WebProfiles fresh; fresh.privateMode=true; auto *c=fresh.profile("Default");
        std::unique_ptr<QObject> freshPage(component.createWithInitialProperties({{"profile",QVariant::fromValue(c)}}));
        QVERIFY(freshPage); QTRY_VERIFY_WITH_TIMEOUT(!freshPage->property("loading").toBool(),10000);
        QSignalSpy restored(c->cookieStore(),&QWebEngineCookieStore::cookieAdded);
        c->cookieStore()->loadAllCookies(); QTest::qWait(100); QVERIFY(restored.isEmpty());
    }
#endif
    void privateSessionsNeverWriteWorkspacesOrHistory() {
        QTemporaryDir directory; const auto before=QDir::currentPath(); QDir::setCurrent(directory.path());
        {
            Tabs tabs; Workspaces spaces(&tabs,QString());
            QSignalSpy changes(&spaces,&Workspaces::changed);
            QVERIFY(tabs.count()>0); tabs.add(QUrl("https://private.example/secret"));
            QVERIFY(!changes.isEmpty());
            QCOMPARE(spaces.choices().first().toMap().value("tabs").toInt(),2);
            spaces.create("Private",QColor("#aabbcc"),true); spaces.flush();
            History history(QString{}); QVERIFY(!history.enabled());
            history.setEnabled(true); QVERIFY(!history.enabled());
            QVERIFY(history.record(QUrl("https://private.example/secret"),"secret",true).isEmpty());
            QCOMPARE(history.totalCount(),0);
        }
        QCOMPARE(QDir(directory.path()).entryList(QDir::AllEntries|QDir::NoDotAndDotDot),QStringList());
        QDir::setCurrent(before);
        Tabs tabs; Workspaces next(&tabs,QString()); QCOMPARE(tabs.count(),1);
        QCOMPARE(tabs.data(tabs.index(0),Tabs::Url).toUrl(),QUrl("about:blank"));
    }
};
int main(int argc,char **argv) {
#ifdef ARBITER_TEST_PRIVATE_PROFILES
    QtWebEngineQuick::initialize();
#endif
    QApplication app(argc,argv);
    PrivateBrowsingTest test; return QTest::qExec(&test,argc,argv);
}
#include "privatebrowsing_test.moc"
