#include <QApplication>
#include <QAction>
#include <QMenuBar>
#include <QMenu>
#include <QQmlApplicationEngine>
#include <QRegularExpression>
#include <QQuickStyle>
#include <QtWebEngineCore/QWebEngineProfile>
#include <QtWebEngineCore/qtwebenginecoreglobal.h>
#include <QtWebEngineQuick/qtwebenginequickglobal.h>
#include "keybindings.h"
#include "settings.h"
#include "linuxwindowframe.h"
#include "localization.h"
#include "browserapplication.h"
#include "browserinstance.h"
#include <QStandardPaths>
#include "tabs.h"
#include "tabsleepsettings.h"
#include "privatebrowsing.h"
#include <QPointer>
#include "splitswapgesture.h"
#include "historyswipe.h"
#include "workspaces.h"
#include "webprofiles.h"
#include "siteaudio.h"
#include "sitepermissions.h"
#include "passwordmanager.h"
#include "credentialstore.h"
#include <QTemporaryDir>
#include <QQuickWindow>
#include "webauthncontroller.h"
#include "passkeycontroller.h"
#include "credentialsync.h"
#include "appearance.h"
#include "browserpreferences.h"
#include "updatetracker.h"
#include "adblocker.h"
#include "onboarding.h"
#include "systemintegration.h"
#include <QEventLoop>
#include "history.h"
#include "downloads.h"
#include "browserimporter.h"
#include "browserimportselection.h"
#include "browsericons.h"
#include "sitenavigation.h"
#include <QWindow>
#include "nativeglasshost.h"
#include <QQuickItem>
#include <QQuickWindow>
#include <QMouseEvent>
#include <qpa/qwindowsysteminterface.h>
#include <QFileInfo>
#include <QTimer>
#ifdef Q_OS_MACOS
void configureMacWindow(QWindow *window);
#endif

// Declare the engine last: it releases every page before its models/profiles.
struct PrivateWindowState final : QObject {
    explicit PrivateWindowState(PrivateBrowsing *privacy, DownloadSettings *downloadSettings, QObject *parent) : QObject(parent),
        workspaces(&tabs,QString()), downloads(downloadSettings,QString()), history(QString()) {
        profiles.privateMode=true;
        auto *auth=createBiometricAuth(this);
        session=new PrivateSession(privacy,auth,this);
        auth->setParent(session);
        QTimer::singleShot(0,auth,&BiometricAuth::refresh);
    }
    Tabs tabs;
    Workspaces workspaces;
    Downloads downloads;
    WebProfiles profiles;
    SiteAudio siteAudio{false};
    History history;
    WebAuthnController webauthn;
    PasskeyController passkeys;
    PrivateSession *session;
    QQmlApplicationEngine engine;
};

int main(int argc, char *argv[])
{
    QtWebEngineQuick::initialize();
    BrowserApplication app(argc, argv);
    if (qWebEngineArbiterExtensionApiAbi() != 2) qFatal("Arbiter requires its current native extension API engine.");
    QCoreApplication::setOrganizationName(QStringLiteral("Arbiter"));
    QCoreApplication::setApplicationName(QStringLiteral("Arbiter"));
    QCoreApplication::setApplicationVersion(QStringLiteral(ARBITER_VERSION));
    if (app.arguments().contains(QStringLiteral("--native-overlays-smoke-test"))) {
        QQmlApplicationEngine engine;
        QObject::connect(&engine, &QQmlEngine::exit, &app, &QCoreApplication::exit);
        engine.loadData(R"QML(
            import QtQuick
            import QtQuick.Controls.Basic
            import QtQuick.Layouts
            import Arbiter
            ApplicationWindow {
                id: root; width: 900; height: 650; visible: true; color: "#324943"
                property alias surface: surface
                readonly property bool surfaceWired: surface.popup === panel
                property alias workspace: workspace
                component ModalPanel: GlassSurface {}
                QtObject { id: testTabs; property int count: 0; property int currentIndex: -1; signal sessionChanged(); function choices() { return [] } }
                QtObject {
                    id: testWorkspaces
                    property int count: 1
                    property string currentId: "personal"
                    property string currentName: "Personal"
                    property color currentColor: "#8dbbff"
                    property string error: ""
                    property var choices: [{id: "personal", name: "Personal", color: "#8dbbff", tabs: 0, isolated: false, profile: "shared"}]
                }
                WorkspacePanel { id: workspace; manager: testWorkspaces; tabs: testTabs }
                property bool privateCovered: false
                property bool nativeEnabled: true
                property bool dismissOutside: true
                Binding { target: Theme; property: "liquidGlass"; value: root.nativeEnabled }
                property alias panel: panel
                property alias input: input
                property alias menu: menu
                property int actions: 0
                UiMenu {
                    id: menu; parent: panel.contentItem; x: 30; y: 40
                    UiMenuItem { objectName: "overlayMenuAction"; text: "Test action"; onTriggered: root.actions++ }
                }
                UiPopup {
                    id: panel; x: 100; y: 100; width: 420; height: 240
                    padding: 20; modal: true; focus: true
                    closePolicy: root.dismissOutside ? Popup.CloseOnEscape | Popup.CloseOnPressOutside : Popup.CloseOnEscape
                    background: ModalPanel { id: surface }
                    contentItem: ColumnLayout {
                        TextField { id: input; objectName: "overlayInput"; Layout.fillWidth: true }
                        UiButton { text: "Close"; onClicked: panel.close() }
                    }
                    onOpened: input.forceActiveFocus()
                }
            }
        )QML", QUrl("qrc:/native-overlay-smoke.qml"));
        if (engine.rootObjects().isEmpty()) return 2;
        auto root = engine.rootObjects().first();
        const bool expectedNative = NativeGlass::supported();
        NativeGlass::activateForTest(qobject_cast<QQuickWindow *>(root));
        QTimer::singleShot(400, root, [root] { QMetaObject::invokeMethod(root->property("panel").value<QObject *>(), "open"); });
        QTimer::singleShot(480, root, [root, expectedNative] {
            auto surface = root->property("surface").value<QObject *>();
            if (expectedNative && (!surface->property("nativeActive").toBool() || !surface->property("nativePreferred").toBool())) {
                qCritical("Native material was not attached during the opening fade"); qApp->exit(14);
            }
        });
        // A standalone GUI diagnostic may return activation to its launching
        // app when a native popup closes. Re-enter through the parent window
        // before the next interaction, just as a real user would.
        auto openPanel = [root] {
            auto main = qobject_cast<QQuickWindow *>(root);
            NativeGlass::activateForTest(main);
            QTimer::singleShot(80, root, [root] { QMetaObject::invokeMethod(root->property("panel").value<QObject *>(), "open"); });
        };
        auto timer = new QTimer(root);
        timer->setInterval(400);
        QObject::connect(timer, &QTimer::timeout, root, [root, timer, openPanel, expectedNative, phase = 0, cycle = 0]() mutable {
            auto panel = root->property("panel").value<QObject *>();
            auto menu = root->property("menu").value<QObject *>();
            auto input = qobject_cast<QQuickItem *>(root->property("input").value<QObject *>());
            auto click = [](QQuickItem *item) {
                auto point = QPointF(item->width()/2, item->height()/2);
                auto local = item->mapToScene(point);
                auto global = item->mapToGlobal(point);
                item->window()->requestActivate();
                if (NativeGlass::clickForTest(item->window(), global)) return;
                QWindowSystemInterface::handleMouseEvent<QWindowSystemInterface::SynchronousDelivery>(item->window(), local, global, Qt::LeftButton, Qt::LeftButton, QEvent::MouseButtonPress);
                QWindowSystemInterface::handleMouseEvent<QWindowSystemInterface::SynchronousDelivery>(item->window(), local, global, Qt::NoButton, Qt::LeftButton, QEvent::MouseButtonRelease);
            };
            qInfo() << "Popup cycle" << cycle << "phase" << phase << "visible" << panel->property("visible") << "focused" << (input && input->hasActiveFocus()) << "application" << qGuiApp->applicationState();
            if (phase == 0) {
                auto surface = root->property("surface").value<QObject *>();
                if (!root->property("surfaceWired").toBool()) { qCritical("Dialog background was not wired to the popup"); qApp->exit(16); return; }
                if (!surface || surface->property("nativeActive").toBool() != expectedNative || !input || !input->hasActiveFocus()) { qCritical("Native popup activation or focus failed"); qApp->exit(3); return; }
                for (auto host : surface->findChildren<NativeGlassHost *>()) {
                    if (!host->active()) continue;
                    if (!host->geometryAligned()) { qApp->exit(4); return; }
                    if (qGuiApp->applicationState() == Qt::ApplicationActive && input->property("color").value<QColor>() != host->foregroundColor()) { qCritical("Popup text did not inherit the native foreground color"); qApp->exit(13); return; }
                }
                input->setProperty("text", "");
                QKeyEvent press(QEvent::KeyPress, Qt::Key_A, Qt::NoModifier, "a");
                QCoreApplication::sendEvent(input->window(), &press);
                if (input->property("text").toString() != "a") { qCritical("Native popup text input failed"); qApp->exit(5); return; }
                QMetaObject::invokeMethod(menu, "open");
            } else if (phase == 1) {
                auto action = root->findChild<QQuickItem *>("overlayMenuAction");
                if (!menu->property("visible").toBool() || !action) { qApp->exit(8); return; }
                click(action);
            } else if (phase == 2) {
                if (root->property("actions").toInt() != cycle + 1 || !panel->property("visible").toBool()) { qCritical("Native nested menu action failed"); qApp->exit(9); return; }
                if (cycle % 2) {
                    auto main = qobject_cast<QQuickWindow *>(root);
                    const QPointF point(20, 20);
                    const auto global = main->mapToGlobal(point);
                    if (expectedNative && cycle == 5) {
                        NativeGlass::clickForTest(main, global);
                    } else if (expectedNative && (cycle == 1 || cycle == 3)) {
                        // Reproduce clicks delivered directly to the browser or
                        // an embedded native page view, rather than injecting
                        // out-of-bounds coordinates into the popup itself.
                        QWindow child(main);
                        child.setGeometry(0, 0, 80, 80);
                        auto target = cycle == 3 ? &child : static_cast<QWindow *>(main);
                        const auto local = target->mapFromGlobal(global);
                        QMouseEvent press(QEvent::MouseButtonPress, local, global, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
                        QMouseEvent release(QEvent::MouseButtonRelease, local, global, Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
                        QCoreApplication::sendEvent(target, &press);
                        QCoreApplication::sendEvent(target, &release);
                    } else {
                        const auto local = input->window()->mapFromGlobal(global);
                        QWindowSystemInterface::handleMouseEvent<QWindowSystemInterface::SynchronousDelivery>(input->window(), local, global, Qt::LeftButton, Qt::LeftButton, QEvent::MouseButtonPress);
                        QWindowSystemInterface::handleMouseEvent<QWindowSystemInterface::SynchronousDelivery>(input->window(), local, global, Qt::NoButton, Qt::LeftButton, QEvent::MouseButtonRelease);
                    }
                } else {
                    QKeyEvent escape(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
                    QCoreApplication::sendEvent(input->window(), &escape);
                }
            } else {
                if (panel->property("visible").toBool()) { qCritical("Native popup dismissal failed"); qApp->exit(6); return; }
                if (++cycle == 8) {
                    timer->stop();
                    openPanel();
                    QTimer::singleShot(500, root, [root] { root->setProperty("privateCovered", true); });
                    QTimer::singleShot(900, root, [root, panel, openPanel] {
                        if (panel->property("visible").toBool()) { qCritical("Private cover left a native popup visible"); qApp->exit(10); return; }
                        root->setProperty("privateCovered", false);
                        root->setProperty("nativeEnabled", false);
                        openPanel();
                    });
                    QTimer::singleShot(1400, root, [root, panel] {
                        auto surface = root->property("surface").value<QObject *>();
                        auto input = qobject_cast<QQuickItem *>(root->property("input").value<QObject *>());
                        if (surface->property("nativeActive").toBool() || input->window() != root) { qCritical("Glacier fallback did not restore the original window"); qApp->exit(11); return; }
                        QMetaObject::invokeMethod(panel, "close");
                    });
                    QTimer::singleShot(1800, root, [root, openPanel] { root->setProperty("nativeEnabled", true); openPanel(); });
                    QTimer::singleShot(2300, root, [root, expectedNative] {
                        if (root->property("surface").value<QObject *>()->property("nativeActive").toBool() != expectedNative) { qApp->exit(12); return; }
                        root->setProperty("dismissOutside", false);
                        auto main = qobject_cast<QQuickWindow *>(root);
                        const QPointF local(20, 20);
                        QMouseEvent press(QEvent::MouseButtonPress, local, main->mapToGlobal(local), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
                        QCoreApplication::sendEvent(main, &press);
                        QTimer::singleShot(300, root, [root, expectedNative] {
                            if (!root->property("panel").value<QObject *>()->property("visible").toBool()) { qCritical("Outside click dismissed a protected dialog"); qApp->exit(15); return; }
                            QMetaObject::invokeMethod(root->property("panel").value<QObject *>(), "close");
                            QTimer::singleShot(250, root, [root, expectedNative] {
                                NativeGlass::activateForTest(qobject_cast<QQuickWindow *>(root));
                                auto workspace = root->property("workspace").value<QObject *>();
                                QMetaObject::invokeMethod(workspace, "open");
                                QTimer::singleShot(400, root, [root, workspace, expectedNative] {
                                    if (!workspace->property("visible").toBool()) { qApp->exit(17); return; }
                                    auto main = qobject_cast<QQuickWindow *>(root);
                                    const QPointF local(15, 15);
                                    if (!NativeGlass::clickForTest(main, main->mapToGlobal(local))) {
                                        auto content = qobject_cast<QQuickItem *>(workspace->property("contentItem").value<QObject *>());
                                        const auto global = main->mapToGlobal(local);
                                        const auto point = content->window()->mapFromGlobal(global);
                                        QMouseEvent event(QEvent::MouseButtonPress, point, global, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
                                        QCoreApplication::sendEvent(content->window(), &event);
                                    }
                                    QTimer::singleShot(300, root, [workspace, expectedNative] {
                                        if (workspace->property("visible").toBool()) { qCritical("Workspace selector ignored the native outside click"); qApp->exit(18); return; }
                                        qInfo("Native menu input, workspace outside dismissal, protected dialogs, private cover and material transitions passed (8 cycles; native=%s)", expectedNative ? "true" : "false");
                                        qApp->quit();
                                    });
                                });
                            });
                        });
                    });
                    return;
                }
                openPanel();
                phase = -1;
            }
            ++phase;
        });
        QTimer::singleShot(700, timer, [timer] { timer->start(); });
        QTimer::singleShot(25000, &app, [] { qApp->exit(7); });
        return app.exec();
    }
    // Isolated UI smoke path: no settings, profiles, credentials or network.
    if (app.arguments().contains(QStringLiteral("--native-glass-smoke-test"))) {
        QQmlApplicationEngine smoke;
        QObject::connect(&smoke, &QQmlEngine::exit, &app, &QCoreApplication::exit);
        QTimer::singleShot(15000, &app, [] { QCoreApplication::exit(5); });
        smoke.loadData(R"QML(
            import QtQuick
            import QtQuick.Controls.Basic
            import Arbiter
            ApplicationWindow {
                id: owner; width: 800; height: 500; visible: true; color: "#315449"
                title: "Arbiter native glass smoke test"
                NativeGlassHost { id: capability; visible: false }
                QtObject {
                    id: preferences
                    property bool liquidGlass: true
                    property string navigationRevealMode: "handle"
                    function displayAddress(url) { return "example.com" }
                }
                NavigationPill {
                    id: pill; objectName: "fixturePill"; anchors.horizontalCenter: parent.horizontalCenter
                    anchors.bottom: parent.bottom; anchors.bottomMargin: 20
                    view: null; appearance: preferences; workspaceName: "Test"
                    workspaceColor: "#8dbbff"; tabCount: 2; revealed: true
                }
                Timer {
                    interval: 500; running: true; repeat: true
                    property int step: 0
                    onTriggered: {
                        if (!capability.available) { Qt.exit(pill.nativeActive ? 2 : 0); return }
                        if (step === 0 && !pill.nativeActive) { Qt.exit(3); return }
                        if (step === 1 && pill.nativeActive) { Qt.exit(4); return }
                        if (step === 2 && !pill.nativeActive) { Qt.exit(6); return }
                        if (step === 3 && pill.nativeActive) { Qt.exit(7); return }
                        if (step === 4 && !pill.nativeActive) { Qt.exit(8); return }
                        if (step === 0) preferences.liquidGlass = false
                        if (step === 1) preferences.liquidGlass = true
                        if (step === 2) pill.forceHidden = true
                        if (step === 3) { pill.forceHidden = false; pill.revealed = true }
                        if (step === 4) pill.reveal()
                        if (step === 5 && !pill.nativeActive) { Qt.exit(10); return }
                        if (step === 6) { if (!pill.nativeActive) { Qt.exit(11); return }; console.info("Desktop native glass lifecycle and menu-input smoke test passed"); Qt.exit(0) }
                        step++
                    }
                }
            }
        )QML", QUrl(QStringLiteral("qrc:/native-glass-smoke.qml")));
        if (smoke.rootObjects().isEmpty()) return 2;
        auto root = smoke.rootObjects().first();
        QTimer::singleShot(2700, root, [root] {
            for (auto host : root->findChildren<NativeGlassHost *>()) {
                if (host->active() && !host->geometryAligned()) { qCritical("Desktop native controls escaped the pill"); QCoreApplication::exit(14); return; }
            }
            auto address = root->findChild<QQuickItem *>("navigationAddressButton");
            if (!address || !address->hasActiveFocus()) { qCritical("Native address keyboard focus was lost"); QCoreApplication::exit(9); return; }
            auto button = root->findChild<QQuickItem *>("navigationMoreButton");
            if (!button || !button->window()) { QCoreApplication::exit(12); return; }
            const auto point = button->mapToScene(QPointF(button->width()/2, button->height()/2));
            const auto global = button->mapToGlobal(QPointF(button->width()/2, button->height()/2));
            QMouseEvent press(QEvent::MouseButtonPress, point, global, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
            QMouseEvent release(QEvent::MouseButtonRelease, point, global, Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
            QCoreApplication::sendEvent(button->window(), &press);
            QCoreApplication::sendEvent(button->window(), &release);
        });
        QTimer::singleShot(2850, root, [root] {
            auto menu = root->findChild<QObject *>("navigationMoreMenu");
            auto content = menu ? qobject_cast<QQuickItem *>(menu->property("contentItem").value<QObject *>()) : nullptr;
            auto pill = root->findChild<QObject *>("fixturePill");
            if (!content || !pill || !pill->property("nativeActive").toBool()) { qCritical("More menu replaced native pill material"); qApp->exit(15); return; }
            root->setProperty("morePosition", content->mapToGlobal(QPointF()));
        });
        QTimer::singleShot(3200, root, [root] {
            auto menu = root->findChild<QObject *>("navigationMoreMenu");
            if (!menu || !menu->property("visible").toBool()) { qCritical("Native More button did not open the menu"); QCoreApplication::exit(13); return; }
            auto content = qobject_cast<QQuickItem *>(menu->property("contentItem").value<QObject *>());
            const auto at = content->mapToGlobal(QPointF());
            const auto previous = root->property("morePosition").toPointF();
            if (QLineF(at, previous).length() > 1) { qCritical() << "More menu moved after opening" << previous << "to" << at << "window" << content->window()->geometry(); qApp->exit(16); return; }
            QMetaObject::invokeMethod(menu, "close");
        });
        return app.exec();
    }
    if(app.arguments().contains(QStringLiteral("--settings-ui-smoke-test"))) {
        QQmlApplicationEngine preview;
        preview.loadFromModule(QStringLiteral("Arbiter"),QStringLiteral("SettingsPreview"));
        if(preview.rootObjects().isEmpty()) return 2;
        auto *window=qobject_cast<QQuickWindow *>(preview.rootObjects().first());
        if(!window) return 3;
        const bool manager=app.arguments().contains(QStringLiteral("--saved-logins"));
        window->setProperty("showManager",manager);
        if(manager) QMetaObject::invokeMethod(window,"openManagerPreview");
        QString output;
        for(const auto &argument:app.arguments()) if(argument.startsWith("--design-output=")) output=argument.mid(16);
        QTimer::singleShot(1200,window,[&,window,output] { app.exit(window->grabWindow().save(output) ? 0 : 4); });
        return app.exec();
    }
    if(app.arguments().contains(QStringLiteral("--credential-store-smoke-test"))) {
        // Disposable signed-app probe: no browser profile, actual credentials,
        // or biometric prompts. Verify the Data Protection master-key path.
        QTemporaryDir directory;
        KeychainSecretStore keys(nullptr,QStringLiteral("Arbiter Credentials"));
        CredentialStore first(directory.path(),&keys),second(directory.path(),&keys);
        const QByteArray fixture(12000,'x');int result=1;
        first.write(QStringLiteral("probe"),fixture,[&](const QString &error,const QByteArray &) {
            if(!error.isEmpty()) { qWarning().noquote()<<error;app.exit(1);return; }
            second.readSilently(QStringLiteral("probe"),[&](const QString &error,const QByteArray &value) {
                const bool valid=error.isEmpty() && value==fixture;
                QFile identity(directory.filePath(QStringLiteral("identity")));
                if(!identity.open(QIODevice::ReadOnly)) {app.exit(2);return;}
                const auto account=QStringLiteral("com.arbiter.browser/credentials/v1/")+QString::fromLatin1(identity.readAll().toHex());
                keys.remove(account,[&,valid](const QString &cleanup,const QByteArray &) {
                    result=valid && cleanup.isEmpty() ? 0 : 3;
                    if(!valid) qWarning()<<"Data Protection credential round trip failed";
                    app.exit(result);
                });
            });
        });
        QTimer::singleShot(30000,&app,[&]{app.exit(4);});
        return app.exec();
    }
    // Isolated engine check: no profile, credentials, settings or websites.
    if (app.arguments().contains(QStringLiteral("--release-smoke-test"))) {
        QQmlApplicationEngine smoke;
        QObject::connect(&smoke, &QQmlEngine::exit, &app, &QCoreApplication::exit);
        QObject::connect(&smoke, &QQmlEngine::quit, &app, [] { QCoreApplication::exit(0); });
        QTimer::singleShot(30000, &app, [] { QCoreApplication::exit(5); });
        smoke.loadData(R"QML(
            import QtQuick
            import QtQuick.Window
            import QtWebEngine
            Window {
                visible: true; width: 64; height: 64; opacity: 0
                WebEngineView {
                    anchors.fill: parent
                    profile: WebEngineProfile { offTheRecord: true }
                    Component.onCompleted: loadHtml("<title>Arbiter release smoke</title><p>Ready</p>")
                    onLoadingChanged: function(request) {
                        if (request.status === WebEngineView.LoadSucceededStatus)
                            runJavaScript("document.title", function(title) { Qt.exit(title === "Arbiter release smoke" ? 0 : 3) })
                        else if (request.status === WebEngineView.LoadFailedStatus) Qt.exit(4)
                    }
                }
            }
        )QML", QUrl(QStringLiteral("qrc:/release-smoke.qml")));
        if (smoke.rootObjects().isEmpty()) return 2;
        return app.exec();
    }
#ifdef Q_OS_LINUX
    app.setDesktopFileName(QStringLiteral("com.arbiter.browser"));
#endif
#ifdef Q_OS_MACOS
    QQuickStyle::setStyle(QStringLiteral("macOS"));
#endif

    BrowserInstance instance(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));
    const auto instanceResult=instance.start(app.arguments().mid(1));
    if (instanceResult==BrowserInstance::Forwarded) return 0;
    if (instanceResult==BrowserInstance::Failed) { qCritical().noquote()<<instance.error(); return 1; }
    QObject::connect(&instance, &BrowserInstance::urlReceived, &app, &BrowserApplication::receiveUrl);
    QObject::connect(&instance, &BrowserInstance::activationRequested, &app, [&app] { app.receiveUrl(QUrl()); });
    const QString settingsError=Settings::initialize();
    Localization localization;
    KeyBindings keyBindings;
    Appearance appearance;
    BrowserPreferences browserPreferences;
    UpdateTracker updateTracker;
    SiteAudio siteAudio;
    AdBlocker adblocker;
    Onboarding onboarding(&appearance);
    SystemIntegration systemIntegration;
    Tabs tabs;
    TabSleepSettings tabSleep;
    PrivateBrowsing privacy;
    Workspaces workspaces(&tabs);
    DownloadSettings downloadSettings;
    Downloads downloads(&downloadSettings);
    WebProfiles webProfiles;
    SitePermissionSettings sitePermissionSettings;
    webProfiles.profileCreated=[&sitePermissionSettings,&downloads](QQuickWebEngineProfile *profile) { sitePermissionSettings.trackProfile(profile); downloads.trackProfile(profile); };
    History history;
    const auto profileRoot=QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
        +QStringLiteral("/credentials-v2/Default");
    KeychainSecretStore credentialKeys(nullptr,QStringLiteral("Arbiter Credentials"));
    CredentialStore credentialStore(profileRoot,&credentialKeys);
    PasswordManager passwords(profileRoot+QStringLiteral("/passwords"),QStringLiteral("Default"),&credentialStore);
    BrowserImporter importer(&passwords,&history);
    BrowserImportSelection importSelection(&importer);
    WebAuthnController webauthn;
    PasskeyController passkeys(&credentialStore,profileRoot+QStringLiteral("/passkeys"),nullptr);
    CredentialSync credentialSync(&passwords,passkeys.vaultForSync(),profileRoot+QStringLiteral("/sync"),&credentialStore,{});
    auto *passwordBiometrics=createBiometricAuth(&passwords);
    const auto updatePasswordBiometrics=[&passwords,passwordBiometrics] {
        passwords.setUserVerification(passwordBiometrics->name(),passwordBiometrics->available(),
            [passwordBiometrics](const QString &reason,PasswordManager::VerifyCallback callback) { passwordBiometrics->authenticate(reason,std::move(callback)); });
    };
    QObject::connect(passwordBiometrics,&BiometricAuth::changed,&passwords,updatePasswordBiometrics);
    QObject::connect(passwordBiometrics,&BiometricAuth::sessionLocked,&passwords,[&passwords,passwordBiometrics] {
        passwords.clearUserVerification();
        passwordBiometrics->cancel();
    });
    updatePasswordBiometrics();
    QTimer::singleShot(0,passwordBiometrics,&BiometricAuth::refresh);
    qmlRegisterType<SplitSwapGesture>("Arbiter.Input", 1, 0, "SplitSwapGesture");
    qmlRegisterType<LinuxWindowFrame>("Arbiter.Windowing", 1, 0, "LinuxWindowFrame");
    qmlRegisterType<PasswordSession>("Arbiter.Credentials", 1, 0, "PasswordSession");
    QMenuBar nativeMenu;
    SiteNavigation siteNavigation(&nativeMenu);
    QQmlApplicationEngine engine;
    QObject privateWindows;
#ifdef ARBITER_WEBENGINE_QML_PATH
    // A split development SDK must supply its matching QML plugin as well as
    // its C++ libraries. Packaged builds must deploy this module with the app.
    engine.addImportPath(QString::fromUtf8(ARBITER_WEBENGINE_QML_PATH));
#endif
    if (!onboarding.completed()) {
        // Finish setup before constructing WebEngine views or choosing native flags.
        app.setQuitOnLastWindowClosed(false);
        QEventLoop setupLoop;
        QQmlApplicationEngine setupEngine;
        setupEngine.addImageProvider(QStringLiteral("browsers"), createBrowserIconProvider());
        setupEngine.setInitialProperties({{QStringLiteral("controller"), QVariant::fromValue(&onboarding)},
                                          {QStringLiteral("appearance"), QVariant::fromValue(&appearance)},
                                          {QStringLiteral("keyBindings"), QVariant::fromValue(&keyBindings)},
                                          {QStringLiteral("importer"), QVariant::fromValue(&importer)},
                                          {QStringLiteral("importSelection"), QVariant::fromValue(&importSelection)},
                                          {QStringLiteral("systemIntegration"), QVariant::fromValue(&systemIntegration)}});
        QObject::connect(&onboarding, &Onboarding::finished, &setupLoop, &QEventLoop::quit);
        QObject::connect(&onboarding, &Onboarding::aborted, &setupLoop, &QEventLoop::quit);
        QObject::connect(&app, &QCoreApplication::aboutToQuit, &setupLoop, &QEventLoop::quit);
        setupEngine.loadFromModule(QStringLiteral("Arbiter"), QStringLiteral("Onboarding"));
        if (setupEngine.rootObjects().isEmpty()) return 1;
        setupLoop.exec();
        if (!onboarding.completed()) return 0;
    }
    app.setQuitOnLastWindowClosed(true);
    // Keep Chromium's real platform/version tokens, without the embedding-toolkit
    // and application products that sites can mistake for an unsupported browser.
    QString userAgent = QWebEngineProfile::defaultProfile()->httpUserAgent();
    userAgent.remove(QRegularExpression(QStringLiteral("\\s+(?:QtWebEngine|Arbiter)/[^\\s]+")));
    webProfiles.userAgent = userAgent;
    const bool diagnostics = qEnvironmentVariableIsSet("ARBITER_DIAGNOSTICS");
    const QVariantMap initialProperties{{QStringLiteral("localization"), QVariant::fromValue(&localization)},
                                 {QStringLiteral("privacy"), QVariant::fromValue(&privacy)},
                                 {QStringLiteral("browserUserAgent"), userAgent},
                                 {QStringLiteral("settingsFile"), Settings::configFile()},
                                 {QStringLiteral("settingsDirectoryUrl"), QUrl::fromLocalFile(QFileInfo(Settings::configFile()).absolutePath())},
                                 {QStringLiteral("settingsError"), settingsError},
                                 {QStringLiteral("appearance"), QVariant::fromValue(&appearance)},
                                 {QStringLiteral("browserPreferences"), QVariant::fromValue(&browserPreferences)},
                                 {QStringLiteral("updateTracker"), QVariant::fromValue(&updateTracker)},
                                 {QStringLiteral("siteAudio"), QVariant::fromValue(&siteAudio)},
                                 {QStringLiteral("adblocker"), QVariant::fromValue(&adblocker)},
                                 {QStringLiteral("onboarding"), QVariant::fromValue(&onboarding)},
                                 {QStringLiteral("systemIntegration"), QVariant::fromValue(&systemIntegration)},
                                 {QStringLiteral("windowDecorations"), appearance.windowDecorations()},
                                 {QStringLiteral("siteNavigation"), QVariant::fromValue(&siteNavigation)},
                                 {QStringLiteral("keyBindings"), QVariant::fromValue(&keyBindings)},
                                 {QStringLiteral("tabs"), QVariant::fromValue(&tabs)},
                                 {QStringLiteral("tabSleep"), QVariant::fromValue(&tabSleep)},
                                 {QStringLiteral("workspaces"), QVariant::fromValue(&workspaces)},
                                 {QStringLiteral("webProfiles"), QVariant::fromValue(&webProfiles)},
                                 {QStringLiteral("sitePermissionSettings"), QVariant::fromValue(&sitePermissionSettings)},
                                 {QStringLiteral("history"), QVariant::fromValue(&history)},
                                 {QStringLiteral("downloads"), QVariant::fromValue(&downloads)},
                                 {QStringLiteral("passwords"), QVariant::fromValue(&passwords)},
                                 {QStringLiteral("credentialSync"), QVariant::fromValue(&credentialSync)},
                                 {QStringLiteral("importer"), QVariant::fromValue(&importer)},
                                 {QStringLiteral("webauthn"), QVariant::fromValue(&webauthn)},
                                 {QStringLiteral("passkeys"), QVariant::fromValue(&passkeys)},
                                 {QStringLiteral("browserDiagnostics"), diagnostics}};
    engine.setInitialProperties(initialProperties);
    if (diagnostics) {
        qInfo().noquote() << "Chromium:" << qWebEngineChromiumVersion();
        qInfo().noquote() << "User agent:" << userAgent;
    }
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
                     &app, [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.loadFromModule(QStringLiteral("Arbiter"), QStringLiteral("Main"));
    if (!engine.rootObjects().isEmpty()) {
        updateTracker.start();
        QObject *root = engine.rootObjects().first();
        QObject::connect(root,SIGNAL(preferencesRequested()),root,SLOT(openSettings()));
        QObject::connect(&privacy,&PrivateBrowsing::openRequested,&app,[&,root,initialProperties] {
            auto *state=new PrivateWindowState(&privacy,&downloadSettings,&privateWindows);
            state->profiles.userAgent=userAgent;
            state->profiles.profileCreated=[&sitePermissionSettings,state](QQuickWebEngineProfile *profile) { sitePermissionSettings.trackProfile(profile); state->downloads.trackProfile(profile); };
#ifdef ARBITER_WEBENGINE_QML_PATH
            state->engine.addImportPath(QString::fromUtf8(ARBITER_WEBENGINE_QML_PATH));
#endif
            auto properties=initialProperties;
            properties["privateMode"]=true;
            properties["privateSession"]=QVariant::fromValue(state->session);
            properties["tabs"]=QVariant::fromValue(&state->tabs);
            properties["workspaces"]=QVariant::fromValue(&state->workspaces);
            properties["webProfiles"]=QVariant::fromValue(&state->profiles);
            properties["siteAudio"]=QVariant::fromValue(&state->siteAudio);
            properties["history"]=QVariant::fromValue(&state->history);
            properties["downloads"]=QVariant::fromValue(&state->downloads);
            properties["webauthn"]=QVariant::fromValue(&state->webauthn);
            properties["passkeys"]=QVariant::fromValue(&state->passkeys);
            properties["browserDiagnostics"]=false;
            state->engine.setInitialProperties(properties);
            state->engine.loadFromModule(QStringLiteral("Arbiter"),QStringLiteral("Main"));
            if(state->engine.rootObjects().isEmpty()) { state->deleteLater(); return; }
            auto *window=qobject_cast<QWindow *>(state->engine.rootObjects().first());
            if(!window) { state->deleteLater(); return; }
            state->session->setWindow(window);
            QObject::connect(window,SIGNAL(preferencesRequested()),root,SLOT(openSettings()));
            // Opening preferences deliberately returns to the regular window.
            QObject::connect(window,SIGNAL(preferencesRequested()),root,SLOT(show()));
            QObject::connect(window,SIGNAL(preferencesRequested()),root,SLOT(raise()));
            QObject::connect(window,SIGNAL(preferencesRequested()),root,SLOT(requestActivate()));
            QObject::connect(window,SIGNAL(keyboardPreferencesRequested()),root,SLOT(openShortcutSettings()));
            QObject::connect(window,SIGNAL(keyboardPreferencesRequested()),root,SLOT(show()));
            QObject::connect(window,SIGNAL(keyboardPreferencesRequested()),root,SLOT(raise()));
            QObject::connect(window,SIGNAL(keyboardPreferencesRequested()),root,SLOT(requestActivate()));
            QObject::connect(window,&QWindow::visibleChanged,state,[state,window](bool visible) {
                if(!visible) { QMetaObject::invokeMethod(window,"prepareForShutdown"); state->deleteLater(); }
            });
            QObject::connect(&app,&QCoreApplication::aboutToQuit,window,[window] { QMetaObject::invokeMethod(window,"prepareForShutdown"); });
#ifdef Q_OS_MACOS
            configureMacWindow(window);
#endif
            window->raise(); window->requestActivate();
        });
        QObject::connect(&app, &QCoreApplication::aboutToQuit, root, [root] {
            QMetaObject::invokeMethod(root, "prepareForShutdown");
        });
        app.setUrlHandler([root](const QUrl &url) {
            if (!url.isEmpty()) QMetaObject::invokeMethod(root, "openExternalUrl", Q_ARG(QString, url.toString()));
            if (auto *window = qobject_cast<QWindow *>(root)) {
                if (window->visibility() == QWindow::Minimized)
                    window->showNormal();
                window->raise();
                window->requestActivate();
            }
        });
        for (const auto &argument : app.arguments().mid(1)) {
            const QUrl url(argument);
            if ((url.scheme() == "http" || url.scheme() == "https") && !url.host().isEmpty())
                QMetaObject::invokeMethod(root, "openExternalUrl", Q_ARG(QString, url.toString()));
        }
        auto *applicationMenu = nativeMenu.addMenu(QStringLiteral("Arbiter"));
        auto *settingsAction = applicationMenu->addAction(QObject::tr("Settings…"));
        settingsAction->setMenuRole(QAction::PreferencesRole);
        const auto updateSettingsShortcut = [&keyBindings, settingsAction] {
            settingsAction->setShortcut(QKeySequence::fromString(
                keyBindings.bindings().value(QStringLiteral("settings")).toString(), QKeySequence::PortableText));
        };
        updateSettingsShortcut();
        QObject::connect(&keyBindings, &KeyBindings::bindingsChanged, settingsAction, updateSettingsShortcut);
        QObject::connect(settingsAction, &QAction::triggered, root, [root] {
            QMetaObject::invokeMethod(root, "openSettings");
        });
        QObject::connect(root, SIGNAL(shortcutRecordingChanged(bool)), settingsAction, SLOT(setDisabled(bool)));
        auto *privateAction=applicationMenu->addAction(QObject::tr("New Private Window"));
        QObject::connect(privateAction,&QAction::triggered,&privacy,&PrivateBrowsing::open);
        auto *tabsMenu = nativeMenu.addMenu(QObject::tr("Tabs"));
        const auto addTabAction = [root, tabsMenu](const QString &label, const char *method) {
            auto *action = tabsMenu->addAction(label);
            QObject::connect(action, &QAction::triggered, root, [root, method] {
                QMetaObject::invokeMethod(QGuiApplication::focusWindow() ? static_cast<QObject *>(QGuiApplication::focusWindow()) : root, method);
            });
        };
        addTabAction(QObject::tr("New Tab"), "newTab");
        addTabAction(QObject::tr("Show Tabs…"), "showTabs");
        addTabAction(QObject::tr("Close Tab"), "closeCurrentTab");
        addTabAction(QObject::tr("Reopen Closed Tab"), "reopenTab");
        tabsMenu->addSeparator();
        addTabAction(QObject::tr("Next Tab"), "nextTab");
        addTabAction(QObject::tr("Previous Tab"), "previousTab");
        tabsMenu->addSeparator();
        addTabAction(QObject::tr("Split Left / Right"), "splitHorizontal");
        addTabAction(QObject::tr("Split Top / Bottom"), "splitVertical");
        addTabAction(QObject::tr("Split With…"), "openSplitWith");
        addTabAction(QObject::tr("Focus Next Pane"), "nextPane");
        addTabAction(QObject::tr("Focus Previous Pane"), "previousPane");
        addTabAction(QObject::tr("Show Next Hidden Tab in Active Pane"), "nextHiddenTab");
        addTabAction(QObject::tr("Keep Only Active Pane"), "singlePane");
        QObject::connect(root, SIGNAL(shortcutRecordingChanged(bool)), tabsMenu, SLOT(setDisabled(bool)));
        auto *workspacesMenu = nativeMenu.addMenu(QObject::tr("Workspaces"));
        QObject::connect(workspacesMenu, &QMenu::aboutToShow, root, [root, workspacesMenu, &workspaces] {
            workspacesMenu->clear();
            auto *show = workspacesMenu->addAction(QObject::tr("Show Workspaces…"));
            QObject::connect(show, &QAction::triggered, root, [root] { QMetaObject::invokeMethod(root,"openWorkspaces"); });
            auto *next = workspacesMenu->addAction(QObject::tr("Next Workspace"));
            QObject::connect(next, &QAction::triggered, &workspaces, [&workspaces] { workspaces.cycle(1); });
            auto *previous = workspacesMenu->addAction(QObject::tr("Previous Workspace"));
            QObject::connect(previous, &QAction::triggered, &workspaces, [&workspaces] { workspaces.cycle(-1); });
            workspacesMenu->addSeparator();
            for(const auto &value : workspaces.choices()) {
                const auto space=value.toMap(); const auto id=space.value("id").toString();
                auto *action=workspacesMenu->addAction(space.value("name").toString().replace('&',"&&"));
                action->setMenuRole(QAction::NoRole); action->setCheckable(true); action->setChecked(id==workspaces.currentId());
                QObject::connect(action,&QAction::triggered,&workspaces,[&workspaces,id] { workspaces.activate(id); });
            }
        });
        QObject::connect(&app,&QGuiApplication::focusWindowChanged,workspacesMenu,[workspacesMenu](QWindow *window) {
            workspacesMenu->setEnabled(!window || !window->property("privateMode").toBool());
        });
        auto *historyMenu = nativeMenu.addMenu(QObject::tr("History"));
        auto *showHistory = historyMenu->addAction(QObject::tr("Show All History…"));
        const auto updateHistoryShortcut = [&keyBindings, showHistory] {
            showHistory->setShortcut(QKeySequence::fromString(keyBindings.bindings().value("history").toString(), QKeySequence::PortableText));
        };
        updateHistoryShortcut();
        QObject::connect(&keyBindings, &KeyBindings::bindingsChanged, showHistory, updateHistoryShortcut);
        QObject::connect(showHistory, &QAction::triggered, root, [root] { QMetaObject::invokeMethod(QGuiApplication::focusWindow() ? static_cast<QObject *>(QGuiApplication::focusWindow()) : root, "openHistory"); });
        auto *passwordsMenu = nativeMenu.addMenu(QObject::tr("Passwords"));
        auto *fillAction = passwordsMenu->addAction(QObject::tr("Fill Saved Login…"));
        QObject::connect(fillAction, &QAction::triggered, root, [root] {
            QMetaObject::invokeMethod(QGuiApplication::focusWindow() ? static_cast<QObject *>(QGuiApplication::focusWindow()) : root, "openPasswords");
        });
        auto *manageAction = passwordsMenu->addAction(QObject::tr("Manage Passwords…"));
        QObject::connect(manageAction, &QAction::triggered, root, [root] {
            QMetaObject::invokeMethod(QGuiApplication::focusWindow() ? static_cast<QObject *>(QGuiApplication::focusWindow()) : root, "openPasswordSettings");
        });
        QObject::connect(root, SIGNAL(shortcutRecordingChanged(bool)), passwordsMenu, SLOT(setDisabled(bool)));
        QObject::connect(root, SIGNAL(shortcutRecordingChanged(bool)), &nativeMenu, SLOT(setDisabled(bool)));
    }
#ifdef Q_OS_MACOS
    if (!engine.rootObjects().isEmpty()) {
        if (auto *window = qobject_cast<QWindow *>(engine.rootObjects().first()))
            configureMacWindow(window);
    }
#endif
    return app.exec();
}
