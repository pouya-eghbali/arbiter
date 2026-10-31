#include <windows.h>
#include <bcrypt.h>
#include <string>
#include <vector>
#ifdef ARBITER_PROBE_OPENSSL
#include <openssl/evp.h>
#endif
static_assert(sizeof(void *) == 8 && sizeof(long) == 4);
int main() {
    std::vector<std::wstring> values{L"Arbiter"};
    unsigned char random[16]{};
    if (BCryptGenRandom(nullptr, random, sizeof(random), BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0) return 1;
#ifdef ARBITER_PROBE_OPENSSL
    unsigned char digest[EVP_MAX_MD_SIZE]{};
    unsigned int length = 0;
    if (!EVP_Digest(random, sizeof(random), digest, &length, EVP_sha256(), nullptr)
        || length != 32) return 3;
#endif
    return GetModuleHandleW(nullptr) && values.front().size() == 7 ? 0 : 2;
}
