#include "mobilemodel.h"
#include "mobileplatform.h"
#ifdef ARBITER_MOBILE_DESIGN_PREVIEW
#include "designpreview.h"
#endif
#include <QTemporaryDir>
#include <QJsonArray>
#include <QJsonObject>
#include <QDesktopServices>
#include <QFile>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QLocale>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickImageProvider>
#include <QPainter>
#include <QTranslator>
#ifdef Q_OS_IOS
#include <TargetConditionals.h>
#include <QTimer>
#if TARGET_OS_SIMULATOR
#include <QtTest/QTest>
#include <QQuickItem>
#include <QQuickWindow>
#include "nativeglasshost.h"
extern QWindow *arbiter_ios_test_window_at(QWindow *, QPointF, QPoint *);
#endif
#endif
#ifdef ARBITER_MOBILE_WEBENGINE
#include <QtWebEngineQuick/qtwebenginequickglobal.h>
#endif
// CPU tinting preserves alpha and works in both native GPU and software previews.
class BrandImages : public QQuickImageProvider {
  public:
    BrandImages() : QQuickImageProvider(QQuickImageProvider::Image), source(":/brand/valkyrie-source.png") {}
    QImage requestImage(const QString &id, QSize *size, const QSize &requested) override {
        const auto target = requested.isValid() ? requested : QSize(96, 96);
        auto image = source.scaled(target, Qt::KeepAspectRatio, Qt::SmoothTransformation)
                         .convertToFormat(QImage::Format_ARGB32_Premultiplied);
        const QColor color(QUrl::fromPercentEncoding(id.toUtf8()));
        if (color.isValid()) {
            QPainter painter(&image);
            painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
            painter.fillRect(image.rect(), color);
        }
        if (size) *size = image.size();
        return image;
    }
  private:
    QImage source;
};
class Catalog : public QTranslator {
  public:
    QJsonObject strings;
    QString translate(const char *, const char *source, const char *, int) const override {
        return strings.value(QString::fromUtf8(source)).toString();
    }
};
int main(int argc, char **argv) {
#ifdef ARBITER_MOBILE_WEBENGINE
    QtWebEngineQuick::initialize();
#endif
    QGuiApplication app(argc, argv);
    app.setOrganizationName("Arbiter");
    app.setApplicationName("ArbiterMobile");
    app.setApplicationVersion(ARBITER_VERSION);
    QQuickStyle::setStyle("Basic");
    MobilePlatform::initialize();
#ifdef ARBITER_MOBILE_DESIGN_PREVIEW
    QTemporaryDir designDirectory;
    QString designScreen, designOutput;
    for (const auto &argument : app.arguments()) {
        if (argument.startsWith("--design-screen="))
            designScreen = argument.mid(16);
        if (argument.startsWith("--design-output="))
            designOutput = argument.mid(16);
    }
    const bool design = !designScreen.isEmpty() && !designOutput.isEmpty() && designDirectory.isValid();
    if (design)
        seedDesignPreview(designDirectory.path());
    MobileModel browser(nullptr, design ? designDirectory.path() : QString());
    if (design)
        browser.setProperty("_designPreview", true);
#else
#if defined(Q_OS_IOS) && TARGET_OS_SIMULATOR
    QTemporaryDir smokeProfile;
    const bool uiSmoke = qEnvironmentVariableIsSet("ARBITER_SUGGESTIONS_SMOKE");
    if (uiSmoke) {
        const QJsonObject profile{
            {"settings", QJsonObject{{"onboardingDone", true}, {"liquidGlass", true}, {"theme", "dark"}}},
            {"workspaces", QJsonArray{QJsonObject{{"id", "personal"}, {"name", "Personal"}, {"profile", "shared"}, {"color", "#8dbbff"}}}},
            {"workspaceId", "personal"}, {"activeId", "start"},
            {"tabs", QJsonArray{
                QJsonObject{{"id", "start"}, {"workspace", "personal"}, {"profile", "shared"}, {"url", ""}},
                QJsonObject{{"id", "target"}, {"workspace", "personal"}, {"profile", "shared"}, {"url", "https://tab.example.test/"}, {"title", "Target tab"}, {"sleeping", true}}}},
            {"history", QJsonArray{QJsonObject{{"url", "https://history.example.test/"}, {"title", "History target"}, {"time", 1}}}}};
        QFile file(smokeProfile.path() + "/browser-session-v1.json");
        if (!smokeProfile.isValid() || !file.open(QIODevice::WriteOnly)) qFatal("Cannot create isolated UI test profile");
        file.write(QJsonDocument(profile).toJson());
    }
    MobileModel browser(nullptr, uiSmoke ? smokeProfile.path() : QString());
#else
    MobileModel browser;
#endif
#endif
    QDesktopServices::setUrlHandler("http", &browser, "openUrl");
    QDesktopServices::setUrlHandler("https", &browser, "openUrl");
    QQmlApplicationEngine engine;
    engine.addImageProvider("brand", new BrandImages);
    Catalog catalog;
    app.installTranslator(&catalog);
    auto translate = [&] {
        const auto requested = browser.settings().value("language", "system").toString();
        QString code = requested;
        if (code == "system") {
            code = "en";
            for (const auto &language : QLocale::system().uiLanguages()) {
                const auto candidate = language.section('-', 0, 0).section('_', 0, 0);
                if (QFile::exists(":/i18n/" + candidate + ".json")) {
                    code = candidate;
                    break;
                }
            }
        }
        QFile file(":/i18n/" + code + ".json");
        catalog.strings = {};
        if (file.open(QIODevice::ReadOnly))
            catalog.strings = QJsonDocument::fromJson(file.readAll()).object();
        engine.retranslate();
    };
    translate();
    QObject::connect(&browser, &MobileModel::settingsChanged, &engine, translate);
    engine.rootContext()->setContextProperty("browser", &browser);
    engine.loadFromModule("Arbiter", "MobileMain");
    if (engine.rootObjects().isEmpty())
        return 1;
#ifdef ARBITER_MOBILE_DESIGN_PREVIEW
    if (design)
        renderDesignPreview(browser, engine, designScreen, designOutput);
#endif
#if defined(Q_OS_IOS) && TARGET_OS_SIMULATOR
    // Native-view visibility and snapshot transitions need a real iOS/Metal test.
    // This diagnostic entry point is never compiled into physical-device builds.
    if (qEnvironmentVariableIsSet("ARBITER_UI_SMOKE") || qEnvironmentVariableIsSet("ARBITER_SUGGESTIONS_SMOKE")) {
        auto window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
        QTimer::singleShot(1500, window, [window, &browser] {
            auto device = QTest::createTouchDevice();
            auto tap = [&](QPointF point) {
                QPoint local;
                auto target = arbiter_ios_test_window_at(window, point, &local);
                if (!target) qFatal("No UIKit hit target at %f,%f", point.x(), point.y());
                qInfo() << "UIKit touch" << point << "local" << local << "target" << target << target->geometry();
                QTest::touchEvent(target, device).press(0, local, target).commit();
                QTest::qWait(30);
                QTest::touchEvent(target, device).release(0, local, target).commit();
                QTest::qWait(150);
            };
            auto tapControl = [&](const char *name) {
                auto item = window->findChild<QQuickItem *>(name);
                if (!item || !item->window()) qFatal("Missing control: %s", name);
                const auto global = item->mapToGlobal(QPointF(item->width()/2, item->height()/2));
                qInfo() << "Control" << name << "visible" << item->isVisible() << "size" << item->size() << "window" << item->window() << item->window()->geometry();
                tap(window->mapFromGlobal(global));
            };
            auto chooseSuggestion = [&](const QString &tabId, const QString &url) {
                tapControl("mobileAddressButton");
                QTest::qWait(450);
                auto input = window->findChild<QQuickItem *>("mobileAddressInput");
                QQuickItem *row = nullptr;
                auto list = window->findChild<QQuickItem *>("mobileAddressSuggestions");
                auto content = list ? qobject_cast<QQuickItem *>(list->property("contentItem").value<QObject *>()) : nullptr;
                for (auto candidate : content ? content->childItems() : QList<QQuickItem *>()) {
                    const auto data = candidate->property("modelData").toMap();
                    if ((!tabId.isEmpty() && data.value("tab") == tabId) || (tabId.isEmpty() && data.value("tab").toString().isEmpty() && data.value("url") == url)) { row = candidate; break; }
                }
                if (!row || !input || !input->hasActiveFocus()) {
                    qInfo() << "Suggestion fixture" << "input" << input << "focused" << (input && input->hasActiveFocus()) << "list" << list << "content" << content << "suggestions" << browser.suggestions(QString());
                    qFatal("Missing focused address suggestion fixture");
                }
                const auto global = row->mapToGlobal(QPointF(row->width()/2, row->height()/2));
                QPoint local;
                auto target = arbiter_ios_test_window_at(window, window->mapFromGlobal(global), &local);
                if (!target) qFatal("Suggestion has no UIKit hit target");
                int focusResets = 0;
                const auto focusConnection = QObject::connect(qGuiApp, &QGuiApplication::focusWindowChanged, window, [target, &focusResets](QWindow *focused) { if (focused != target) ++focusResets; });
                // QTest bypasses QUIView::touchesBegan. Mirror its automatic
                // activation of the top-level ancestor before delivering touch.
                // Tooltip windows deliberately opt out of that UIKit behavior.
                if (target->type() != Qt::ToolTip && (target->type() != Qt::Popup || !target->isActive())) {
                    auto top = target;
                    while (top->parent()) top = top->parent();
                    if (top != qGuiApp->focusWindow()) top->requestActivate();
                }
                QTest::touchEvent(target, device).press(0, local, target).commit();
                QTest::qWait(180);
                if (!input->hasActiveFocus()) qFatal("Suggestion press stole address focus before release");
                QObject::disconnect(focusConnection);
                if (focusResets) qFatal("Suggestion press cycled native keyboard focus before selection");
                QTest::touchEvent(target, device).release(0, local, target).commit();
                QTest::qWait(250);
                if (window->property("panel") != "") qFatal("Suggestion tap did not dismiss the address sheet");
                if (!tabId.isEmpty() && browser.activeId() != tabId) qFatal("Switch-to-tab suggestion did not select its tab");
                if (tabId.isEmpty() && window->property("current").toMap().value("url") != url) qFatal("History suggestion did not navigate");
            };
            if (qEnvironmentVariableIsSet("ARBITER_SUGGESTIONS_SMOKE")) {
                chooseSuggestion("target", QString());
                chooseSuggestion(QString(), "https://history.example.test/");
                browser.navigate("https://tab.example.test/");
                browser.selectTab("start");
                browser.setSetting("liquidGlass", false);
                QTest::qWait(350);
                chooseSuggestion("target", QString());
                chooseSuggestion(QString(), "https://history.example.test/");
                browser.setSetting("liquidGlass", true);
                QTest::qWait(350);
            }
            tapControl("mobileAddressButton");
            if (window->property("panel") != "address") qFatal("Address sheet touch failed");
            QTest::qWait(400);
            auto address = window->findChild<QQuickItem *>("mobileAddressInput");
            if (!address || !address->hasActiveFocus()) {
                qInfo() << "Focus window" << qGuiApp->focusWindow() << "address window" << (address ? address->window() : nullptr);
                for (auto item = address; item; item = item->parentItem()) qInfo() << item << "focus" << item->hasFocus() << "active" << item->hasActiveFocus() << "enabled" << item->isEnabled() << "visible" << item->isVisible();
                qFatal("Native search lost input focus");
            }
            QTest::keyClick(address->window(), Qt::Key_A);
            if (address->property("text") != "a") qFatal("Native search typing failed");
            tapControl("mobileAddressCloseButton");
            if (window->property("panel") != "") qFatal("Address sheet close failed");
            if (qEnvironmentVariableIsSet("ARBITER_SUGGESTIONS_SMOKE")) {
                qInfo("History and switch-to-tab taps preserved keyboard focus and executed in native glass and Glacier; typing and dismissal passed");
                qApp->quit(); return;
            }
            QTest::qWait(300);
            tapControl("mobileTabsButton");
            tapControl("mobileWorkspaceSelectorButton");
            if (window->property("panel") != "workspaces") qFatal("Workspace selector touch failed");
            tapControl("mobileWorkspaceNewButton");
            auto editor = window->findChild<QObject *>("workspaceEditor");
            if (!editor || !editor->property("visible").toBool()) qFatal("Workspace editor failed to open");
            tapControl("mobileWorkspaceNameInput");
            QTest::qWait(400);
            auto field = window->findChild<QQuickItem *>("mobileWorkspaceNameInput");
            if (!field || !field->hasActiveFocus()) qFatal("Native editor lost input focus");
            QTest::keyClick(field->window(), Qt::Key_A);
            if (field->property("text") != "a") qFatal("Native editor typing failed");
            tapControl("mobileWorkspaceCancelButton");
            if (editor->property("visible").toBool()) qFatal("Native editor Cancel failed");
            QMetaObject::invokeMethod(window, "dismiss");
            QTest::qWait(300);
            for (int i = 0; i < 12; ++i) {
                qInfo("Native touch cycle %d: menu", i);
                tapControl("mobileMoreButton");
                if (window->property("panel") != "more") qFatal("Menu touch failed");
                if (i % 2) tapControl("mobileMenuCloseButton");
                else tap(QPointF(window->width()/2, 80));
                if (window->property("panel") != "") qFatal("Outside touch failed to dismiss menu");
                qInfo("Native touch cycle %d: tabs", i);
                tapControl("mobileTabsButton");
                if (window->property("panel") != "tabs") qFatal("Tabs touch failed");
                tapControl("mobileTabsDoneButton");
                if (window->property("panel") != "") qFatal("Tabs Done touch failed");
            }
            browser.newPrivateTab();
            auto confirmation = window->findChild<QObject *>("mobileHistoryConfirmation");
            QMetaObject::invokeMethod(confirmation, "open");
            QTest::qWait(350);
            auto content = qobject_cast<QQuickItem *>(confirmation->property("contentItem").value<QObject *>());
            QPointer<QQuickWindow> foreground = content ? content->window() : nullptr;
            if (!foreground || foreground == window || !foreground->isVisible()) qFatal("Native private dialog did not activate");
            qGuiApp->applicationStateChanged(Qt::ApplicationInactive);
            if (foreground && foreground->isVisible()) qFatal("Private cover did not immediately conceal the native dialog");
            QTest::qWait(200);
            qInfo("Native search, workspace editor, menu dismissal, tabs and private-dialog concealment checks passed");
            qApp->quit();
        });
    }
#endif
    const auto result = app.exec();
    MobilePlatform::shutdown();
    return result;
}
