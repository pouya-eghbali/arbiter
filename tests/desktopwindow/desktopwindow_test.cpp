#include "desktopwindow_test.h"
#include "../../src/keybindings.h"
#include "../../src/settings.h"
#include "../../src/linuxwindowframe.h"
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtWebEngineQuick/qtwebenginequickglobal.h>
#include <QtTest>

void DesktopWindowTest::roundedFrameGeometry() {
        if (QGuiApplication::platformName() != "wayland" && QGuiApplication::platformName() != "xcb")
            QSKIP("Native Linux frame test needs Wayland or X11");
        QQmlApplicationEngine engine;
        engine.loadData(R"QML(
import QtQuick
import QtQuick.Controls.Basic as Basic
import QtQuick.Effects
import Arbiter.Windowing
import "qrc:/desktop-window-test/Arbiter" as WindowComponents
Basic.ApplicationWindow {
    id: root; width: 640; height: 480; visible: true
    flags: Qt.Window | Qt.FramelessWindowHint
    property bool framed: visibility === Window.Windowed
    readonly property int inset: framed ? 24 : 0
    color: framed ? "transparent" : "#19222d"
    topPadding: inset; bottomPadding: inset; leftPadding: inset; rightPadding: inset
    LinuxWindowFrame { targetWindow: root; shadowMargin: root.inset }
    background: WindowComponents.DesktopWindowSurface { framed: root.framed; windowActive: true; color: "#19222d" }
    contentItem.layer.enabled: framed
    contentItem.layer.effect: MultiEffect { maskEnabled: true; maskSource: cornerMask }
    Rectangle { id: cornerMask; anchors.fill: parent; color: "white"; radius: 12; visible: false; layer.enabled: true }
    Rectangle { anchors.fill: parent; color: "#e7e8eb" }
    WindowComponents.DesktopWindowOverlayClip { targetWindow: root; framed: root.framed }
    Basic.Popup {
        objectName: "testPopup"
        width: 200; height: 120; modal: true; dim: true
        parent: Basic.Overlay.overlay
        anchors.centerIn: parent
        Basic.Overlay.modal: Rectangle { color: "#88000000" }
    }
})QML", QUrl("qrc:/frame-fixture.qml"));
        QVERIFY(!engine.rootObjects().isEmpty());
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first()); QVERIFY(window);
        QVERIFY(QTest::qWaitForWindowExposed(window));
        QTest::qWait(100);
        QVERIFY(window->mask().isEmpty() || !window->mask().contains({0,0}));
        if (qgetenv("QT_QUICK_BACKEND") != "software") {
            const QImage image = window->grabWindow();
            QVERIFY(!image.isNull());
            QCOMPARE(image.pixelColor(0,0).alpha(),0);
            const qreal scale = image.width() / qreal(window->width());
            const int y = image.height() / 2;
            const int shadowAlpha = image.pixelColor(qRound(16 * scale),y).alpha();
            QVERIFY2(shadowAlpha > 0 && shadowAlpha < 255,"Shadow must be visible outside the content, with transparency");
            QVERIFY(image.pixelColor(qRound(24 * scale),qRound(24 * scale)).alpha() < 255);
            QCOMPARE(image.pixelColor(image.width()/2,y).alpha(),255);
            if (!qEnvironmentVariableIsEmpty("ARBITER_FRAME_SCREENSHOT"))
                QVERIFY(image.save(qEnvironmentVariable("ARBITER_FRAME_SCREENSHOT")));
            auto *popup = window->findChild<QObject *>("testPopup"); QVERIFY(popup);
            QVERIFY(QMetaObject::invokeMethod(popup, "open"));
            QTest::qWait(150);
            const QImage popupImage = window->grabWindow();
            QCOMPARE(popupImage.pixelColor(0,0).alpha(),0);
            // A modal backdrop must not turn transparent shadow margins opaque.
            QVERIFY(popupImage.pixelColor(qRound(16 * scale),y).alpha() < 255);
            QVERIFY(QMetaObject::invokeMethod(popup, "close"));
            QTest::qWait(150);
        }
        const QSize floatingSize = window->size();
        window->showMaximized();
        QTRY_COMPARE(window->visibility(), QWindow::Maximized);
        QTRY_VERIFY(!window->property("framed").toBool());
        QVERIFY(window->mask().isEmpty());
        window->showNormal();
        QTRY_COMPARE(window->visibility(), QWindow::Windowed);
        QTRY_VERIFY(window->property("framed").toBool());
        QTRY_COMPARE(window->size(), floatingSize);
        window->setProperty("framed",false);
        QTest::qWait(50);
        QVERIFY(window->mask().isEmpty());
        window->setProperty("framed",true);
        QTest::qWait(50);
        QVERIFY(window->mask().isEmpty() || !window->mask().contains({0,0}));
    }
void DesktopWindowTest::frameAndSettingsWhilePageFocused() {
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
void DesktopWindowTest::webFullscreen() {
    QQmlApplicationEngine engine;
    engine.loadData(R"QML(
import QtQuick
import QtQuick.Controls.Basic as Basic
import QtWebEngine
import Arbiter
Basic.ApplicationWindow {
    id: root; width: 800; height: 600; visible: true
    property bool loaded: false
    property bool allowFullscreen: true
    property alias contentFullscreen: fullscreen.active
    property alias engineFullscreen: page.isFullScreen
    property alias pageWidth: page.width
    property alias pageHeight: page.height
    property alias pageX: page.x
    property alias pageY: page.y
    property bool reparented: page.parent === fullscreen
    function leaveFromPage() { page.runJavaScript("document.exitFullscreen()") }
    function navigateAway() { page.url = "about:blank" }
    Rectangle { id: split; anchors.fill: parent; color: "#202530" }
    WebEngineView {
        id: page; objectName: "fullscreenPage"; parent: split
        x: 90; y: 80; width: 320; height: 240
        profile: WebEngineProfile { offTheRecord: true }
        settings.fullScreenSupportEnabled: true
        onFullScreenRequested: function(request) {
            fullscreen.request(page, request, root.allowFullscreen)
        }
        onJavaScriptConsoleMessage: function(level, message) { console.log("video fixture", message) }
        onLoadingChanged: request => {
            if (request.status === WebEngineView.LoadSucceededStatus) root.loaded = true
        }
        Component.onCompleted: loadHtml("<!doctype html><html><body style='margin:0'><button style='position:fixed;left:0;top:0;width:150px;height:60px;z-index:9' onclick='document.querySelector(\"video\").requestFullscreen().catch(e=>console.error(e.name+e.message))'>Fullscreen video</button><video muted autoplay playsinline></video><script>const c=document.createElement('canvas');c.width=320;c.height=180;c.getContext('2d').fillRect(0,0,320,180);document.querySelector('video').srcObject=c.captureStream(10);</script></body></html>")
    }
    WebFullScreen { id: fullscreen }
})QML", QUrl("qrc:/fullscreen-fixture.qml"));
    QVERIFY(!engine.rootObjects().isEmpty());
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first()); QVERIFY(window);
    QVERIFY(QTest::qWaitForWindowExposed(window));
    window->requestActivate(); QTRY_VERIFY(window->isActive());
    QTRY_VERIFY(window->property("loaded").toBool());
    auto *page = window->findChild<QQuickItem *>("fullscreenPage"); QVERIFY(page);
    const auto enter = [&] {
        page->forceActiveFocus();
        const QPoint point = page->mapToScene(QPointF(60,30)).toPoint();
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, point);
    };
    const auto restored = [&] {
        return !window->property("contentFullscreen").toBool()
            && !window->property("engineFullscreen").toBool()
            && !window->property("reparented").toBool()
            && page->x() == 90 && page->y() == 80
            && page->width() == 320 && page->height() == 240;
    };
    enter();
    QTRY_VERIFY(window->property("contentFullscreen").toBool());
    QTRY_VERIFY(window->property("engineFullscreen").toBool());
    QTRY_COMPARE(window->visibility(), QWindow::FullScreen);
    QTRY_VERIFY(window->property("reparented").toBool());
    QTRY_COMPARE(page->width(), window->contentItem()->width());
    QTRY_COMPARE(page->height(), window->contentItem()->height());
    QTest::keyClick(window, Qt::Key_Escape);
    QTRY_VERIFY(restored());
    QTRY_COMPARE(window->visibility(), QWindow::Windowed);

    // Page-owned exit also restores split bindings, including a previously
    // fullscreen browser window.
    window->showFullScreen();
    QTRY_COMPARE(window->visibility(), QWindow::FullScreen);
    QTest::qWait(1200); // Native Space transition must finish before injecting input.
    enter();
    QTRY_VERIFY(window->property("contentFullscreen").toBool());
    QVERIFY(QMetaObject::invokeMethod(window, "leaveFromPage"));
    QTRY_VERIFY(restored());
    QCOMPARE(window->visibility(), QWindow::FullScreen);
    window->showNormal();
    QTRY_COMPARE(window->visibility(), QWindow::Windowed);
    QTest::qWait(1200);

    window->setProperty("allowFullscreen", false);
    enter();
    QTest::qWait(300);
    QVERIFY(restored());
    window->setProperty("allowFullscreen", true);
    enter();
    QTRY_VERIFY(window->property("contentFullscreen").toBool());
    QVERIFY(QMetaObject::invokeMethod(window, "navigateAway"));
    QTRY_VERIFY(restored());
    QTRY_COMPARE(window->visibility(), QWindow::Windowed);
}

int main(int argc,char **argv) {
    QtWebEngineQuick::initialize(); QGuiApplication app(argc,argv);
    QStandardPaths::setTestModeEnabled(true);
    qmlRegisterType<LinuxWindowFrame>("Arbiter.Windowing",1,0,"LinuxWindowFrame");
    DesktopWindowTest test; return QTest::qExec(&test,argc,argv);
}
