#include "mobilemodel.h"
#include "mobileplatform.h"
#ifdef ARBITER_MOBILE_DESIGN_PREVIEW
#include "designpreview.h"
#include <QTemporaryDir>
#endif
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
    MobileModel browser;
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
    if (qEnvironmentVariableIsSet("ARBITER_UI_SMOKE")) {
        auto window = engine.rootObjects().first();
        auto timer = new QTimer(window);
        timer->setInterval(200);
        QObject::connect(timer, &QTimer::timeout, window, [window, timer, step = 0]() mutable {
            if (step++ >= 40) {
                qInfo("Native menu transition smoke test passed");
                timer->stop();
                qApp->quit();
                return;
            }
            if (step % 2) {
                qInfo("Opening native menu, iteration %d", step / 2);
                QMetaObject::invokeMethod(window, "open", Q_ARG(QVariant, QVariant("more")));
            } else window->setProperty("panel", "");
        });
        QTimer::singleShot(1200, timer, [timer] { timer->start(); });
    }
#endif
    const auto result = app.exec();
    MobilePlatform::shutdown();
    return result;
}
