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
#include "tabs.h"
#include "splitswapgesture.h"
#include "workspaces.h"
#include "webprofiles.h"
#include "passwordmanager.h"
#include "webauthncontroller.h"
#include "passkeycontroller.h"
#include "appearance.h"
#include "history.h"
#include "browserimporter.h"
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

    const QString settingsError=Settings::initialize();
    KeyBindings keyBindings;
    Appearance appearance;
    Tabs tabs;
    Workspaces workspaces(&tabs);
    WebProfiles webProfiles;
    History history;
    PasswordManager passwords;
    BrowserImporter importer(&passwords,&history);
    WebAuthnController webauthn;
    PasskeyController passkeys;
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
            QMetaObject::invokeMethod(root, "openExternalUrl", Q_ARG(QString, url.toString()));
            if (auto *window = qobject_cast<QWindow *>(root)) {
                if (window->visibility() == QWindow::Minimized)
                    window->showNormal();
                window->raise();
                window->requestActivate();
            }
        });
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
        auto *fillAction = passwordsMenu->addAction(QObject::tr("Fill or Save Login…"));
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
