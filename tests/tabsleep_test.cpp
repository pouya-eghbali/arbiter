#include "../src/tabsleepsettings.h"
#include "../src/settings.h"
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QTemporaryDir>
#include <QtTest>
#include <QtWebEngineQuick/qtwebenginequickglobal.h>

class TabSleepTest : public QObject {
    Q_OBJECT
    QTemporaryDir config;
    QQmlEngine engine;
    std::unique_ptr<QObject> policy() {
        QQmlComponent component(&engine);
        component.setData(R"QML(
import QtQuick
import QtWebEngine
import "../qml" as Browser
QtObject {
    property QtObject page: QtObject {
        property bool visible: false
        property bool recentlyAudible: false
        property bool loading: false
        property int recommendedState: WebEngineView.LifecycleState.Frozen
        property int lifecycleState: WebEngineView.LifecycleState.Active
    }
    property QtObject sleeper: Browser.TabSleepController {
        view: page
        timeoutSeconds: 1
    }
}
)QML",QUrl::fromLocalFile(QStringLiteral(ARBITER_TEST_SOURCE_DIR "/tabsleep_fixture.qml")));
        auto result=std::unique_ptr<QObject>(component.create());
        if(!result) qWarning()<<component.errors();
        return result;
    }
private slots:
    void initTestCase() {
        qputenv("XDG_CONFIG_HOME",config.path().toUtf8());
        qputenv("XDG_CONFIG_DIRS",config.path().toUtf8());
    }
    void init() { Settings().clear(); }
    void settingsPersistAndClamp() {
        TabSleepSettings settings; QCOMPARE(settings.timeoutSeconds(),900);
        settings.setTimeoutSeconds(300);
        TabSleepSettings restored; QCOMPARE(restored.timeoutSeconds(),300);
        settings.setTimeoutSeconds(-10); QCOMPARE(settings.timeoutSeconds(),0);
        settings.setTimeoutSeconds(100000); QCOMPARE(settings.timeoutSeconds(),86400);
    }
    void freezesAndResumesAndProtectsSplitPanes() {
        auto root=policy(); QVERIFY(root);
        auto *page=root->property("page").value<QObject *>();
        auto *sleep=root->property("sleeper").value<QObject *>();
        QTRY_COMPARE_WITH_TIMEOUT(page->property("lifecycleState").toInt(),1,2000);
        page->setProperty("visible",true);
        QCOMPARE(page->property("lifecycleState").toInt(),0);
        sleep->setProperty("keepAwake",true); page->setProperty("visible",false);
        QTest::qWait(1100); QCOMPARE(page->property("lifecycleState").toInt(),0);
        sleep->setProperty("keepAwake",false);
        QTRY_COMPARE_WITH_TIMEOUT(page->property("lifecycleState").toInt(),1,2000);
    }
    void audioAndEngineProtectionRestartTheTimer() {
        auto root=policy(); QVERIFY(root);
        auto *page=root->property("page").value<QObject *>();
        QTest::qWait(600); page->setProperty("recentlyAudible",true);
        QTest::qWait(600); QCOMPARE(page->property("lifecycleState").toInt(),0);
        page->setProperty("recentlyAudible",false);
        QTest::qWait(600); QCOMPARE(page->property("lifecycleState").toInt(),0);
        QTRY_COMPARE_WITH_TIMEOUT(page->property("lifecycleState").toInt(),1,1500);
        page->setProperty("recommendedState",0);
        QCOMPARE(page->property("lifecycleState").toInt(),0);
        QTest::qWait(1100); QCOMPARE(page->property("lifecycleState").toInt(),0);
        page->setProperty("recommendedState",1); page->setProperty("loading",true);
        QTest::qWait(1100); QCOMPARE(page->property("lifecycleState").toInt(),0);
    }
    void disablingOrChangingTimeoutWakesSleepingPages() {
        auto root=policy(); QVERIFY(root);
        auto *page=root->property("page").value<QObject *>();
        auto *sleep=root->property("sleeper").value<QObject *>();
        QTRY_COMPARE_WITH_TIMEOUT(page->property("lifecycleState").toInt(),1,2000);
        sleep->setProperty("timeoutSeconds",0);
        QCOMPARE(page->property("lifecycleState").toInt(),0);
        QTest::qWait(1100); QCOMPARE(page->property("lifecycleState").toInt(),0);
        sleep->setProperty("timeoutSeconds",1);
        QTRY_COMPARE_WITH_TIMEOUT(page->property("lifecycleState").toInt(),1,2000);
        sleep->setProperty("timeoutSeconds",2);
        QCOMPARE(page->property("lifecycleState").toInt(),0);
        QTest::qWait(1100); QCOMPARE(page->property("lifecycleState").toInt(),0);
    }
};
int main(int argc,char **argv) {
    QtWebEngineQuick::initialize();
    QGuiApplication app(argc,argv);
    TabSleepTest test; return QTest::qExec(&test,argc,argv);
}
#include "tabsleep_test.moc"
