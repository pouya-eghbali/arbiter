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
class PrivacyMemoryStore final : public SecretStore {
public:
    QMap<QString,QByteArray> values;
    QString readError,writeError;
    void read(const QString &key,Callback callback) override { callback(readError,values.value(key)); }
    void write(const QString &key,const QByteArray &data,Callback callback) override {
        if(writeError.isEmpty()) values[key]=data;
        callback(writeError,{});
    }
    void remove(const QString &key,Callback callback) override { values.remove(key); callback({},{}); }
};
class PrivateBrowsingTest : public QObject {
    Q_OBJECT
    QTemporaryDir config;
    PrivacyMemoryStore store;
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
    void init() { Settings().clear(); store.values.clear(); store.readError.clear(); store.writeError.clear(); }
    void timeoutPersistsAndClamps() {
        PrivateBrowsing settings(&store); settings.load(); QCOMPARE(settings.timeoutSeconds(),300);
        settings.setTimeoutSeconds(120); PrivateBrowsing restored(&store); restored.load(); QCOMPARE(restored.timeoutSeconds(),120);
        settings.setTimeoutSeconds(0); QCOMPARE(settings.timeoutSeconds(),1);
        settings.setTimeoutSeconds(999999); QCOMPARE(settings.timeoutSeconds(),86400);
    }
    void credentialIsGlobalAcrossWindowsAndRestarts() {
        {
            PrivateBrowsing settings(&store); settings.load();
            TestAuth firstAuth,secondAuth;
            PrivateSession first(&settings,&firstAuth),second(&settings,&secondAuth);
            QWindow a,b; first.setWindow(&a); second.setWindow(&b); activate(a); setup(first);
            QVERIFY(second.configured()); QVERIFY(second.locked());
            activate(b); second.unlockPassword("private-test-password");
            QTRY_VERIFY(!second.busy()); QVERIFY(!second.locked());
        }
        QVERIFY(!store.values.isEmpty());
        PrivateBrowsing restored(&store); restored.load(); QVERIFY(restored.configured());
        TestAuth auth; PrivateSession fresh(&restored,&auth); QWindow window; fresh.setWindow(&window); activate(window);
        QVERIFY(fresh.configured()); QVERIFY(fresh.locked());
        fresh.unlockPassword("private-test-password"); QTRY_VERIFY(!fresh.busy()); QVERIFY(!fresh.locked());
        QVERIFY(!Settings().contains("Privacy/password"));
        for(const auto &data:store.values) QVERIFY(!data.contains("private-test-password"));
    }
    void changingRequiresOldCredentialPersistsAndLocksEveryWindow() {
        PrivateBrowsing settings(&store); settings.load(); TestAuth authA,authB;
        PrivateSession a(&settings,&authA),b(&settings,&authB); QWindow first,second;
        a.setWindow(&first); b.setWindow(&second); activate(first); setup(a);
        QSignalSpy changed(&settings,&PrivateBrowsing::credentialChanged);
        const auto original=store.values;
        settings.setPassword("wrong","123456","123456"); QTRY_VERIFY(!settings.busy());
        QCOMPARE(store.values,original); QVERIFY(changed.isEmpty()); QVERIFY(!a.locked());
        QTest::qWait(2001);
        a.lock(); a.unlockBiometric(); QVERIFY(a.busy());
        settings.setPassword("private-test-password","123456","123456"); QTRY_VERIFY(!settings.busy());
        QCOMPARE(changed.size(),1); QVERIFY(a.locked()); QVERIFY(b.locked());
        authA.succeed(); QTest::qWait(200); QVERIFY(a.locked()); QVERIFY(!a.busy());
        QVERIFY(store.values!=original);
        PrivateBrowsing restored(&store); restored.load();
        bool ok=true; restored.verify("private-test-password",[&](bool value,const QString &) { ok=value; });
        QTRY_VERIFY(!restored.busy()); QVERIFY(!ok);
        QTest::qWait(2001);
        restored.verify("123456",[&](bool value,const QString &) { ok=value; });
        QTRY_VERIFY(!restored.busy()); QVERIFY(ok);
    }
    void unavailableCorruptOrUnwritableStoreNeverResetsCredential() {
        store.readError="Store locked"; PrivateBrowsing settings(&store); settings.load();
        QVERIFY(!settings.ready()); QVERIFY(!settings.configured());
        settings.setPassword({},"123456","123456"); QVERIFY(store.values.isEmpty());
        store.readError.clear(); settings.load(); QVERIFY(settings.ready());
        store.writeError="Store write denied";
        settings.setPassword({},"123456","123456"); QTRY_VERIFY(!settings.busy());
        QVERIFY(!settings.configured()); QVERIFY(store.values.isEmpty());
        store.writeError.clear(); settings.setPassword({},"123456","123456"); QTRY_VERIFY(!settings.busy()); QVERIFY(settings.configured());
        const auto original=store.values;
        store.writeError="Store write denied";
        settings.setPassword("123456","changed-password","changed-password"); QTRY_VERIFY(!settings.busy()); QCOMPARE(store.values,original);
        for(auto &data:store.values) data="corrupt";
        PrivateBrowsing corrupted(&store); corrupted.load(); QVERIFY(!corrupted.ready()); QVERIFY(!corrupted.configured());
        corrupted.setPassword({},"123456","123456");
        for(const auto &data:store.values) QCOMPARE(data,QByteArray("corrupt"));
    }
    void globalRetryLimitCannotBeBypassedWithAnotherWindow() {
        PrivateBrowsing settings(&store); settings.load(); TestAuth authA,authB; PrivateSession a(&settings,&authA),b(&settings,&authB);
        QWindow first,second; a.setWindow(&first); b.setWindow(&second); activate(first); setup(a);
        a.lock(); a.unlockPassword("wrong"); QTRY_VERIFY(!a.busy()); QVERIFY(a.locked());
        activate(second); b.unlockPassword("private-test-password"); QVERIFY(!b.busy()); QVERIFY(b.locked());
        QTest::qWait(2001); b.unlockPassword("private-test-password"); QTRY_VERIFY(!b.busy()); QVERIFY(!b.locked());
    }
    void idleBackgroundPasswordAndBiometric() {
        PrivateBrowsing settings(&store); settings.load(); TestAuth auth; PrivateSession session(&settings,&auth); QWindow window,other;
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
        PrivateBrowsing settings(&store); settings.load(); TestAuth auth; PrivateSession session(&settings,&auth); QWindow window,other;
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
        PrivateBrowsing settings(&store); settings.load(); settings.setTimeoutSeconds(1); TestAuth auth;
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
        auto *scripts=a->property("userScripts").value<QObject *>();
        QVERIFY(scripts);
        QList<QWebEngineScript> compatibilityScripts;
        QVERIFY(QMetaObject::invokeMethod(scripts,"find",
            Q_RETURN_ARG(QList<QWebEngineScript>,compatibilityScripts),
            Q_ARG(QString,QStringLiteral("Arbiter Google browser identity"))));
        QCOMPARE(compatibilityScripts.size(),1);
        QCOMPARE(compatibilityScripts.first(),SiteUserAgent::documentScript());
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
