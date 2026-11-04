#include "../../src/keybindings.h"
#include "../../src/settings.h"
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtWebEngineQuick/qtwebenginequickglobal.h>
#include <QtTest>

class DesktopWindowTest : public QObject {
    Q_OBJECT
private slots:
    void frameAndSettingsWhilePageFocused() {
        QTemporaryDir config;
        qputenv("XDG_CONFIG_HOME",config.path().toUtf8());
        qputenv("XDG_CONFIG_DIRS",config.path().toUtf8()); Settings().clear();
        KeyBindings keys;
        QQmlApplicationEngine engine;
        engine.setInitialProperties({{"keys",QVariant::fromValue(&keys)}});
        engine.loadData(R"QML(
import QtQuick
import QtQuick.Controls.Basic as Basic
import QtWebEngine
import "qrc:/desktop-window-test/Arbiter" as WindowComponents
import "qrc:/desktop-window-test/Arbiter/DesktopWindowFrame.js" as WindowFrame
Basic.ApplicationWindow {
    id: root; width: 640; height: 480; visible: true
    required property var keys
    property bool decorated: false
    property bool loaded: false
    property bool blocked: false
    property string shortcutPlatform: "linux"
    property int activated: 0
    flags: WindowFrame.flags(decorated, "linux")
    WindowComponents.DesktopSettingsShortcut {
        binding: root.keys.bindings.settings
        platformName: root.shortcutPlatform
        blocked: root.blocked
        onActivated: root.activated++
    }
    WebEngineView {
        id: page; objectName: "page"; anchors.fill: parent
        profile: WebEngineProfile { offTheRecord: true }
        Component.onCompleted: loadHtml("<title>Shortcut test</title><input placeholder='Page input'>")
        onLoadingChanged: function(request) {
            if (request.status === WebEngineView.LoadSucceededStatus) { root.loaded = true; forceActiveFocus() }
        }
    }
    WindowComponents.FramelessResizeEdges { anchors.fill: parent; targetWindow: root; z: 1000 }
})QML",QUrl("qrc:/desktop-window-fixture.qml"));
        QVERIFY(!engine.rootObjects().isEmpty());
        auto *window=qobject_cast<QQuickWindow *>(engine.rootObjects().first()); QVERIFY(window);
        QVERIFY(window->flags().testFlag(Qt::FramelessWindowHint));
        QVERIFY(QTest::qWaitForWindowExposed(window));
        window->requestActivate(); QTRY_VERIFY(window->isActive());
        QTRY_VERIFY(window->property("loaded").toBool());
        auto *page=window->findChild<QQuickItem *>("page"); QVERIFY(page);
        page->forceActiveFocus(); QTRY_VERIFY(page->hasActiveFocus());
        QTest::keyClick(window,Qt::Key_Comma,Qt::ControlModifier);
        QTRY_COMPARE(window->property("activated").toInt(),1);
        window->setProperty("blocked",true);
        QTest::keyClick(window,Qt::Key_Comma,Qt::ControlModifier); QTest::qWait(30);
        QCOMPARE(window->property("activated").toInt(),1);
        window->setProperty("blocked",false);
        window->setProperty("shortcutPlatform","osx");
        QTest::keyClick(window,Qt::Key_Comma,Qt::ControlModifier); QTest::qWait(30);
        QCOMPARE(window->property("activated").toInt(),1);
        window->setProperty("shortcutPlatform","linux");
        int settings=-1;
        for(int i=0;i<keys.rowCount();++i) if(keys.data(keys.index(i),Qt::UserRole)=="settings") settings=i;
        QVERIFY(settings>=0); QVERIFY(keys.assign(settings,Qt::Key_K,Qt::ControlModifier|Qt::AltModifier).isEmpty());
        QTest::keyClick(window,Qt::Key_K,Qt::ControlModifier|Qt::AltModifier);
        QTRY_COMPARE(window->property("activated").toInt(),2);
        window->setProperty("decorated",true);
        QTRY_VERIFY(!window->flags().testFlag(Qt::FramelessWindowHint));
    }
};
int main(int argc,char **argv) {
    QtWebEngineQuick::initialize(); QGuiApplication app(argc,argv);
    QStandardPaths::setTestModeEnabled(true);
    DesktopWindowTest test; return QTest::qExec(&test,argc,argv);
}
#include "desktopwindow_test.moc"
