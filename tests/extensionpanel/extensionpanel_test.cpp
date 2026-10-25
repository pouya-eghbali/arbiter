#include <QtTest>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QQuickItem>
#include <functional>
#include <QtWebEngineQuick>

#include "extensionpanel_test.h"
void ExtensionPanelTest::extensionPillMode() {
    QQmlApplicationEngine engine;
    engine.loadData(R"QML(
import QtQuick
import QtQuick.Controls.Basic as Basic
import Arbiter
Basic.ApplicationWindow {
    id:owner;visible:true;width:800;height:220
    property string chosenId:""
    property bool chosenOptions:false
    property alias extensionsMode:pill.extensionsMode
    property alias privateMode:pill.privateMode
    property var items:[
        {id:"popup",name:"Popup extension",enabled:true,hasPopup:true,hasOptions:true},
        {id:"options",name:"Options extension",enabled:true,hasPopup:false,hasOptions:true},
        {id:"disabled",name:"Disabled extension",enabled:false,hasPopup:true,hasOptions:true}
    ]
    QtObject {id:fakeView;property bool canGoBack:false;property bool canGoForward:false;property bool loading:false;property string url:"https://example.com"}
    QtObject {id:fakeAppearance;property string navigationRevealMode:"pill";function displayAddress(url){return url}}
    NavigationPill {
        id:pill;anchors.centerIn:parent;revealed:true
        view:fakeView;appearance:fakeAppearance;workspaceName:"Personal";workspaceColor:"blue";tabCount:1
        installedExtensions:owner.items
        onExtensionRequested:function(id,options){owner.chosenId=id;owner.chosenOptions=options}
    }
    function manyExtensions(){
        const next=[]
        for(let i=0;i<30;i++)next.push({id:"extra"+i,name:"Extension "+i,enabled:true,hasPopup:true,hasOptions:false})
        items=next
    }
})QML", QUrl("qrc:/extension-pill-test.qml"));
    QVERIFY(!engine.rootObjects().isEmpty());auto *root=engine.rootObjects().first();
    auto *toggle=root->findChild<QObject *>("extensionsPillButton");QVERIFY(toggle);
    QTRY_VERIFY(toggle->property("visible").toBool());
    QVERIFY(QMetaObject::invokeMethod(toggle,"clicked"));
    QVERIFY(root->property("extensionsMode").toBool());
    QVERIFY(!toggle->property("visible").toBool());
    std::function<QQuickItem *(QQuickItem *,const QString &)> findItem = [&](QQuickItem *item,const QString &name)->QQuickItem * {
        if(item->objectName()==name)return item;
        for(auto *child:item->childItems())if(auto *found=findItem(child,name))return found;
        return nullptr;
    };
    auto *window=qobject_cast<QQuickWindow *>(root);QVERIFY(window);
    QQuickItem *popup=nullptr;
    QTRY_VERIFY((popup=findItem(window->contentItem(),"pillExtension-popup")));
    QTRY_VERIFY(popup->isVisible());
    QVERIFY(QMetaObject::invokeMethod(popup,"clicked"));
    QCOMPARE(root->property("chosenId").toString(),QString("popup"));
    QVERIFY(!root->property("chosenOptions").toBool());
    auto *options=findItem(window->contentItem(),"pillExtension-options");QVERIFY(options);
    QVERIFY(QMetaObject::invokeMethod(options,"clicked"));
    QVERIFY(root->property("chosenOptions").toBool());
    auto *disabled=findItem(window->contentItem(),"pillExtension-disabled");QVERIFY(disabled);
    QVERIFY(!disabled->property("enabled").toBool());
    QTRY_VERIFY(qAbs((popup->mapToScene(QPointF()).x()+disabled->mapToScene(QPointF(disabled->width(),0)).x())/2-window->width()/2.)<1);
    auto *fallback=findItem(popup,"extensionFallbackIcon");QVERIFY(fallback);
    QTRY_COMPARE(fallback->property("status").toInt(),1); // Image.Ready
    QTest::qWait(150);
    if(const auto path=qEnvironmentVariable("ARBITER_TEST_PILL_IMAGE");!path.isEmpty())
        QVERIFY(window->grabWindow().save(path));
    auto *back=root->findChild<QObject *>("extensionsBackButton");QVERIFY(back);
    QVERIFY(QMetaObject::invokeMethod(back,"clicked"));
    QVERIFY(!root->property("extensionsMode").toBool());
    QVERIFY(toggle->property("visible").toBool());
    QVERIFY(QMetaObject::invokeMethod(toggle,"clicked"));
    QVERIFY(QMetaObject::invokeMethod(root,"manyExtensions"));
    auto *strip=root->findChild<QObject *>("pillExtensionStrip");QVERIFY(strip);
    QTRY_VERIFY(strip->property("contentWidth").toReal()>strip->property("width").toReal());
    auto *stripItem=qobject_cast<QQuickItem *>(strip);QVERIFY(stripItem);
    const auto position=stripItem->mapToScene(QPointF(stripItem->width()/2,stripItem->height()/2));
    QWheelEvent wheel(position,window->mapToGlobal(position.toPoint()),QPoint(),QPoint(0,-120),Qt::NoButton,Qt::NoModifier,Qt::NoScrollPhase,false);
    QCoreApplication::sendEvent(window,&wheel);
    QTRY_VERIFY(strip->property("contentX").toReal()>0);
    QCOMPARE(stripItem->rotation(),0.);
    root->setProperty("privateMode",true);
    QVERIFY(!root->property("extensionsMode").toBool());
    QVERIFY(!toggle->property("visible").toBool());
}

void ExtensionPanelTest::cookieWarningAppearsBeforeApproval() {
        QQmlApplicationEngine engine;
        engine.loadData(R"QML(
import QtQuick
import QtQuick.Controls.Basic as Basic
import Arbiter
Basic.ApplicationWindow {
    id:owner;visible:true;width:800;height:700
    property bool approved:false
    QtObject {
        id:catalog
        property var review: ({id:"fixture",name:"Cookie fixture",cookieAccess:true,permissions:["cookies","https://allowed.example/"]})
        property string status:""
        signal changed()
        function cancelReview(){review=({});changed()}
        function approve(){owner.approved=true;review=({});changed()}
    }
    ExtensionReview { catalog:catalog }
})QML",QUrl("qrc:/extension-cookie-review-test.qml"));
        QVERIFY(!engine.rootObjects().isEmpty());auto *root=engine.rootObjects().first();
        auto *panel=root->findChild<QObject *>("extensionPermissionReview");QVERIFY(panel);
        QTRY_VERIFY(panel->property("opened").toBool());
        auto *warning=root->findChild<QObject *>("extensionCookieWarning");QVERIFY(warning);
        QVERIFY(warning->property("visible").toBool());
        QVERIFY(warning->property("text").toString().contains("session data"));
        QVERIFY(!root->property("approved").toBool());
    }
void ExtensionPanelTest::pageInteractionKeepsExtensionOpen() {
        QQmlApplicationEngine engine;
#ifdef ARBITER_WEBENGINE_QML_PATH
        engine.addImportPath(QString::fromUtf8(ARBITER_WEBENGINE_QML_PATH));
#endif
        engine.loadData(R"QML(
import QtQuick
import QtQuick.Controls.Basic as Basic
import QtWebEngine
import Arbiter
Basic.ApplicationWindow {
    id: window
    visible: true
    width: 1100; height: 760
    property string fieldValue: ""
    property string optionsTabUrl: ""
    property int invokedCount: 0
    property bool pageLoaded: false
    property alias panelOpen: panel.opened
    property alias panelX: panel.x
    property alias panelY: panel.y
    property alias panelWidth: panel.width
    property alias panelHeight: panel.height
    WebEngineView {
        id: page
        objectName: "page"
        anchors.fill: parent
        url: "data:text/html,<input id='field' style='position:absolute;left:20px;top:100px;width:100px;height:100px'>"
        onLoadingChanged: function(request) { if (request.status === WebEngineView.LoadSucceededStatus) window.pageLoaded = true }
    }
    QtObject {
        id: catalog
        property var items: []
        signal changed()
        function popupUrl(id) { return "about:blank" }
        function optionsUrl(id) { return "https://extension-options.example/settings.html" }
    }
    ExtensionPanel {
        id: panel; controller: catalog; browserProfile: page.profile
        onOptionsRequested: function(url) { window.optionsTabUrl = url }
        onInvoked: window.invokedCount++
    }
    function inspectPage() { page.mainFrame.runJavaScript("document.getElementById('field').value", 0, value => window.fieldValue = value) }
    function showExtension() { panel.showExtension("fixture", false) }
    function showOptions() { panel.showExtension("fixture", true) }
    function dismiss() { panel.close() }
    function findPopup(item) {
        if (item.objectName === "extensionPopupView") return item
        for (const child of item.children || []) { const found=findPopup(child); if(found) return found }
        return null
    }
    function resizePopup(width:int,height:int) {
        const view=findPopup(panel.contentItem)
        if(view) view.mainFrame.runJavaScript("document.open();document.write('<!doctype html><body style=\"margin:0;width:"+width+"px;height:"+height+"px\">Popup content</body>');document.close()",0,function(){})
    }
})QML", QUrl("qrc:/extension-panel-test.qml"));
        QVERIFY(!engine.rootObjects().isEmpty());
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
        QVERIFY(window);
        QVERIFY(QTest::qWaitForWindowExposed(window));
        QTRY_VERIFY(window->property("pageLoaded").toBool());
        QVERIFY(QMetaObject::invokeMethod(window, "showExtension"));
        QTRY_VERIFY(window->property("panelOpen").toBool());
        auto *popup = window->findChild<QObject *>("extensionPopupView");
        QTRY_VERIFY((popup = window->findChild<QObject *>("extensionPopupView")));
        QVERIFY(QMetaObject::invokeMethod(window, "resizePopup", Q_ARG(int,325), Q_ARG(int,240)));
        QTRY_COMPARE(window->property("panelWidth").toInt(), 341);
        QTRY_COMPARE(window->property("panelHeight").toInt(), 290);
        QVERIFY(QMetaObject::invokeMethod(window, "resizePopup", Q_ARG(int,325), Q_ARG(int,320)));
        QTRY_COMPARE(window->property("panelHeight").toInt(), 370);
        QVERIFY(QMetaObject::invokeMethod(window, "resizePopup", Q_ARG(int,325), Q_ARG(int,200)));
        QTRY_COMPARE(window->property("panelHeight").toInt(), 250);
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, QPoint(60, 140));
        QTest::keyClick(window, Qt::Key_A);
        QTRY_VERIFY((QMetaObject::invokeMethod(window, "inspectPage"), window->property("fieldValue").toString() == "a"));
        QVERIFY(window->property("panelOpen").toBool());
        QCOMPARE(window->findChild<QObject *>("extensionPopupView"), popup);
        // Pointer dragging the bottom handle moves the panel, not its document.
        const auto oldX = window->property("panelX").toInt();
        const auto oldY = window->property("panelY").toInt();
        const QPoint handle(oldX + 120, oldY + window->property("panelHeight").toInt() - 22);
        QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, handle);
        QTest::mouseMove(window, handle - QPoint(120, 30), 40);
        QTest::mouseMove(window, handle - QPoint(170, 60), 40);
        QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, handle - QPoint(170, 60));
        QVERIFY(window->property("panelX").toInt() < oldX);
        QVERIFY(window->property("panelOpen").toBool());
        window->resize(400, 400);
        QTRY_VERIFY(window->property("panelX").toReal() >= 0);
        QTRY_VERIFY(window->property("panelX").toReal() + window->property("panelWidth").toReal() <= window->width());
        const QPoint closeButton(window->property("panelX").toInt() + window->property("panelWidth").toInt() - 22,
                                 window->property("panelY").toInt() + window->property("panelHeight").toInt() - 22);
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, closeButton);
        QTRY_VERIFY(!window->property("panelOpen").toBool());
        QTRY_VERIFY(!window->findChild<QObject *>("extensionPopupView"));
        QVERIFY(QMetaObject::invokeMethod(window, "showExtension"));
        QTRY_VERIFY(window->property("panelOpen").toBool());
        const int invocations=window->property("invokedCount").toInt();
        QVERIFY(QMetaObject::invokeMethod(window, "showOptions"));
        QCOMPARE(window->property("optionsTabUrl").toString(),QString("https://extension-options.example/settings.html"));
        QCOMPARE(window->property("invokedCount").toInt(),invocations);
        QTRY_VERIFY(!window->property("panelOpen").toBool());
        QTRY_VERIFY(!window->findChild<QObject *>("extensionPopupView"));
    }
int main(int argc, char **argv) {
    QtWebEngineQuick::initialize();
    QGuiApplication app(argc, argv);
    ExtensionPanelTest test;
    return QTest::qExec(&test, argc, argv);
}
