#include "credentialsyncstate.h"
#include "mobileplatform.h"
namespace ArbiterSync {
QByteArray seal(const QByteArray &plain, const QByteArray &key, const QByteArray &context,
                const QByteArray &nonce) {
    if (!nonce.isEmpty())
        return {};
    return MobilePlatform::seal(plain, key, context);
}
QByteArray open(const QByteArray &data, const QByteArray &key, const QByteArray &context, bool *ok) {
    return MobilePlatform::open(data, key, context, ok);
}
} // namespace ArbiterSync
