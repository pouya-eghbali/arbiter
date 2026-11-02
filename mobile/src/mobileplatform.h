#pragma once
#include <QByteArray>
#include <QColor>
#include <QJsonObject>
#include <QRectF>
#include <QUrl>
#include <functional>
class QWindow;
class NativePage;
namespace MobilePlatform {
void initialize();
void openSystemSettings();
void shutdown();
QString vaultDirectory();
QByteArray vaultKey(bool create);
QByteArray seal(const QByteArray &, const QByteArray &, const QByteArray &);
QByteArray open(const QByteArray &, const QByteArray &, const QByteArray &, bool *ok);
void verify(const QString &, std::function<void(bool)>);
void refreshIdentities();
void share(const QUrl &);
void secureScreen(bool);
void configureBackgroundSync(bool);
void setSystemAppearance(const QColor &);
void setLanguage(const QString &);
QString legacySession();
#ifdef Q_OS_ANDROID
void createWebViewAsync(NativePage *, const QString &profile, bool privateMode, std::function<void(QWindow *)>);
#else
QWindow *createWebView(NativePage *, const QString &profile, bool privateMode);
inline void createWebViewAsync(NativePage *page, const QString &profile, bool privateMode, std::function<void(QWindow *)> ready) {
    ready(createWebView(page, profile, privateMode));
}
#endif
void destroyWebView(NativePage *);
void navigate(NativePage *, const QUrl &);
void action(NativePage *, const QString &);
void evaluate(NativePage *, const QString &, std::function<void(QVariant)> = {});
void configurePage(NativePage *);
void setOverlayRect(NativePage *, const QRectF &normalized);
bool supportsObscuredInsets();
void setToolbarBottomInset(NativePage *, qreal);
void capture(NativePage *);
void setCoverProgress(NativePage *, qreal);
void find(NativePage *, const QString &, bool backwards);
void mediaPlaying(NativePage *, std::function<void(bool)>);
} // namespace MobilePlatform
