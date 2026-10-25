#include <QtTest>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QtWebEngineQuick>

class ExtensionPanelTest : public QObject {
    Q_OBJECT
private slots:
    void pageInteractionKeepsExtensionOpen() {
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
        function optionsUrl(id) { return "about:blank" }
    }
    ExtensionPanel { id: panel; controller: catalog; browserProfile: page.profile }
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
    }
};
int main(int argc, char **argv) {
    QtWebEngineQuick::initialize();
    QGuiApplication app(argc, argv);
    ExtensionPanelTest test;
    return QTest::qExec(&test, argc, argv);
}
#include "extensionpanel_test.moc"
