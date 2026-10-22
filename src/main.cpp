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
#include "browserapplication.h"
#include <QWindow>
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

    KeyBindings keyBindings;
    QMenuBar nativeMenu;
    QQmlApplicationEngine engine;
    // Keep Chromium's real platform/version tokens, without the embedding-toolkit
    // and application products that sites can mistake for an unsupported browser.
    QString userAgent = QWebEngineProfile::defaultProfile()->httpUserAgent();
    userAgent.remove(QRegularExpression(QStringLiteral("\\s+(?:QtWebEngine|Arbiter)/[^\\s]+")));
    const bool diagnostics = qEnvironmentVariableIsSet("ARBITER_DIAGNOSTICS");
    engine.setInitialProperties({{QStringLiteral("browserUserAgent"), userAgent},
                                 {QStringLiteral("keyBindings"), QVariant::fromValue(&keyBindings)},
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
        app.setUrlHandler([root](const QUrl &url) {
            QMetaObject::invokeMethod(root, "navigate", Q_ARG(QString, url.toString()));
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
    }
#ifdef Q_OS_MACOS
    if (!engine.rootObjects().isEmpty()) {
        if (auto *window = qobject_cast<QWindow *>(engine.rootObjects().first()))
            configureMacWindow(window);
    }
#endif
    return app.exec();
}
