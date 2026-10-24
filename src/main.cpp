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
#include "browserapplication.h"
#include "browserinstance.h"
#include <QStandardPaths>
#include "tabs.h"
#include "splitswapgesture.h"
#include "workspaces.h"
#include "webprofiles.h"
#include "passwordmanager.h"
#include "webauthncontroller.h"
#include "passkeycontroller.h"
#include "appearance.h"
#include "onboarding.h"
#include "systemintegration.h"
#include <QEventLoop>
#include "history.h"
#include "browserimporter.h"
#include "browserimportselection.h"
#include "browsericons.h"
#include "sitenavigation.h"
#include <QWindow>
#include <QFileInfo>
#ifdef Q_OS_MACOS
void configureMacWindow(QWindow *window);
#endif

int main(int argc, char *argv[])
{
    QtWebEngineQuick::initialize();
    BrowserApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("Arbiter"));
    QCoreApplication::setApplicationName(QStringLiteral("Arbiter"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.1.0"));
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
    KeyBindings keyBindings;
    Appearance appearance;
    Onboarding onboarding(&appearance);
    SystemIntegration systemIntegration;
    Tabs tabs;
    Workspaces workspaces(&tabs);
    WebProfiles webProfiles;
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
    engine.setInitialProperties({{QStringLiteral("browserUserAgent"), userAgent},
                                 {QStringLiteral("settingsFile"), Settings::configFile()},
                                 {QStringLiteral("settingsDirectoryUrl"), QUrl::fromLocalFile(QFileInfo(Settings::configFile()).absolutePath())},
                                 {QStringLiteral("settingsError"), settingsError},
                                 {QStringLiteral("appearance"), QVariant::fromValue(&appearance)},
                                 {QStringLiteral("onboarding"), QVariant::fromValue(&onboarding)},
                                 {QStringLiteral("windowDecorations"), appearance.windowDecorations()},
                                 {QStringLiteral("siteNavigation"), QVariant::fromValue(&siteNavigation)},
                                 {QStringLiteral("keyBindings"), QVariant::fromValue(&keyBindings)},
                                 {QStringLiteral("tabs"), QVariant::fromValue(&tabs)},
                                 {QStringLiteral("workspaces"), QVariant::fromValue(&workspaces)},
                                 {QStringLiteral("webProfiles"), QVariant::fromValue(&webProfiles)},
                                 {QStringLiteral("history"), QVariant::fromValue(&history)},
                                 {QStringLiteral("passwords"), QVariant::fromValue(&passwords)},
                                 {QStringLiteral("importer"), QVariant::fromValue(&importer)},
                                 {QStringLiteral("webauthn"), QVariant::fromValue(&webauthn)},
                                 {QStringLiteral("passkeys"), QVariant::fromValue(&passkeys)},
                                 {QStringLiteral("browserDiagnostics"), diagnostics}});
    if (diagnostics) {
        qInfo().noquote() << "Chromium:" << qWebEngineChromiumVersion();
        qInfo().noquote() << "User agent:" << userAgent;
    }
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
                     &app, [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.loadFromModule(QStringLiteral("Arbiter"), QStringLiteral("Main"));
    if (!engine.rootObjects().isEmpty()) {
        QObject *root = engine.rootObjects().first();
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
        auto *tabsMenu = nativeMenu.addMenu(QObject::tr("Tabs"));
        const auto addTabAction = [root, tabsMenu](const QString &label, const char *method) {
            auto *action = tabsMenu->addAction(label);
            QObject::connect(action, &QAction::triggered, root, [root, method] {
                QMetaObject::invokeMethod(root, method);
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
        auto *historyMenu = nativeMenu.addMenu(QObject::tr("History"));
        auto *showHistory = historyMenu->addAction(QObject::tr("Show All History…"));
        const auto updateHistoryShortcut = [&keyBindings, showHistory] {
            showHistory->setShortcut(QKeySequence::fromString(keyBindings.bindings().value("history").toString(), QKeySequence::PortableText));
        };
        updateHistoryShortcut();
        QObject::connect(&keyBindings, &KeyBindings::bindingsChanged, showHistory, updateHistoryShortcut);
        QObject::connect(showHistory, &QAction::triggered, root, [root] { QMetaObject::invokeMethod(root, "openHistory"); });
        auto *passwordsMenu = nativeMenu.addMenu(QObject::tr("Passwords"));
        auto *fillAction = passwordsMenu->addAction(QObject::tr("Fill Saved Login…"));
        QObject::connect(fillAction, &QAction::triggered, root, [root] {
            QMetaObject::invokeMethod(root, "openPasswords");
        });
        auto *manageAction = passwordsMenu->addAction(QObject::tr("Manage Passwords…"));
        QObject::connect(manageAction, &QAction::triggered, root, [root] {
            QMetaObject::invokeMethod(root, "openPasswordSettings");
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
