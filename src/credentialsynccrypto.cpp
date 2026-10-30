#include "credentialsyncstate.h"
#include <memory>
#include <openssl/evp.h>
#include <openssl/rand.h>
namespace ArbiterSync {
QByteArray seal(const QByteArray &plain, const QByteArray &key, const QByteArray &context,
                const QByteArray &nonce) {
    if (key.size() != 32 || plain.size() > MaximumBytes || (!nonce.isEmpty() && nonce.size() != 12))
        return {};
    QByteArray iv = nonce;
    if (iv.isEmpty()) {
        iv.resize(12);
        if (RAND_bytes(reinterpret_cast<unsigned char *>(iv.data()), iv.size()) != 1)
            return {};
    }
    std::unique_ptr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)> c(EVP_CIPHER_CTX_new(),
                                                                      EVP_CIPHER_CTX_free);
    if (!c || EVP_EncryptInit_ex(c.get(), EVP_aes_256_gcm(), nullptr,
                                 reinterpret_cast<const unsigned char *>(key.constData()),
                                 reinterpret_cast<const unsigned char *>(iv.constData())) != 1)
        return {};
    int count = 0, total = 0;
    if (EVP_EncryptUpdate(c.get(), nullptr, &count,
                          reinterpret_cast<const unsigned char *>(context.constData()), context.size()) != 1)
        return {};
    QByteArray cipher(plain.size() + 16, Qt::Uninitialized), tag(16, Qt::Uninitialized);
    if (EVP_EncryptUpdate(c.get(), reinterpret_cast<unsigned char *>(cipher.data()), &count,
                          reinterpret_cast<const unsigned char *>(plain.constData()), plain.size()) != 1)
        return {};
    total = count;
    if (EVP_EncryptFinal_ex(c.get(), reinterpret_cast<unsigned char *>(cipher.data()) + total, &count) != 1)
        return {};
    cipher.resize(total + count);
    if (EVP_CIPHER_CTX_ctrl(c.get(), EVP_CTRL_GCM_GET_TAG, 16, tag.data()) != 1)
        return {};
    return iv + cipher + tag;
}
QByteArray open(const QByteArray &sealed, const QByteArray &key, const QByteArray &context, bool *ok) {
    *ok = false;
    if (key.size() != 32 || sealed.size() < 28 || sealed.size() > MaximumBytes + 28)
        return {};
    std::unique_ptr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)> c(EVP_CIPHER_CTX_new(),
                                                                      EVP_CIPHER_CTX_free);
    if (!c || EVP_DecryptInit_ex(c.get(), EVP_aes_256_gcm(), nullptr,
                                 reinterpret_cast<const unsigned char *>(key.constData()),
                                 reinterpret_cast<const unsigned char *>(sealed.constData())) != 1)
        return {};
    int count = 0, total = 0;
    if (EVP_DecryptUpdate(c.get(), nullptr, &count,
                          reinterpret_cast<const unsigned char *>(context.constData()), context.size()) != 1)
        return {};
    QByteArray plain(sealed.size() - 28 + 16, Qt::Uninitialized);
    if (EVP_DecryptUpdate(c.get(), reinterpret_cast<unsigned char *>(plain.data()), &count,
                          reinterpret_cast<const unsigned char *>(sealed.constData() + 12),
                          sealed.size() - 28) != 1)
        return {};
    total = count;
    QByteArray tag = sealed.right(16);
    if (EVP_CIPHER_CTX_ctrl(c.get(), EVP_CTRL_GCM_SET_TAG, 16, tag.data()) != 1 ||
        EVP_DecryptFinal_ex(c.get(), reinterpret_cast<unsigned char *>(plain.data()) + total, &count) != 1) {
        OPENSSL_cleanse(plain.data(), plain.size());
        return {};
    }
    plain.resize(total + count);
    *ok = true;
    return plain;
}
} // namespace ArbiterSync
