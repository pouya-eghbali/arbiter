#include "devtools_test.h"
#include <QtTest>
#include <QtWebEngineQuick>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlExpression>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QJsonArray>
#include <QJsonDocument>

void DevToolsTest::frontendAndLifecycle()
{
    QQmlApplicationEngine engine;
    engine.loadData(R"(
        import QtQuick
        import QtWebEngine
        import Arbiter
        Window {
            id: root
            width: 960; height: 720; visible: true
            property var consoleResult: null
            property var pageResult: null
            property bool inspectedFromMenu: false
            WebEngineView {
                id: first
                width: root.width; height: root.height - dock.height
                profile: WebEngineProfile { offTheRecord: true }
                onContextMenuRequested: request => contextMenu.show(request)
                Component.onCompleted: loadHtml('<h1 id="fixture">Developer tools fixture</h1><input style="position:absolute;left:20px;top:100px">')
            }
            WebContextMenu {
                id: contextMenu
                view: first
                onInspectRequested: { root.inspectedFromMenu = true; dock.openFor(first, true) }
            }
            WebEngineView { id: second; visible: false; profile: first.profile }
            DeveloperToolsDock {
                id: dock
                anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
                maximumHeight: root.height - 160
            }
        }
    )");
    QVERIFY(!engine.rootObjects().isEmpty());
    QObject *root = engine.rootObjects().first();
    const auto evaluate = [&](const QString &code) {
        QQmlExpression expression(QQmlEngine::contextForObject(root), root, code);
        auto result = expression.evaluate();
        if (expression.hasError()) qWarning() << expression.error();
        return result;
    };
    QTRY_VERIFY_WITH_TIMEOUT(evaluate("!first.loading && first.url.toString() !== ''").toBool(), 10000);
    auto *window = qobject_cast<QQuickWindow *>(root);
    QVERIFY(window);
    const QString inspectAction = "(() => { for (let i = 0; i < contextMenu.count; ++i) { const item = contextMenu.itemAt(i); if (item.objectName === 'inspectElementAction') return item; } return null; })()";
    // Exercise real right-click requests on page content and on an editable field.
    for (const QPoint point : {QPoint(40, 35), QPoint(40, 115)}) {
        QTest::mouseClick(window, Qt::RightButton, Qt::NoModifier, point);
        QTRY_VERIFY(evaluate("contextMenu.visible").toBool());
        QVERIFY(evaluate("contextMenu.width >= 200").toBool());
        QVERIFY(evaluate("(() => { for (let i = 0; i < contextMenu.count; ++i) if (contextMenu.itemAt(i).objectName === 'printPageAction') return true; return false; })()").toBool());
        auto *action = evaluate(inspectAction).value<QObject *>();
        QVERIFY(action);
        QCOMPARE(action->property("text").toString(), QString("Inspect element"));
        QVERIFY(action->property("enabled").toBool());
        if (point.y() == 115) QVERIFY(QMetaObject::invokeMethod(action, "triggered"));
        evaluate("contextMenu.close()");
        QTRY_VERIFY(!evaluate("contextMenu.visible").toBool());
    }
    QVERIFY(root->property("inspectedFromMenu").toBool());
    QTRY_VERIFY_WITH_TIMEOUT(evaluate("dock.frontend && !dock.frontend.loading && dock.frontend.url.toString().startsWith('devtools://')").toBool(), 15000);
    QVERIFY(evaluate("first.devToolsView === dock.frontend && dock.target === first").toBool());
    QVERIFY(evaluate("dock.frontend.profile.offTheRecord").toBool());
    QVERIFY(evaluate("first.height === height - dock.height && dock.height === 340").toBool());
    // Wait for the frontend's SDK to attach before evaluating in the actual page.
    // This probes the bundled frontend; production code uses only public Qt APIs.
    const QString probe = R"JS(
        (async () => {
            const SDK = await import(new URL('core/sdk/sdk.js', location.href).href);
            let target;
            for (let i = 0; i < 200; ++i) {
                target = SDK.TargetManager.TargetManager.instance().targets().find(t => t.type() === 'frame');
                if (target) break;
                await new Promise(resolve => setTimeout(resolve, 50));
            }
            const result = await target.runtimeAgent().invoke_evaluate({
                expression: 'window.arbiterDevToolsCheck = 42', returnByValue: true
            });
            return result.result.value;
        })().then(value => globalThis.arbiterTestResult = value)
            .catch(error => globalThis.arbiterTestResult = String(error));
    )JS";
    const auto quoted = [](const QString &text) {
        QJsonArray array{text}; return QString::fromUtf8(QJsonDocument(array).toJson(QJsonDocument::Compact)).mid(1).chopped(1);
    };
    evaluate("dock.frontend.mainFrame.runJavaScript(" + quoted(probe) + ", 0, function() {})");
    QTRY_VERIFY_WITH_TIMEOUT(([&] {
        evaluate("dock.frontend.mainFrame.runJavaScript('globalThis.arbiterTestResult', 0, function(v) { root.consoleResult = v })");
        return root->property("consoleResult").toInt() == 42;
    })(), 15000);
    evaluate("first.mainFrame.runJavaScript('window.arbiterDevToolsCheck', 0, function(v) { root.pageResult = v })");
    QTRY_COMPARE(root->property("pageResult").toInt(), 42);
    evaluate("dock.preferredHeight = 450");
    QCOMPARE(evaluate("dock.height").toInt(), 450);
    evaluate("height = 480");
    QTRY_COMPARE(evaluate("dock.height").toInt(), 320);
    evaluate("dock.openFor(second)");
    QTRY_VERIFY(evaluate("dock.frontend && dock.frontend.inspectedView === second").toBool());
    QVERIFY(evaluate("first.devToolsView === null").toBool());
    evaluate("dock.closeFor(first)");
    QVERIFY(evaluate("dock.target === second").toBool());
    evaluate("dock.closeFor(second)");
    QVERIFY(evaluate("dock.target === null && dock.frontend === null && !dock.visible && dock.height === 0").toBool());
    QVERIFY(evaluate("second.devToolsView === null").toBool());
}

int main(int argc, char **argv)
{
    QStandardPaths::setTestModeEnabled(true);
    QtWebEngineQuick::initialize();
    QGuiApplication app(argc, argv);
    DevToolsTest test;
    return QTest::qExec(&test, argc, argv);
}
