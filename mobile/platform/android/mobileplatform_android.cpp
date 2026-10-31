#include "mobilemodel.h"
#include "mobileplatform.h"
#include "nativepage.h"
#include <QGuiApplication>
#include <QJniEnvironment>
#include <QJniObject>
#include <QJsonDocument>
#include <QPointer>
#include <QTimer>
#include <atomic>
namespace {
std::atomic<bool> acceptingEvents{true};
constexpr auto bridge = "com/arbiter/mobile/NativeBridge";
std::function<void(bool)> verification;
jbyteArray bytes(const QByteArray &data) {
    QJniEnvironment env;
    auto array = env->NewByteArray(data.size());
    env->SetByteArrayRegion(array, 0, data.size(), reinterpret_cast<const jbyte *>(data.constData()));
    return array;
}
QByteArray data(const QJniObject &object) {
    if (!object.isValid())
        return {};
    QJniEnvironment env;
    auto array = object.object<jbyteArray>();
    QByteArray value(env->GetArrayLength(array), Qt::Uninitialized);
    env->GetByteArrayRegion(array, 0, value.size(), reinterpret_cast<jbyte *>(value.data()));
    return value;
}
void nativeEvent(JNIEnv *env, jclass, jstring key, jstring type, jstring value) {
    if (!acceptingEvents || !qApp)
        return;
    auto id = QJniObject(key).toString(), name = QJniObject(type).toString(),
         json = QJniObject(value).toString();
    QMetaObject::invokeMethod(qApp, [id, name, json] {
        auto data = QJsonDocument::fromJson(json.toUtf8()).object().toVariantMap();
        if (name.startsWith("download")) {
            if (MobileModel::instance)
                MobileModel::instance->downloadEvent(name, data, data.value("private").toBool());
        } else if (auto page = NativePage::pages.value(id))
            page->event(name, data);
    });
}
void nativeVerified(JNIEnv *, jclass, jboolean approved) {
    if (!acceptingEvents || !qApp)
        return;
    QMetaObject::invokeMethod(qApp, [approved] {
        auto callback = std::move(verification);
        verification = {};
        if (callback)
            callback(approved);
        QTimer::singleShot(500, qApp, [] {
            if (MobileModel::instance)
                MobileModel::instance->verificationActive = false;
        });
    });
}
void nativeOpenUrl(JNIEnv *, jclass, jstring text) {
    if (!acceptingEvents || !qApp)
        return;
    auto url = QJniObject(text).toString();
    QMetaObject::invokeMethod(qApp, [url] {
        if (MobileModel::instance)
            MobileModel::instance->navigate(url);
    });
}
jboolean nativeBlocked(JNIEnv *, jclass, jstring url, jstring source, jstring kind, jstring method) {
    return MobileModel::filterRequest(QUrl(QJniObject(url).toString()), QUrl(QJniObject(source).toString()),
                                      QJniObject(kind).toString().toUtf8(),
                                      QJniObject(method).toString().toUtf8());
}
} // namespace
namespace MobilePlatform {
void setCoverProgress(NativePage *, qreal) {}
void openSystemSettings() { QJniObject::callStaticMethod<void>(bridge, "appSettings"); }
void initialize() {
    QJniEnvironment env;
    JNINativeMethod methods[] = {
        {const_cast<char *>("event"),
         const_cast<char *>("(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V"),
         reinterpret_cast<void *>(nativeEvent)},
        {const_cast<char *>("verified"), const_cast<char *>("(Z)V"),
         reinterpret_cast<void *>(nativeVerified)},
        {const_cast<char *>("openUrl"), const_cast<char *>("(Ljava/lang/String;)V"),
         reinterpret_cast<void *>(nativeOpenUrl)},
        {const_cast<char *>("blocked"),
         const_cast<char *>("(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)Z"),
         reinterpret_cast<void *>(nativeBlocked)}};
    env.registerNativeMethods(bridge, methods, 4);
    QJniObject::callStaticMethod<void>(bridge, "initialize");
    QTimer::singleShot(0, qApp, [] {
        auto model = MobileModel::instance;
        if (!model)
            return;
        QObject::connect(model, &MobileModel::defaultBrowserRequested, model,
                         [] { QJniObject::callStaticMethod<void>(bridge, "defaultBrowser"); });
        QObject::connect(model, &MobileModel::credentialProviderRequested, model,
                         [] { QJniObject::callStaticMethod<void>(bridge, "credentialProvider"); });
    });
}
void shutdown() {
    acceptingEvents = false;
    QJniObject::callStaticMethod<void>(bridge, "shutdown");
}
QString vaultDirectory() {
    return QJniObject::callStaticObjectMethod(bridge, "directory", "()Ljava/lang/String;").toString();
}
QByteArray vaultKey(bool create) {
    return data(QJniObject::callStaticObjectMethod(bridge, "vaultKey", "(Z)[B", jboolean(create)));
}
QByteArray crypt(const QByteArray &input, const QByteArray &key, const QByteArray &aad, bool decrypt) {
    QJniEnvironment env;
    auto a = bytes(input), b = bytes(key), c = bytes(aad);
    auto output =
        QJniObject::callStaticObjectMethod(bridge, "crypt", "([B[B[BZ)[B", a, b, c, jboolean(decrypt));
    env->DeleteLocalRef(a);
    env->DeleteLocalRef(b);
    env->DeleteLocalRef(c);
    return data(output);
}
QByteArray seal(const QByteArray &a, const QByteArray &b, const QByteArray &c) {
    return crypt(a, b, c, false);
}
QByteArray open(const QByteArray &a, const QByteArray &b, const QByteArray &c, bool *ok) {
    auto value = crypt(a, b, c, true);
    *ok = !value.isEmpty();
    return value;
}
void verify(const QString &reason, std::function<void(bool)> callback) {
    if (verification) {
        callback(false);
        return;
    }
    verification = std::move(callback);
    if (MobileModel::instance)
        MobileModel::instance->verificationActive = true;
    QJniObject::callStaticMethod<void>(bridge, "verify", "(Ljava/lang/String;)V",
                                       QJniObject::fromString(reason).object<jstring>());
}
void refreshIdentities() {
    QJniObject::callStaticMethod<void>(bridge, "refreshIdentities");
}
void share(const QUrl &url) {
    QJniObject::callStaticMethod<void>(bridge, "share", "(Ljava/lang/String;)V",
                                       QJniObject::fromString(url.toString()).object<jstring>());
}
void secureScreen(bool value) {
    QJniObject::callStaticMethod<void>(bridge, "secure", "(Z)V", jboolean(value));
}
void configureBackgroundSync(bool value) {
    QJniObject::callStaticMethod<void>(bridge, "background", "(Z)V", jboolean(value));
}
void setSystemAppearance(const QColor &) {}
void setLanguage(const QString &code) {
    QJniObject::callStaticMethod<void>(bridge, "language", "(Ljava/lang/String;)V",
                                       QJniObject::fromString(code).object<jstring>());
}
QString legacySession() {
    return {};
}
QWindow *createWebView(NativePage *page, const QString &profile, bool privateMode) {
    auto object = QJniObject::callStaticObjectMethod(
        bridge, "create", "(Ljava/lang/String;Ljava/lang/String;ZLjava/lang/String;)Landroid/webkit/WebView;",
        QJniObject::fromString(page->tabId).object<jstring>(),
        QJniObject::fromString(profile).object<jstring>(), jboolean(privateMode),
        QJniObject::fromString(MobileModel::instance->formScript(privateMode)).object<jstring>());
    if (!object.isValid()) {
        QJniEnvironment env;
        env.checkAndClearExceptions();
        page->event("error",
                    {{"message", QObject::tr("Update Android System WebView to use isolated sessions.")}});
        return nullptr;
    }
    auto owned = new QJniObject(object);
    page->handle = owned;
    return QWindow::fromWinId(reinterpret_cast<WId>(owned->object()));
}
void destroyWebView(NativePage *page) {
    QJniObject::callStaticMethod<void>(bridge, "destroy", "(Ljava/lang/String;)V",
                                       QJniObject::fromString(page->tabId).object<jstring>());
    delete static_cast<QJniObject *>(page->handle);
    page->handle = nullptr;
}
void navigate(NativePage *page, const QUrl &url) {
    if (!QStringList{"http", "https"}.contains(url.scheme()))
        return;
    QJniObject::callStaticMethod<void>(bridge, "navigate", "(Ljava/lang/String;Ljava/lang/String;)V",
                                       QJniObject::fromString(page->tabId).object<jstring>(),
                                       QJniObject::fromString(url.toString()).object<jstring>());
}
void action(NativePage *page, const QString &name) {
    QJniObject::callStaticMethod<void>(bridge, "action", "(Ljava/lang/String;Ljava/lang/String;)V",
                                       QJniObject::fromString(page->tabId).object<jstring>(),
                                       QJniObject::fromString(name).object<jstring>());
}
void evaluate(NativePage *page, const QString &script, std::function<void(QVariant)> callback) {
    if (callback)
        QJniObject::callStaticMethod<void>(bridge, "inspect", "(Ljava/lang/String;)V",
                                           QJniObject::fromString(page->tabId).object<jstring>());
    else
        QJniObject::callStaticMethod<void>(bridge, "evaluate", "(Ljava/lang/String;Ljava/lang/String;)V",
                                           QJniObject::fromString(page->tabId).object<jstring>(),
                                           QJniObject::fromString(script).object<jstring>());
}
void mediaPlaying(NativePage *, std::function<void(bool)> callback) {
    callback(!QJniObject::callStaticMethod<jboolean>(bridge, "canSleep"));
}
void setOverlayRect(NativePage *page, const QRectF &rect) {
    QJniObject::callStaticMethod<void>(
        bridge, "overlay", "(Ljava/lang/String;FFFF)V", QJniObject::fromString(page->tabId).object<jstring>(),
        jfloat(rect.x()), jfloat(rect.y()), jfloat(rect.width()), jfloat(rect.height()));
}
void capture(NativePage *page) {
    QJniObject::callStaticMethod<void>(bridge, "capture", "(Ljava/lang/String;)V",
                                       QJniObject::fromString(page->tabId).object<jstring>());
}
void find(NativePage *page, const QString &text, bool backwards) {
    QJniObject::callStaticMethod<void>(bridge, "find", "(Ljava/lang/String;Ljava/lang/String;Z)V",
                                       QJniObject::fromString(page->tabId).object<jstring>(),
                                       QJniObject::fromString(text).object<jstring>(), jboolean(backwards));
}
void configurePage(NativePage *page) {
    auto model = MobileModel::instance;
    if (!model)
        return;
    auto prefs =
        QJsonDocument(QJsonObject{{"savePasswords",
                                   model->settings().value("savePasswords").toBool() && !page->privateMode},
                                  {"suggestPasswords", model->settings().value("suggestPasswords").toBool()}})
            .toJson(QJsonDocument::Compact);
    QJniObject::callStaticMethod<void>(
        bridge, "configure", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V",
        QJniObject::fromString(page->tabId).object<jstring>(),
        QJniObject::fromString(model->formScript(page->privateMode)).object<jstring>(),
        QJniObject::fromString(QString::fromUtf8(prefs)).object<jstring>());
}
} // namespace MobilePlatform
