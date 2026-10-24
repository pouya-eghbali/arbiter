#include "browsericons.h"
#include <QQuickImageProvider>
#include <QFileIconProvider>
#include <QFileInfo>
#include <QDir>
#include <QIcon>
#include <QMap>
#include <QPainter>
namespace {
class BrowserIcons final : public QQuickImageProvider {
public:
    BrowserIcons():QQuickImageProvider(QQuickImageProvider::Image) {}
    QImage requestImage(const QString &name,QSize *size,const QSize &requested) override {
        static const QMap<QString,QString> themes{{"Chrome","google-chrome"},{"Chromium","chromium"},{"Firefox","firefox"},
            {"Safari","safari"},{"Edge","microsoft-edge"},{"Brave","brave-browser"},{"Vivaldi","vivaldi"},{"Arc","arc"}};
        QIcon icon;
#ifdef Q_OS_MACOS
        static const QMap<QString,QString> apps{{"Chrome","Google Chrome"},{"Chromium","Chromium"},{"Firefox","Firefox"},
            {"Safari","Safari"},{"Edge","Microsoft Edge"},{"Brave","Brave Browser"},{"Vivaldi","Vivaldi"},{"Arc","Arc"}};
        if(apps.contains(name)) for(const auto &root:{QStringLiteral("/Applications/"),QDir::homePath()+"/Applications/",QStringLiteral("/System/Applications/")}) {
            const auto path=root+apps.value(name)+".app";
            if(QFileInfo::exists(path)) { icon=nativeBrowserAppIcon(QFileInfo(path).canonicalFilePath()); break; }
        }
#elif defined(Q_OS_WIN)
        static const QMap<QString,QString> apps{{"Chrome","Google/Chrome/Application/chrome.exe"},{"Chromium","Chromium/Application/chrome.exe"},
            {"Firefox","Mozilla Firefox/firefox.exe"},{"Edge","Microsoft/Edge/Application/msedge.exe"},{"Brave","BraveSoftware/Brave-Browser/Application/brave.exe"},{"Vivaldi","Vivaldi/Application/vivaldi.exe"}};
        if(apps.contains(name)) for(const auto &variable:{"LOCALAPPDATA","ProgramFiles","ProgramFiles(x86)"}) {
            const auto root=qEnvironmentVariable(variable);
            if(root.isEmpty()) continue;
            const auto path=root+"/"+apps.value(name);
            if(QFileInfo::exists(path)) { icon=QFileIconProvider().icon(QFileInfo(path)); break; }
        }
#else
        if(themes.contains(name)) icon=QIcon::fromTheme(themes.value(name));
        if(icon.isNull() && name=="Firefox") icon=QIcon::fromTheme("org.mozilla.firefox");
#endif
        const QSize target=requested.isValid() ? requested.boundedTo(QSize(256,256)) : QSize(96,96);
        auto result=icon.isNull() ? QImage() : icon.pixmap(target).toImage();
        if(result.isNull()) {
            result=QImage(target,QImage::Format_ARGB32_Premultiplied); result.fill(Qt::transparent);
            QPainter painter(&result); painter.setRenderHint(QPainter::Antialiasing);
            QFont font; font.setPixelSize(target.height()*0.6); font.setBold(true); painter.setFont(font);
            painter.setPen(QColor("#8793a6")); painter.drawText(result.rect(),Qt::AlignCenter,name.left(1).toUpper());
        }
        if(size) *size=result.size(); return result;
    }
};
}
QQuickImageProvider *createBrowserIconProvider() { return new BrowserIcons; }
