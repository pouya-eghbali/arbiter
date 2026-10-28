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
#include "webauthncontroller.h"
#include "passkeycontroller.h"
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
    // CI verifies the deployed WebEngine helper and QML plugins without opening
    // profiles, loading settings, touching credentials, or contacting a website.
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
    PasswordManager passwords;
    BrowserImporter importer(&passwords,&history);
    BrowserImportSelection importSelection(&importer);
    WebAuthnController webauthn;
    PasskeyController passkeys;
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
