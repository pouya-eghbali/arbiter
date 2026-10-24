#include "../src/extensioncatalog.h"
#include <QtTest>
#include <QtWebEngineQuick>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlExpression>
#include <QQuickWindow>
#include <QTemporaryDir>
#include <QStandardPaths>
#include <QJsonDocument>
#include <QTcpServer>
#include <QTcpSocket>

namespace {
QStringList profilePaths;
QString profileName(const QString &temporaryPath) { return "arbiter-extension-test-" + QFileInfo(temporaryPath).fileName(); }
    static void write(const QString &path, const QByteArray &bytes) {
        QFile file(path); QVERIFY(file.open(QIODevice::WriteOnly)); QCOMPARE(file.write(bytes), bytes.size());
    }
    static void configure(QQuickWebEngineProfile &profile, const QString &) {
        // Changing a live Quick profile's storage path invalidates Chromium's
        // ExtensionPrefs. Use a unique storage name and its initialized path.
        const auto path = profile.persistentStoragePath();
        if (!profilePaths.contains(path)) profilePaths.append(path);
    }
    static QString fixture(const QString &root, int version = 3, bool runtime = false) {
        const auto path = root + "/source"; QDir().mkpath(path);
        write(path + "/manifest.json", QByteArray(R"({"manifest_version":)") + QByteArray::number(version) + R"(,"name":"Arbiter extension test","version":"1.0","permissions":["storage"],"host_permissions":["http://127.0.0.1/*"],"content_scripts":[{"matches":["http://127.0.0.1/*"],"js":["content.js"]}],"action":{"default_popup":"popup.html"},"options_ui":{"page":"options.html"},"background":{"service_worker":"worker.js"}})");
        if (!runtime) {
            QFile file(path + "/manifest.json"); if (!file.open(QIODevice::ReadOnly)) return {};
            auto json = QJsonDocument::fromJson(file.readAll()).object(); file.close();
            json.remove("background"); write(file.fileName(), QJsonDocument(json).toJson());
        }
        write(path + "/content.js", R"(document.body.dataset.extensionInjected = "yes"; chrome.runtime.sendMessage({ping:true}, reply => {document.body.dataset.workerReply = reply && reply.answer || "failed"} );)");
        write(path + "/worker.js", R"(chrome.runtime.onMessage.addListener((msg,sender,reply)=>{ chrome.storage.local.set({visited:true}).then(()=>reply({answer:"pong"})); return true; });)");
        write(path + "/popup.html", "<!doctype html><title>Arbiter test popup</title><div id='popup'>Popup works</div><script src='popup.js'></script>");
        write(path + "/popup.js", R"(chrome.storage.local.get("visited").then(value=>document.body.dataset.visited=String(value.visited));)");
        write(path + "/options.html", "<!doctype html><title>Options</title>");
        return path;
    }
}

class ExtensionsTest : public QObject {
    Q_OBJECT
private slots:
    void liveChromeWebStoreInstall() {
        if (!qEnvironmentVariableIsSet("ARBITER_TEST_WEBSTORE")) QSKIP("Set ARBITER_TEST_WEBSTORE=1 for a live public store download.");
        QTemporaryDir temp; QQuickWebEngineProfile profile(profileName(temp.path())); configure(profile, {});
        ExtensionCatalog catalog(&profile);
        const QString id = "eimadpbcbfnmbkopoojfekhnkhdbieeh"; // Dark Reader, downloaded but never enabled.
        catalog.installFromStore("https://chromewebstore.google.com/detail/dark-reader/" + id);
        QVERIFY(catalog.busy());
        QTRY_VERIFY_WITH_TIMEOUT(!catalog.busy(), 150000);
        QVERIFY2(catalog.items().size() == 1, qPrintable(catalog.status()));
        QCOMPARE(catalog.items().first().toMap().value("id").toString(), id);
        QVERIFY(!catalog.items().first().toMap().value("enabled").toBool());
        QCOMPARE(catalog.review().value("id").toString(), id);
        catalog.cancelReview(); catalog.remove(id); QTRY_VERIFY_WITH_TIMEOUT(!catalog.busy(), 15000);
        QVERIFY(catalog.items().isEmpty());
    }
    void storeCancellationAndValidation() {
        QTemporaryDir temp; QQuickWebEngineProfile profile(profileName(temp.path())); configure(profile, {});
        ExtensionCatalog catalog(&profile);
        catalog.installFromStore("https://evil.example/extension"); QVERIFY(!catalog.busy()); QVERIFY(!catalog.status().isEmpty());
        catalog.installFromStore("eimadpbcbfnmbkopoojfekhnkhdbieeh"); QVERIFY(catalog.busy()); QVERIFY(catalog.canCancel());
        catalog.cancelInstall(); QVERIFY(!catalog.busy()); QVERIFY(!catalog.canCancel()); QVERIFY(catalog.items().isEmpty());
        QTest::qWait(100); QVERIFY(!catalog.busy()); QVERIFY(catalog.items().isEmpty());
    }
    void installReviewAndPersistence() {
        QTemporaryDir temp; QVERIFY(temp.isValid());
        QString id;
        const QString storage = temp.path() + "/profile";
        const QString name = profileName(temp.path());
        {
            QQuickWebEngineProfile profile(name); configure(profile, storage);
            ExtensionCatalog catalog(&profile);
            QVERIFY(catalog.available()); QVERIFY(catalog.items().isEmpty());
            catalog.install(QUrl::fromLocalFile(fixture(temp.path())));
            QVERIFY(catalog.busy());
            QTRY_VERIFY_WITH_TIMEOUT(!catalog.busy(), 15000);
            QCOMPARE(catalog.items().size(), 1);
            id = catalog.items().first().toMap().value("id").toString();
            QCOMPARE(catalog.review().value("id").toString(), id);
            QVERIFY(!catalog.items().first().toMap().value("enabled").toBool());
            QVERIFY(catalog.popupUrl(id).isEmpty());
            const auto permissions = catalog.review().value("permissions").toStringList();
            QVERIFY(permissions.contains("storage")); QVERIFY(permissions.contains("http://127.0.0.1/*"));
            catalog.cancelReview(); QVERIFY(catalog.review().isEmpty());
            catalog.requestEnable(id); catalog.approve();
            QVERIFY(catalog.items().first().toMap().value("enabled").toBool());
            QCOMPARE(catalog.popupUrl(id).scheme(), QString("chrome-extension"));
            QVERIFY(catalog.optionsUrl(id).path().endsWith("options.html"));
            // Invalid IDs must never reach Qt's internal uninstall lookup.
            catalog.remove("not-an-extension"); QVERIFY(!catalog.busy());
            QQuickWebEngineProfile other(name + "-other"); configure(other, temp.path() + "/other");
            ExtensionCatalog isolated(&other); QVERIFY(isolated.items().isEmpty());
            QQuickWebEngineProfile privateProfile;
            ExtensionCatalog privateCatalog(&privateProfile); QVERIFY(!privateCatalog.available());
            privateCatalog.install(QUrl::fromLocalFile(temp.path())); QVERIFY(!privateCatalog.busy());
        }
        QCoreApplication::processEvents();
        {
            QQuickWebEngineProfile profile(name); configure(profile, storage);
            ExtensionCatalog catalog(&profile);
            QTRY_COMPARE_WITH_TIMEOUT(catalog.items().size(), 1, 15000);
            QVERIFY(catalog.items().first().toMap().value("enabled").toBool());
            QCOMPARE(catalog.items().first().toMap().value("id").toString(), id);
            catalog.disable(id);
            QVERIFY(!catalog.items().first().toMap().value("enabled").toBool());
        }
        QCoreApplication::processEvents();
        {
            QQuickWebEngineProfile profile(name); configure(profile, storage);
            ExtensionCatalog catalog(&profile);
            QTRY_COMPARE_WITH_TIMEOUT(catalog.items().size(), 1, 15000);
            QVERIFY(!catalog.items().first().toMap().value("enabled").toBool());
            catalog.remove(id); QTRY_VERIFY_WITH_TIMEOUT(!catalog.busy(), 15000);
            QVERIFY(catalog.items().isEmpty());
        }
    }
    void zipAndFailures() {
        QTemporaryDir temp; QQuickWebEngineProfile profile(profileName(temp.path())); configure(profile, temp.path() + "/profile");
        ExtensionCatalog catalog(&profile);
        catalog.install(QUrl("https://example.com/extension.zip")); QVERIFY(!catalog.busy()); QVERIFY(!catalog.status().isEmpty());
        catalog.install(QUrl::fromLocalFile(fixture(temp.path(), 2)));
        QTRY_VERIFY_WITH_TIMEOUT(!catalog.busy(), 15000);
        QVERIFY(catalog.items().isEmpty()); QVERIFY(!catalog.status().isEmpty());
        const auto zipPath = temp.path() + "/extension.zip";
        QVERIFY(QFile::copy(QString::fromUtf8(ARBITER_EXTENSION_FIXTURE), zipPath));
        catalog.install(QUrl::fromLocalFile(zipPath));
        QTRY_VERIFY_WITH_TIMEOUT(!catalog.busy(), 15000);
        QCOMPARE(catalog.items().size(), 1); QVERIFY(!catalog.review().isEmpty());
        // Consent cannot be accepted after the manifest changes on disk.
        const auto info = profile.extensionManager()->extensions();
        for (const auto &extension : info) if (extension.isInstalled()) {
            QFile manifest(extension.path() + "/manifest.json"); QVERIFY(manifest.open(QIODevice::ReadOnly));
            auto json = QJsonDocument::fromJson(manifest.readAll()).object(); manifest.close();
            json.insert("description", "Changed since review"); write(manifest.fileName(), QJsonDocument(json).toJson());
        }
        catalog.approve(); QVERIFY(catalog.review().isEmpty());
        QVERIFY(!catalog.items().first().toMap().value("enabled").toBool());
        QVERIFY(catalog.status().contains("changed"));
    }
    void contentScriptsWorkerAndPopup() {
        QTemporaryDir temp; QQuickWebEngineProfile profile(profileName(temp.path())); configure(profile, temp.path() + "/profile");
        ExtensionCatalog catalog(&profile);
        catalog.install(QUrl::fromLocalFile(fixture(temp.path(), 3, true)));
        QTRY_VERIFY_WITH_TIMEOUT(!catalog.busy(), 15000);
        QCOMPARE(catalog.items().size(), 1); catalog.approve();
        const auto id = catalog.items().first().toMap().value("id").toString();
        QTcpServer server; QVERIFY(server.listen(QHostAddress::LocalHost));
        connect(&server, &QTcpServer::newConnection, this, [&] {
            auto *socket = server.nextPendingConnection();
            connect(socket, &QTcpSocket::readyRead, socket, [socket] {
                socket->readAll();
                const QByteArray body("<!doctype html><title>Test</title><body>Normal page</body>");
                socket->write("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: " + QByteArray::number(body.size()) + "\r\nConnection: close\r\n\r\n" + body);
                socket->disconnectFromHost();
            }); connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
        });
        QQmlApplicationEngine engine;
#ifdef ARBITER_WEBENGINE_QML_PATH
        engine.addImportPath(QString::fromUtf8(ARBITER_WEBENGINE_QML_PATH));
#endif
        engine.rootContext()->setContextProperty("testProfile", &profile);
        engine.rootContext()->setContextProperty("testUrl", QUrl(QString("http://127.0.0.1:%1/").arg(server.serverPort())));
        engine.rootContext()->setContextProperty("popupUrl", catalog.popupUrl(id));
        engine.loadData(R"(
import QtQuick
import QtWebEngine
Window {
    visible: true; width: 800; height: 600
    property string injected: ""
    property string reply: ""
    property string popupTitle: popup.title
    property string popupStorage: ""
    WebEngineView { id: page; width: 400; height: 500; profile: testProfile; url: testUrl }
    WebEngineView { id: popup; x: 400; width: 400; height: 500; profile: testProfile; url: popupUrl }
    function inspect() {
        page.mainFrame.runJavaScript("document.body?.dataset.extensionInjected || ''",0,function(v) { injected=v || '' })
        page.mainFrame.runJavaScript("document.body?.dataset.workerReply || ''",0,function(v) { reply=v || '' })
    }
    function inspectPopup() { popup.mainFrame.runJavaScript("document.body?.dataset.visited || ''",0,function(v) { popupStorage=v || '' }) }
    function reloadPopup() { popup.reload() }
})", QUrl("qrc:/extensions-test.qml"));
        QTRY_VERIFY_WITH_TIMEOUT(!engine.rootObjects().isEmpty(), 5000);
        auto *root = engine.rootObjects().first();
        QTRY_VERIFY_WITH_TIMEOUT((QMetaObject::invokeMethod(root,"inspect"), root->property("injected").toString() == "yes"), 15000);
        QTRY_VERIFY_WITH_TIMEOUT((QMetaObject::invokeMethod(root,"inspect"), root->property("reply").toString() == "pong"), 15000);
        QCOMPARE(root->property("popupTitle").toString(), QString("Arbiter test popup"));
        QMetaObject::invokeMethod(root,"reloadPopup");
        QTRY_VERIFY_WITH_TIMEOUT((QMetaObject::invokeMethod(root,"inspectPopup"), root->property("popupStorage").toString() == "true"), 15000);
    }
};
int main(int argc, char **argv) {
    QtWebEngineQuick::initialize(); QGuiApplication app(argc, argv);
    QCoreApplication::setOrganizationName("ArbiterExtensionTests");
    QCoreApplication::setApplicationName("ArbiterExtensionTests");
    QStandardPaths::setTestModeEnabled(true);
    ExtensionsTest test; const int result = QTest::qExec(&test, argc, argv);
    QCoreApplication::processEvents();
    for (const auto &path : profilePaths) QDir(path).removeRecursively();
    return result;
}
#include "extensions_test.moc"
