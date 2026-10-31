#include "credentialsyncstate.h"
#include "mobileplatform.h"
#include <QFile>
namespace TestPlatform {
QString directory;
bool approved = false;
int verifications = 0;
} // namespace TestPlatform
namespace MobilePlatform {
void initialize() {}
void openSystemSettings() {}
void shutdown() {}
QString vaultDirectory() {
    return TestPlatform::directory;
}
QByteArray vaultKey(bool create) {
    if (create || QFile::exists(TestPlatform::directory + "/credentials-v1.enc"))
        return QByteArray(32, 'x');
    return {};
}
QByteArray seal(const QByteArray &a, const QByteArray &b, const QByteArray &c) {
    return ArbiterSync::seal(a, b, c);
}
QByteArray open(const QByteArray &a, const QByteArray &b, const QByteArray &c, bool *ok) {
    return ArbiterSync::open(a, b, c, ok);
}
void verify(const QString &, std::function<void(bool)> callback) {
    ++TestPlatform::verifications;
    callback(TestPlatform::approved);
}
void refreshIdentities() {}
void share(const QUrl &) {}
void secureScreen(bool) {}
void configureBackgroundSync(bool) {}
void setSystemAppearance(const QColor &) {}
void setLanguage(const QString &) {}
QString legacySession() {
    return {};
}
QWindow *createWebView(NativePage *, const QString &, bool) {
    return nullptr;
}
void destroyWebView(NativePage *) {}
void navigate(NativePage *, const QUrl &) {}
void action(NativePage *, const QString &) {}
void evaluate(NativePage *, const QString &, std::function<void(QVariant)> callback) {
    if (callback)
        callback(false);
}
void setOverlayRect(NativePage *, const QRectF &) {}
void capture(NativePage *) {}
void find(NativePage *, const QString &, bool) {}
void configurePage(NativePage *) {}
void mediaPlaying(NativePage *, std::function<void(bool)> callback) {
    callback(false);
}
} // namespace MobilePlatform
