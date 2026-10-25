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
#include <QJsonArray>
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
    void awesomeScreenshotPopupSmoke() {
        const QString source = qEnvironmentVariable("ARBITER_TEST_AWESOME_SOURCE");
        if (source.isEmpty()) QSKIP("Set ARBITER_TEST_AWESOME_SOURCE to a trusted unpacked package for an opt-in popup smoke test.");
        QTemporaryDir temp; QQuickWebEngineProfile profile(profileName(temp.path())); configure(profile, {});
        ExtensionCatalog catalog(&profile); catalog.install(QUrl::fromLocalFile(source));
        QTRY_VERIFY_WITH_TIMEOUT(!catalog.busy(),30000); QCOMPARE(catalog.items().size(),1); catalog.approve();
        const auto id=catalog.items().first().toMap().value("id").toString();
        QQmlApplicationEngine engine;
#ifdef ARBITER_WEBENGINE_QML_PATH
        engine.addImportPath(QString::fromUtf8(ARBITER_WEBENGINE_QML_PATH));
#endif
        engine.rootContext()->setContextProperty("testProfile", &profile);
        engine.rootContext()->setContextProperty("popupUrl", catalog.popupUrl(id));
        engine.loadData(R"QML(
import QtQuick
import QtWebEngine
Window {
    id:owner;visible:true;width:600;height:600
    property bool ready:false
    property bool rendererTerminated:false
    WebEngineView {
        anchors.fill:parent;profile:testProfile;url:popupUrl
        onLoadingChanged: function(request) { if(request.status===WebEngineView.LoadSucceededStatus) owner.ready=true }
        onRenderProcessTerminated: owner.rendererTerminated=true
        onPermissionRequested: function(permission) { permission.deny() }
        onDesktopMediaRequested: function(request) { request.cancel() }
    }
})QML",QUrl("qrc:/awesome-popup-smoke.qml"));
        QVERIFY(!engine.rootObjects().isEmpty()); auto *root=engine.rootObjects().first();
        QTRY_VERIFY_WITH_TIMEOUT(root->property("ready").toBool(),15000);
        // Its popup starts asynchronous offscreen media-device discovery.
        QTest::qWait(5000);
        QVERIFY(!root->property("rendererTerminated").toBool());
    }

    void offscreenMediaDeviceEnumeration() {
        QTemporaryDir temp;
        QQuickWebEngineProfile profile(profileName(temp.path())); configure(profile, {});
        const auto source = fixture(temp.path());
        QFile manifest(source + "/manifest.json"); QVERIFY(manifest.open(QIODevice::ReadOnly));
        auto json = QJsonDocument::fromJson(manifest.readAll()).object(); manifest.close();
        json.remove("host_permissions"); json.remove("content_scripts");
        json.insert("permissions", QJsonArray{"storage", "offscreen"});
        write(manifest.fileName(), QJsonDocument(json).toJson());
        write(source + "/offscreen.html", "<!doctype html><script src='offscreen.js'></script>");
        write(source + "/offscreen.js", R"JS(
            navigator.mediaDevices.enumerateDevices().then(
                async () => {
                    let denied=false;
                    try {
                        const stream=await navigator.mediaDevices.getUserMedia({audio:true});
                        stream.getTracks().forEach(track=>track.stop());
                    } catch(error) { denied=error.name==='NotAllowedError'; }
                    chrome.runtime.sendMessage({enumerated:denied,error:'Hidden capture was not denied'});
                },
                error => chrome.runtime.sendMessage({enumerated:false,error:error.name}));
        )JS");
        write(source + "/worker.js", R"JS(
            chrome.runtime.onMessage.addListener(message => {
                if ('enumerated' in message)
                    chrome.storage.local.set({mediaProbe:message.enumerated?'passed':'FAILED: '+message.error});
            });
        )JS");
        json.insert("background", QJsonObject{{"service_worker", "worker.js"}});
        write(manifest.fileName(), QJsonDocument(json).toJson());
        write(source + "/popup.js", R"JS(
            window.startProbe=async()=>{
                try {
                    await chrome.storage.local.set({mediaProbe:''});
                    await chrome.offscreen.createDocument({url:'offscreen.html',reasons:['USER_MEDIA'],justification:'Test hidden-document device enumeration without requesting capture'});
                } catch(error) { await chrome.storage.local.set({mediaProbe:'FAILED: '+error.message}); }
            };
            window.readProbe=async()=>document.body.dataset.probe=(await chrome.storage.local.get('mediaProbe')).mediaProbe||'';
            window.closeProbe=async()=>{await chrome.offscreen.closeDocument();document.body.dataset.closed='yes'};
        )JS");
        ExtensionCatalog catalog(&profile); catalog.install(QUrl::fromLocalFile(source));
        QTRY_VERIFY_WITH_TIMEOUT(!catalog.busy(),15000); QCOMPARE(catalog.items().size(),1); catalog.approve();
        const auto id = catalog.items().first().toMap().value("id").toString();
        QQmlApplicationEngine engine;
#ifdef ARBITER_WEBENGINE_QML_PATH
        engine.addImportPath(QString::fromUtf8(ARBITER_WEBENGINE_QML_PATH));
#endif
        engine.rootContext()->setContextProperty("testProfile", &profile);
        engine.rootContext()->setContextProperty("popupUrl", catalog.popupUrl(id));
        engine.loadData(R"QML(
import QtQuick
import QtWebEngine
Window {
    id:owner
    visible:true;width:600;height:450
    property bool ready:false
    property string result:""
    property bool closed:false
    WebEngineView {
        id:popup;anchors.fill:parent;profile:testProfile;url:popupUrl
        onLoadingChanged: function(request) { if(request.status===WebEngineView.LoadSucceededStatus) owner.ready=true }
    }
    function start(){result="";closed=false;popup.mainFrame.runJavaScript("startProbe()",0,function(){})}
    function inspect(){popup.mainFrame.runJavaScript("readProbe();document.body.dataset.probe||''",0,value=>result=value||'')}
    function closeDocument(){popup.mainFrame.runJavaScript("closeProbe()",0,function(){})}
    function inspectClosed(){popup.mainFrame.runJavaScript("document.body.dataset.closed==='yes'",0,value=>closed=!!value)}
})QML", QUrl("qrc:/offscreen-media-test.qml"));
        QVERIFY(!engine.rootObjects().isEmpty()); auto *root=engine.rootObjects().first();
        QTRY_VERIFY_WITH_TIMEOUT(root->property("ready").toBool(),15000);
        QVERIFY(QMetaObject::invokeMethod(root,"start"));
        QTRY_VERIFY_WITH_TIMEOUT((QMetaObject::invokeMethod(root,"inspect"),!root->property("result").toString().isEmpty()),15000);
        QCOMPARE(root->property("result").toString(),QString("passed"));
        QVERIFY(QMetaObject::invokeMethod(root,"closeDocument"));
        QTRY_VERIFY_WITH_TIMEOUT((QMetaObject::invokeMethod(root,"inspectClosed"),root->property("closed").toBool()),10000);
    }

    void activeTabGrantIsScopedAndRevoked() {
        QTemporaryDir temp; QQuickWebEngineProfile profile(profileName(temp.path())); configure(profile, {});
        const auto source = fixture(temp.path());
        QFile manifest(source + "/manifest.json"); QVERIFY(manifest.open(QIODevice::ReadOnly));
        auto json = QJsonDocument::fromJson(manifest.readAll()).object(); manifest.close();
        json.remove("host_permissions"); json.remove("content_scripts"); json.remove("background");
        json.insert("permissions", QJsonArray{"activeTab", "scripting"});
        write(manifest.fileName(), QJsonDocument(json).toJson());
        write(source + "/popup.js", R"JS(
            window.probeAccess=async(granted)=>{
                try {
                    const [tab]=await chrome.tabs.query({active:true});
                    if(!tab)throw Error('Missing browser tab');
                    if(Boolean(tab.url)!==granted)throw Error('Tab URL permission filtering failed');
                    let allowed=false;
                    try { const value=await chrome.scripting.executeScript({target:{tabId:tab.id},func:()=>document.title}); allowed=value[0]?.result==='Access fixture'; } catch(e) {}
                    if(allowed!==granted)throw Error('Injection access does not match the user grant');
                    if(!granted){let rejected=false;try{await chrome.tabs.captureVisibleTab()}catch(e){rejected=true}if(!rejected)throw Error('Capture without access');}
                    document.body.dataset.access='passed';
                }catch(e){document.body.dataset.access='FAILED: '+e.message}
            };
        )JS");
        ExtensionCatalog catalog(&profile); catalog.install(QUrl::fromLocalFile(source));
        QTRY_VERIFY_WITH_TIMEOUT(!catalog.busy(),15000); QCOMPARE(catalog.items().size(),1);catalog.approve();
        const auto id=catalog.items().first().toMap().value("id").toString();
        QTcpServer server; QVERIFY(server.listen(QHostAddress::LocalHost));
        connect(&server,&QTcpServer::newConnection,this,[&]{auto *socket=server.nextPendingConnection();connect(socket,&QTcpSocket::readyRead,socket,[socket]{socket->readAll();const QByteArray body("<!doctype html><title>Access fixture</title><body>Access fixture</body>");socket->write("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: "+QByteArray::number(body.size())+"\r\nConnection: close\r\n\r\n"+body);socket->disconnectFromHost();});connect(socket,&QTcpSocket::disconnected,socket,&QObject::deleteLater);});
        QQmlApplicationEngine engine;
#ifdef ARBITER_WEBENGINE_QML_PATH
        engine.addImportPath(QString::fromUtf8(ARBITER_WEBENGINE_QML_PATH));
#endif
        engine.rootContext()->setContextProperty("testProfile",&profile);
        engine.rootContext()->setContextProperty("testUrl",QUrl(QString("http://127.0.0.1:%1/").arg(server.serverPort())));
        engine.rootContext()->setContextProperty("nextUrl",QUrl(QString("http://localhost:%1/next").arg(server.serverPort())));
        engine.rootContext()->setContextProperty("popupUrl",catalog.popupUrl(id));
        engine.rootContext()->setContextProperty("extensionId",id);
        engine.loadData(R"QML(
import QtQuick
import QtWebEngine
Window {
 id: owner;visible:true;width:800;height:500
 property string result:""
 property bool loaded:false
 property bool navigated:false
 WebEngineView {
  id:page;width:400;height:450;profile:testProfile;url:testUrl
  onLoadingChanged: function(request) {
   if (request.status === WebEngineView.LoadSucceededStatus) {
    owner.loaded = true
    owner.navigated = request.url.toString() === nextUrl.toString()
   }
  }
  extensionContext:QtObject {
   property int extensionTabId:-1
   readonly property QtObject window:owner
   property int index:0
   property bool active:true
   property bool pinned:false
   property bool blocked:false
  }
 }
 WebEngineView{id:popup;x:400;width:400;height:450;profile:testProfile;url:popupUrl}
 function probe(granted:bool){result="";popup.mainFrame.runJavaScript("document.body.dataset.access='';probeAccess("+granted+")",0,function(){})}
 function inspect(){popup.mainFrame.runJavaScript("document.body?.dataset.access || ''",0,v=>result=v||'')}
 function grant(){page.grantExtensionActiveTab(extensionId)}
 function navigate(){page.url=nextUrl}
})QML",QUrl("qrc:/extension-access-test.qml"));
        QVERIFY(!engine.rootObjects().isEmpty());auto *root=engine.rootObjects().first();
        QTRY_VERIFY_WITH_TIMEOUT(root->property("loaded").toBool(),15000);QTest::qWait(300);
        auto verify=[&](bool granted){QVERIFY(QMetaObject::invokeMethod(root,"probe",Q_ARG(bool,granted)));QTRY_VERIFY_WITH_TIMEOUT((QMetaObject::invokeMethod(root,"inspect"),!root->property("result").toString().isEmpty()),10000);QCOMPARE(root->property("result").toString(),QString("passed"));};
        verify(false);QVERIFY(QMetaObject::invokeMethod(root,"grant"));verify(true);
        QVERIFY(QMetaObject::invokeMethod(root,"navigate"));QTRY_VERIFY_WITH_TIMEOUT(root->property("navigated").toBool(),15000);verify(false);
    }

    void nativeTabsWindowsMessagingAndScripting() {
        QTemporaryDir temp;
        QQuickWebEngineProfile profile(profileName(temp.path())); configure(profile, {});
        QQuickWebEngineProfile other(profileName(temp.path()) + "-isolated"); configure(other, {});
        const auto source = fixture(temp.path(), 3, true);
        QFile manifest(source + "/manifest.json"); QVERIFY(manifest.open(QIODevice::ReadOnly));
        auto json = QJsonDocument::fromJson(manifest.readAll()).object(); manifest.close();
        json.insert("permissions", QJsonArray{"storage", "tabs", "scripting", "activeTab", "contextMenus"});
        json.insert("commands", QJsonObject{{"probe-command", QJsonObject{{"suggested_key", QJsonObject{{"default", "Alt+Shift+Y"}}}, {"description", "Test command"}}}});
        json.insert("host_permissions", QJsonArray{"<all_urls>"});
        write(manifest.fileName(), QJsonDocument(json).toJson());
        write(source + "/content.js", R"JS(
            document.body.dataset.ready='yes';
            chrome.runtime.onMessage.addListener((message,sender,reply)=>{
                if(message.probe){chrome.runtime.sendMessage({senderProbe:true},reply);return true;}
            });
        )JS");
        write(source + "/worker.js", R"JS(chrome.runtime.onMessage.addListener((message,sender,reply)=>{if(message.senderProbe) reply({tab:sender.tab?.id,window:sender.tab?.windowId});});)JS");
        write(source + "/injected.js", "document.body.dataset.injectedFile='yes';'file-result';");
        write(source + "/popup.js", R"JS(
            window.runProbe=async(capture)=>{
                const assert=(ok,message)=>{if(!ok)throw Error(message)};
                try{
                    const [tab]=await chrome.tabs.query({active:true,currentWindow:true});
                    assert(tab?.id>0 && tab.url.startsWith('http://127.0.0.1:'),'active browser tab');
                    assert((await chrome.tabs.query({})).length===1,'profile isolation and popup exclusion');
                    assert((await chrome.tabs.get(tab.id)).id===tab.id,'get tab');
                    assert((await chrome.tabs.getCurrent())===undefined,'popup is not a browser tab');
                    let failed=false;try{await chrome.tabs.get(999999999)}catch(e){failed=true}assert(failed,'unknown tab rejected');
                    const w=await chrome.windows.getCurrent({populate:true});
                    assert(w.id===tab.windowId && w.tabs.length===1,'window and populated tabs');
                    assert((await chrome.windows.getAll()).length===1,'window profile isolation');
                    assert((await chrome.windows.getLastFocused()).id===w.id,'last focused window');
                    await chrome.windows.update(w.id,{focused:true});
                    await chrome.tabs.setZoom(tab.id,1.25);assert((await chrome.tabs.getZoom(tab.id))===1.25,'zoom round trip');
                    await chrome.tabs.setZoom(tab.id,0);
                    await chrome.tabs.update(tab.id,{pinned:true});
                    assert((await chrome.tabs.query({pinned:true}))[0]?.id===tab.id,'pin update and filter');
                    await chrome.tabs.update(tab.id,{pinned:false});
                    const message=await chrome.tabs.sendMessage(tab.id,{probe:true});
                    assert(message?.tab===tab.id && message.window===w.id,'content messaging and sender.tab');
                    const injected=await chrome.scripting.executeScript({target:{tabId:tab.id},func:(value)=>{globalThis.__arbiterIsolatedProbe=value;return document.title+':'+value},args:['isolated']});
                    assert(injected[0]?.result==='Native API page:isolated' && injected[0].frameId===0,'function injection');
                    const main=await chrome.scripting.executeScript({target:{tabId:tab.id},world:'MAIN',func:()=>typeof globalThis.__arbiterIsolatedProbe});
                    assert(main[0]?.result==='undefined','isolated world');
                    assert((await chrome.scripting.executeScript({target:{tabId:tab.id},files:['injected.js']}))[0]?.result==='file-result','file injection');
                    failed=false;try{await chrome.scripting.executeScript({target:{tabId:tab.id},files:['../manifest.json']})}catch(e){failed=true}assert(failed,'unsafe resource rejected');
                    await chrome.scripting.insertCSS({target:{tabId:tab.id},css:'body{background:rgb(12,34,56)!important}'});
                    assert((await chrome.scripting.executeScript({target:{tabId:tab.id},func:()=>getComputedStyle(document.body).backgroundColor}))[0]?.result==='rgb(12, 34, 56)','CSS injection');
                    await chrome.scripting.removeCSS({target:{tabId:tab.id},css:'body{background:rgb(12,34,56)!important}'});
                    assert((await chrome.scripting.executeScript({target:{tabId:tab.id},func:()=>getComputedStyle(document.body).backgroundColor}))[0]?.result!=='rgb(12, 34, 56)','CSS removal');
                    await chrome.action.setBadgeText({text:'OK',tabId:tab.id});assert((await chrome.action.getBadgeText({tabId:tab.id}))==='OK','action badge state');
                    await chrome.action.setTitle({title:'Native API action'});assert((await chrome.action.getTitle({}))==='Native API action','action title');
                    if(capture){
                        const image=await chrome.tabs.captureVisibleTab(w.id,{format:'png'});
                        assert(image.startsWith('data:image/png;base64,iVBOR'),'real PNG screenshot');
                        const decoded=new Image();decoded.src=image;await decoded.decode();
                        assert(decoded.width>0&&decoded.height>0,'screenshot pixels');
                    }
                    assert(await chrome.permissions.contains({permissions:['activeTab']}),'approved permission query');
                    assert(await chrome.permissions.request({permissions:['activeTab']}),'already-approved permission request');
                    failed=false;try{await chrome.permissions.request({permissions:['downloads']})}catch(e){failed=true}assert(failed,'optional permission cannot silently expand access');
                    assert((await chrome.permissions.getAll()).permissions.includes('contextMenus'),'permission list');
                    assert((await chrome.commands.getAll()).some(value=>value.name==='probe-command'),'command metadata');
                    chrome.commands.onCommand.addListener((name,tab)=>document.body.dataset.command=name+':'+tab.id);
                    chrome.contextMenus.onClicked.addListener((info,tab)=>document.body.dataset.clicked=String(info.checked)+':'+tab.id);
                    const createMenu=(value)=>new Promise((resolve,reject)=>chrome.contextMenus.create(value,()=>chrome.runtime.lastError?reject(Error(chrome.runtime.lastError.message)):resolve()));
                    await createMenu({id:'parent',title:'Tools',contexts:['all']});
                    await createMenu({id:'child',parentId:'parent',title:'Capture fixture',type:'checkbox',contexts:['all']});
                    await createMenu({id:'hidden',title:'Hidden',visible:false});
                    await createMenu({id:'removed',title:'Removed'});
                    await chrome.contextMenus.remove('removed');
                    await chrome.contextMenus.update('child',{title:'Updated capture fixture'});
                    failed=false;try{await chrome.contextMenus.update('parent',{parentId:'child'})}catch(e){failed=true}assert(failed,'menu cycles rejected');
                    await chrome.action.setBadgeBackgroundColor({color:'#123456'});
                    assert(JSON.stringify(await chrome.action.getBadgeBackgroundColor({}))==='[18,52,86,255]','badge color state');
                    const created=await chrome.tabs.create({url:chrome.runtime.getURL('options.html'),active:false});
                    assert(created.id>0 && created.id!==tab.id && !created.active,'create extension result tab');
                    assert((await chrome.tabs.query({})).length===2,'created browser tab tracked');
                    await chrome.tabs.remove(created.id);
                    document.body.dataset.probe='passed';
                }catch(error){document.body.dataset.probe='FAILED: '+error.stack;}
            };
        )JS");
        ExtensionCatalog catalog(&profile); catalog.install(QUrl::fromLocalFile(source));
        QTRY_VERIFY_WITH_TIMEOUT(!catalog.busy(), 15000); QCOMPARE(catalog.items().size(), 1); catalog.approve();
        const auto id = catalog.items().first().toMap().value("id").toString();
        QTcpServer server; QVERIFY(server.listen(QHostAddress::LocalHost));
        connect(&server, &QTcpServer::newConnection, this, [&] {
            auto *socket = server.nextPendingConnection(); connect(socket, &QTcpSocket::readyRead, socket, [socket] {
                socket->readAll(); const QByteArray body("<!doctype html><title>Native API page</title><body style='background:white;color:black'>Native API fixture</body>");
                socket->write("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: " + QByteArray::number(body.size()) + "\r\nConnection: close\r\n\r\n" + body); socket->disconnectFromHost();
            }); connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
        });
        QQmlApplicationEngine engine;
#ifdef ARBITER_WEBENGINE_QML_PATH
        engine.addImportPath(QString::fromUtf8(ARBITER_WEBENGINE_QML_PATH));
#endif
        engine.rootContext()->setContextProperty("testProfile", &profile);
        engine.rootContext()->setContextProperty("otherProfile", &other);
        engine.rootContext()->setContextProperty("testUrl", QUrl(QString("http://127.0.0.1:%1/").arg(server.serverPort())));
        engine.rootContext()->setContextProperty("popupUrl", catalog.popupUrl(id));
        engine.rootContext()->setContextProperty("captureTest", qEnvironmentVariableIsSet("ARBITER_TEST_EXTENSION_CAPTURE"));
        engine.loadData(R"QML(
import QtQuick
import QtWebEngine
Window {
    id: testWindow
    visible: true; width: 900; height: 620
    property string ready: ""
    property string probe: ""
    property string menuResult: ""
    property string commandResult: ""
    property int menuCount: 0
    property int selected: 0
    property var spawned: []
    function createTab(address: string, active: bool): int {
        const view = dynamicPage.createObject(contentItem,{url:address,extensionIndex:spawned.length+1})
        spawned.push(view)
        if(active) selected=view.extensionContext.extensionTabId
        return view.extensionContext.extensionTabId
    }
    Component {
        id: dynamicPage
        WebEngineView {
            id: extra; property int extensionIndex: 1
            width: 300; height: 300; visible: false; profile: testProfile
            extensionContext: QtObject {
                property int extensionTabId: -1
                readonly property QtObject window: testWindow
                readonly property int index: extra.extensionIndex
                readonly property bool active: testWindow.selected===extensionTabId
                property bool pinned: false
                property bool blocked: false
                function activate(){testWindow.selected=extensionTabId}
                function closeTab(){extra.destroy()}
            }
        }
    }
    WebEngineView {
        id: page; width: 450; height: 550; profile: testProfile; url: testUrl
        extensionContext: QtObject {
            property int extensionTabId: -1
            readonly property QtObject window: testWindow
            property int index: 0
            readonly property bool active: testWindow.selected===0 || testWindow.selected===extensionTabId
            property bool pinned: false
            property bool blocked: false
            function activate(){testWindow.selected=extensionTabId}
            function setPinned(value: bool){pinned=value}
            function createTab(address: string, active: bool): int { return testWindow.createTab(address,active) }
        }
    }
    WebEngineView {
        id: foreign; width: 100; height: 100; visible: false; profile: otherProfile; url: testUrl
        extensionContext: QtObject {
            property int extensionTabId: -1
            readonly property QtObject window: testWindow
            property int index: 0
            property bool active: true
            property bool pinned: false
            property bool blocked: false
        }
    }
    WebEngineView { id: popup; x: 450; width: 450; height: 550; profile: testProfile; url: popupUrl }
    function inspect(){page.mainFrame.runJavaScript("document.body?.dataset.ready || ''",0,v=>ready=v||'')}
    function run(){popup.mainFrame.runJavaScript("runProbe("+captureTest+")",0,function(){})}
    function inspectProbe(){popup.mainFrame.runJavaScript("document.body?.dataset.probe || ''",0,v=>probe=v||'')}
    function clickMenu(){
        const data={pageUrl:page.mainFrame.url.toString(),editable:false,selectionText:"",linkUrl:"",srcUrl:"",mediaType:""}
        const items=page.extensionMenuItems(data)
        menuCount=items.length
        if(items.length) page.invokeExtensionMenu(items[0].key,data)
    }
    function inspectEvents(){
        popup.mainFrame.runJavaScript("document.body?.dataset.clicked || ''",0,v=>menuResult=v||'')
        popup.mainFrame.runJavaScript("document.body?.dataset.command || ''",0,v=>commandResult=v||'')
    }
})QML", QUrl("qrc:/extension-native-api-test.qml"));
        QVERIFY(!engine.rootObjects().isEmpty()); auto *root = engine.rootObjects().first();
        QTRY_VERIFY_WITH_TIMEOUT((QMetaObject::invokeMethod(root,"inspect"), root->property("ready").toString()=="yes"),15000);
        QTest::qWait(300);
        QVERIFY(QMetaObject::invokeMethod(root,"run"));
        QTRY_VERIFY_WITH_TIMEOUT((QMetaObject::invokeMethod(root,"inspectProbe"), !root->property("probe").toString().isEmpty()),30000);
        QCOMPARE(root->property("probe").toString(), QString("passed"));
        QVERIFY(QMetaObject::invokeMethod(root,"clickMenu"));
        QCOMPARE(root->property("menuCount").toInt(), 1);
        QTRY_VERIFY_WITH_TIMEOUT((QMetaObject::invokeMethod(root,"inspectEvents"), root->property("menuResult").toString().startsWith("true:")), 5000);
        if (qEnvironmentVariableIsSet("ARBITER_TEST_EXTENSION_CAPTURE")) {
            auto *window = qobject_cast<QQuickWindow *>(root); QVERIFY(window);
            window->requestActivate(); QTRY_VERIFY_WITH_TIMEOUT(window->isActive(), 5000);
            QTest::keyClick(window, Qt::Key_Y, Qt::AltModifier | Qt::ShiftModifier);
            QTRY_VERIFY_WITH_TIMEOUT((QMetaObject::invokeMethod(root,"inspectEvents"), root->property("commandResult").toString().startsWith("probe-command:")), 5000);
        }
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
