#include "credentialsyncstate.h"
#include "mobilemodel.h"
#include "mobileplatform.h"
#include "nativepage.h"
#include <QDesktopServices>
#include <QDir>
#include <QEventLoop>
#include <QRandomGenerator>
#include <QStandardPaths>
#include <keychain.h>
namespace MobilePlatform {
void setCoverProgress(NativePage *, qreal) {}
void openSystemSettings() {}
void initialize() {}
void shutdown() {}
QString vaultDirectory() {
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/vault";
}
QByteArray vaultKey(bool create) {
    QEventLoop loop;
    QKeychain::ReadPasswordJob read("Arbiter Mobile Preview");
    read.setKey("arbiter-vault-key-v1");
    QObject::connect(&read, &QKeychain::Job::finished, &loop, &QEventLoop::quit);
    read.start();
    loop.exec();
    if (!read.error())
        return read.binaryData();
    if (!create || read.error() != QKeychain::EntryNotFound)
        return {};
    QByteArray key(32, Qt::Uninitialized);
    for (int i = 0; i < 32; ++i)
        key[i] = char(QRandomGenerator::system()->bounded(256));
    QKeychain::WritePasswordJob write("Arbiter Mobile Preview");
    write.setKey("arbiter-vault-key-v1");
    write.setBinaryData(key);
    QObject::connect(&write, &QKeychain::Job::finished, &loop, &QEventLoop::quit);
    write.start();
    loop.exec();
    return write.error() ? QByteArray() : key;
}
QByteArray seal(const QByteArray &data, const QByteArray &key, const QByteArray &aad) {
    return ArbiterSync::seal(data, key, aad);
}
QByteArray open(const QByteArray &data, const QByteArray &key, const QByteArray &aad, bool *ok) {
    return ArbiterSync::open(data, key, aad, ok);
}
void verify(const QString &, std::function<void(bool)> callback) {
    callback(false);
    if (MobileModel::instance)
        emit MobileModel::instance->message(QObject::tr("Use a phone to authenticate to the mobile vault."));
}
void refreshIdentities() {}
void share(const QUrl &url) {
    QDesktopServices::openUrl(
        QUrl("mailto:?body=" + QString::fromLatin1(QUrl::toPercentEncoding(url.toString()))));
}
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
void evaluate(NativePage *, const QString &, std::function<void(QVariant)> cb) {
    if (cb)
        cb({});
}
bool supportsObscuredInsets() { return false; }
void setToolbarBottomInset(NativePage *, qreal) {}
void setOverlayRect(NativePage *, const QRectF &) {}
void capture(NativePage *) {}
void find(NativePage *, const QString &, bool) {}
void configurePage(NativePage *) {}
void mediaPlaying(NativePage *, std::function<void(bool)> callback) {
    callback(false);
}
} // namespace MobilePlatform
