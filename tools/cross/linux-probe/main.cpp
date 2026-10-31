#include <QCoreApplication>
#include <QFile>
#include <thread>
#include <vector>
#include <nss.h>
#include <openssl/evp.h>
extern "C" {
#include <libavformat/avformat.h>
}
#if !defined(__linux__) || !defined(__x86_64__)
#error This probe must target Linux x64
#endif
static_assert(sizeof(void *) == 8 && sizeof(long) == 8);
class Probe : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
};
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    Probe probe;
    std::vector<int> values{1, 2};
    std::thread worker([&] { values.push_back(3); });
    worker.join();
    unsigned char digest[EVP_MAX_MD_SIZE]{};
    unsigned int length = 0;
    if (!EVP_Digest("Arbiter", 7, digest, &length, EVP_sha256(), nullptr)) return 1;
    if (NSS_NoDB_Init(nullptr) != SECSuccess) return 2;
    NSS_Shutdown();
    return !avformat_version() || !QFile::exists(":/main.cpp") || values.size() != 3
        || probe.metaObject()->className()[0] == '\0';
}
#include "main.moc"
