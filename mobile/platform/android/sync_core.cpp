#include "credentialsyncstate.h"
#include <QJsonDocument>
#include <jni.h>
namespace {
QString string(JNIEnv *env, jstring input) {
    if (!input)
        return {};
    auto chars = env->GetStringChars(input, nullptr);
    auto value = QString::fromUtf16(reinterpret_cast<const char16_t *>(chars), env->GetStringLength(input));
    env->ReleaseStringChars(input, chars);
    return value;
}
} // namespace
extern "C" JNIEXPORT jstring JNICALL Java_com_arbiter_mobile_BackgroundSync_merge(JNIEnv *env, jclass,
                                                                                  jstring local,
                                                                                  jstring remote) {
    auto a = string(env, local).toUtf8(), b = string(env, remote).toUtf8();
    if (a.size() > ArbiterSync::MaximumBytes || b.size() > ArbiterSync::MaximumBytes)
        return nullptr;
    ArbiterSync::State own, peer;
    QString error;
    if (!ArbiterSync::State::parse(QJsonDocument::fromJson(a).object(), &own, &error) ||
        !ArbiterSync::State::parse(QJsonDocument::fromJson(b).object(), &peer, &error) ||
        !own.merge(peer, &error))
        return nullptr;
    auto result = QString::fromUtf8(QJsonDocument(own.json()).toJson(QJsonDocument::Compact));
    return env->NewString(reinterpret_cast<const jchar *>(result.utf16()), result.size());
}
