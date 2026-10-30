#pragma once
#include <QByteArray>
#include <QJsonObject>
#include <QUrl>
#include <functional>
class QWindow;
class NativePage;
namespace MobilePlatform {
void initialize();
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
void setLanguage(const QString &);
QString legacySession();
QWindow *createWebView(NativePage *, const QString &profile, bool privateMode);
void destroyWebView(NativePage *);
void navigate(NativePage *, const QUrl &);
void action(NativePage *, const QString &);
void evaluate(NativePage *, const QString &, std::function<void(QVariant)> = {});
void configurePage(NativePage *);
void mediaPlaying(NativePage *, std::function<void(bool)>);
} // namespace MobilePlatform
