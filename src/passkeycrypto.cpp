#include "passkeycrypto.h"
#include <QCryptographicHash>
#include <QCborValue>
#include <openssl/core_names.h>
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/x509.h>
#include <memory>

namespace {
template<class T, auto Free> using Handle = std::unique_ptr<T, decltype(Free)>;
}
namespace PasskeyCrypto {
void wipe(QByteArray &bytes) { if (!bytes.isEmpty()) OPENSSL_cleanse(bytes.data(), size_t(bytes.size())); bytes.clear(); }
QByteArray random(int size)
{
    QByteArray bytes(size, Qt::Uninitialized);
    if (size <= 0 || RAND_bytes(reinterpret_cast<unsigned char *>(bytes.data()), size) != 1) return {};
    return bytes;
}
QByteArray derive(const QByteArray &passphrase, const QByteArray &salt)
{
    if (salt.size() != 32 || passphrase.size() > 1024) return {};
    QByteArray key(32, Qt::Uninitialized);
    if (PKCS5_PBKDF2_HMAC(passphrase.constData(), int(passphrase.size()),
        reinterpret_cast<const unsigned char *>(salt.constData()), 32, 600000,
        EVP_sha256(), 32, reinterpret_cast<unsigned char *>(key.data())) != 1) { wipe(key); return {}; }
    return key;
}
QByteArray seal(const QByteArray &plain, const QByteArray &key, const QByteArray &context)
{
    if (key.size() != 32 || plain.size() > 8192) return {};
    auto nonce = random(12);
    if (nonce.size() != 12) return {};
    Handle<EVP_CIPHER_CTX, EVP_CIPHER_CTX_free> ctx(EVP_CIPHER_CTX_new(), EVP_CIPHER_CTX_free);
    if (!ctx) return {};
    QByteArray out(plain.size() + 16, Qt::Uninitialized), tag(16, Qt::Uninitialized);
    int n = 0, total = 0;
    if (EVP_EncryptInit_ex(ctx.get(), EVP_aes_256_gcm(), nullptr,
            reinterpret_cast<const unsigned char *>(key.constData()),
            reinterpret_cast<const unsigned char *>(nonce.constData())) != 1
        || EVP_EncryptUpdate(ctx.get(), nullptr, &n, reinterpret_cast<const unsigned char *>(context.constData()), int(context.size())) != 1
        || EVP_EncryptUpdate(ctx.get(), reinterpret_cast<unsigned char *>(out.data()), &n,
            reinterpret_cast<const unsigned char *>(plain.constData()), int(plain.size())) != 1) return {};
    total = n;
    if (EVP_EncryptFinal_ex(ctx.get(), reinterpret_cast<unsigned char *>(out.data()) + total, &n) != 1
        || EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_GET_TAG, 16, tag.data()) != 1) return {};
    out.resize(total + n);
    return nonce + out + tag;
}
QByteArray open(const QByteArray &sealed, const QByteArray &key, const QByteArray &context)
{
    if (key.size() != 32 || sealed.size() < 28 || sealed.size() > 8220) return {};
    Handle<EVP_CIPHER_CTX, EVP_CIPHER_CTX_free> ctx(EVP_CIPHER_CTX_new(), EVP_CIPHER_CTX_free);
    if (!ctx) return {};
    QByteArray out(sealed.size(), Qt::Uninitialized);
    auto tag = sealed.last(16);
    int n = 0, total = 0;
    bool ok = EVP_DecryptInit_ex(ctx.get(), EVP_aes_256_gcm(), nullptr,
            reinterpret_cast<const unsigned char *>(key.constData()),
            reinterpret_cast<const unsigned char *>(sealed.constData())) == 1
        && EVP_DecryptUpdate(ctx.get(), nullptr, &n, reinterpret_cast<const unsigned char *>(context.constData()), int(context.size())) == 1
        && EVP_DecryptUpdate(ctx.get(), reinterpret_cast<unsigned char *>(out.data()), &n,
            reinterpret_cast<const unsigned char *>(sealed.constData() + 12), int(sealed.size() - 28)) == 1;
    total = n;
    ok = ok && EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_TAG, 16, tag.data()) == 1
        && EVP_DecryptFinal_ex(ctx.get(), reinterpret_cast<unsigned char *>(out.data()) + total, &n) == 1;
    if (!ok) { wipe(out); return {}; }
    out.resize(total + n);
    return out;
}
Key generate()
{
    Handle<EVP_PKEY_CTX, EVP_PKEY_CTX_free> ctx(EVP_PKEY_CTX_new_from_name(nullptr, "EC", nullptr), EVP_PKEY_CTX_free);
    if (!ctx || EVP_PKEY_keygen_init(ctx.get()) != 1 || EVP_PKEY_CTX_set_group_name(ctx.get(), "prime256v1") != 1) return {};
    EVP_PKEY *raw = nullptr;
    if (EVP_PKEY_generate(ctx.get(), &raw) != 1) return {};
    Handle<EVP_PKEY, EVP_PKEY_free> key(raw, EVP_PKEY_free);
    unsigned char pub[65]; size_t length = sizeof(pub);
    if (EVP_PKEY_get_octet_string_param(key.get(), OSSL_PKEY_PARAM_PUB_KEY, pub, sizeof(pub), &length) != 1 || length != 65 || pub[0] != 4) return {};
    int size = i2d_PrivateKey(key.get(), nullptr);
    if (size <= 0) return {};
    QByteArray der(size, Qt::Uninitialized);
    auto *cursor = reinterpret_cast<unsigned char *>(der.data());
    if (i2d_PrivateKey(key.get(), &cursor) != size) { wipe(der); return {}; }
    QCborMap cose;
    cose.insert(1, 2); cose.insert(3, -7); cose.insert(-1, 1);
    cose.insert(-2, QByteArray(reinterpret_cast<char *>(pub + 1), 32));
    cose.insert(-3, QByteArray(reinterpret_cast<char *>(pub + 33), 32));
    return {der, QCborValue(cose).toCbor()};
}
QByteArray sign(const QByteArray &der, const QByteArray &message)
{
    const auto *cursor = reinterpret_cast<const unsigned char *>(der.constData());
    Handle<EVP_PKEY, EVP_PKEY_free> key(d2i_AutoPrivateKey(nullptr, &cursor, long(der.size())), EVP_PKEY_free);
    Handle<EVP_MD_CTX, EVP_MD_CTX_free> ctx(EVP_MD_CTX_new(), EVP_MD_CTX_free);
    if (!key || !ctx || cursor != reinterpret_cast<const unsigned char *>(der.constData()) + der.size()
        || EVP_DigestSignInit(ctx.get(), nullptr, EVP_sha256(), nullptr, key.get()) != 1) return {};
    size_t length = 0;
    if (EVP_DigestSign(ctx.get(), nullptr, &length, reinterpret_cast<const unsigned char *>(message.constData()), size_t(message.size())) != 1) return {};
    QByteArray signature(qsizetype(length), Qt::Uninitialized);
    if (EVP_DigestSign(ctx.get(), reinterpret_cast<unsigned char *>(signature.data()), &length,
            reinterpret_cast<const unsigned char *>(message.constData()), size_t(message.size())) != 1) return {};
    signature.resize(qsizetype(length));
    return signature;
}
QByteArray authenticatorData(const QString &rp, bool registration)
{
    auto result = QCryptographicHash::hash(rp.toUtf8(), QCryptographicHash::Sha256);
    // Software, device-bound authenticator: UP + verified passphrase UV. Never
    // claim backup eligibility/state. A constant zero counter is permitted by
    // WebAuthn for authenticators without a signature counter.
    result.append(char(registration ? 0x45 : 0x05));
    result.append(QByteArray(4, char(0)));
    return result;
}
}
