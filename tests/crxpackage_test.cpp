#include "../src/crxpackage.h"
#include <QtTest>
#include <QBuffer>
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QUrlQuery>
#include <QtEndian>
#include <QtCore/private/qzipwriter_p.h>
#include <openssl/evp.h>
#include <openssl/x509.h>
#include <memory>

namespace {
using Key = std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)>;
QByteArray publicBytes(EVP_PKEY *key) {
    const int size = i2d_PUBKEY(key, nullptr); if (size <= 0) return {};
    QByteArray bytes(size, 0); auto *ptr = reinterpret_cast<unsigned char *>(bytes.data()); i2d_PUBKEY(key, &ptr); return bytes;
}
QByteArray varint(quint64 value) {
    QByteArray bytes;
    do { auto byte = quint8(value & 127); value >>= 7; bytes.append(char(byte | (value ? 128 : 0))); } while (value);
    return bytes;
}
QByteArray field(quint64 number, const QByteArray &value) { return varint((number << 3) | 2) + varint(value.size()) + value; }
QByteArray littleEndian(quint32 value) { QByteArray out(4, 0); qToLittleEndian(value, out.data()); return out; }
QByteArray signature(EVP_PKEY *key, const QByteArray &bytes) {
    std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> ctx(EVP_MD_CTX_new(), EVP_MD_CTX_free);
    if (!ctx || EVP_DigestSignInit(ctx.get(), nullptr, EVP_sha256(), nullptr, key) != 1
        || EVP_DigestSignUpdate(ctx.get(), bytes.constData(), bytes.size()) != 1) return {};
    size_t size = 0; if (EVP_DigestSignFinal(ctx.get(), nullptr, &size) != 1) return {};
    QByteArray sig(size, 0);
    if (EVP_DigestSignFinal(ctx.get(), reinterpret_cast<unsigned char *>(sig.data()), &size) != 1) return {};
    sig.resize(size); return sig;
}
QByteArray archive(const QString &extra = {}, bool symlink = false, int manifestVersion = 3) {
    QBuffer buffer; buffer.open(QIODevice::WriteOnly);
    QZipWriter zip(&buffer);
    zip.addFile("manifest.json", QByteArray("{\"manifest_version\":") + QByteArray::number(manifestVersion) + ",\"name\":\"Store test\",\"version\":\"1.0\",\"key\":\"untrusted manifest key\"}");
    zip.addDirectory("_metadata");
    zip.addFile("_metadata/test.json", "{}");
    zip.addFile("content.js", "document.body.dataset.storeTest='works'");
    if (!extra.isEmpty()) { if (symlink) zip.addSymLink(extra, "../outside"); else zip.addFile(extra, "test"); }
    zip.close(); return buffer.data();
}
QString encodedId(const QByteArray &bytes) {
    QString value; for (quint8 byte : bytes) { value += QChar('a' + (byte >> 4)); value += QChar('a' + (byte & 15)); } return value;
}
}
class CrxPackageTest : public QObject {
    Q_OBJECT
    Key developer{nullptr, EVP_PKEY_free}, publisher{nullptr, EVP_PKEY_free};
    QByteArray developerBytes, publisherBytes, pin;
    QString id;
    QByteArray package(const QByteArray &zip, bool developerProof = true, bool publisherProof = true, bool corruptPublisher = false) {
        const auto signedData = field(1, QCryptographicHash::hash(developerBytes, QCryptographicHash::Sha256).first(16));
        const auto data = QByteArray("CRX3 SignedData\0", 16) + littleEndian(signedData.size()) + signedData + zip;
        QByteArray header;
        if (developerProof) header += field(2, field(1, developerBytes) + field(2, signature(developer.get(), data)));
        if (publisherProof) {
            auto proof = signature(publisher.get(), data); if (corruptPublisher) proof[proof.size() - 1] ^= 1;
            header += field(3, field(1, publisherBytes) + field(2, proof));
        }
        header += field(10000, signedData);
        return "Cr24" + littleEndian(3) + littleEndian(header.size()) + header + zip;
    }
private slots:
    void initTestCase() {
        developer.reset(EVP_PKEY_Q_keygen(nullptr, nullptr, "RSA", size_t(2048)));
        publisher.reset(EVP_PKEY_Q_keygen(nullptr, nullptr, "EC", "prime256v1"));
        QVERIFY(developer); QVERIFY(publisher);
        developerBytes = publicBytes(developer.get()); publisherBytes = publicBytes(publisher.get());
        pin = QCryptographicHash::hash(publisherBytes, QCryptographicHash::Sha256);
        id = encodedId(QCryptographicHash::hash(developerBytes, QCryptographicHash::Sha256).first(16));
    }
    void storeLinksAndRedirects() {
        QCOMPARE(CrxPackage::storeId(id), id);
        QCOMPARE(CrxPackage::storeId("https://chromewebstore.google.com/detail/some-extension/" + id + "?hl=en"), id);
        QCOMPARE(CrxPackage::storeId("https://chromewebstore.google.com/detail/" + id), id);
        QCOMPARE(CrxPackage::storeId("https://chrome.google.com/webstore/detail/name/" + id), id);
        for (const auto &link : {"http://chromewebstore.google.com/detail/name/", "https://chromewebstore.google.com.evil.example/detail/name/", "https://user@chromewebstore.google.com/detail/name/", "https://chromewebstore.google.com:444/detail/name/", "https://chromewebstore.google.com/search/"}) QVERIFY(CrxPackage::storeId(link + id).isEmpty());
        QVERIFY(CrxPackage::storeId(QString(32, 'z')).isEmpty());
        const auto url = CrxPackage::downloadUrl(id, "140.0.0.0");
        QVERIFY(CrxPackage::allowedDownloadUrl(url));
        QCOMPARE(QUrlQuery(url).queryItemValue("acceptformat"), QString("crx3"));
        QCOMPARE(QUrlQuery(url).queryItemValue("x", QUrl::FullyDecoded), "id=" + id + "&installsource=ondemand&uc");
        QVERIFY(CrxPackage::allowedDownloadUrl(QUrl("https://clients2.googleusercontent.com/crx/package")));
        QVERIFY(CrxPackage::allowedDownloadUrl(QUrl("https://redirector.gvt1.com/crx/package")));
        QVERIFY(!CrxPackage::allowedDownloadUrl(QUrl("http://clients2.googleusercontent.com/crx/package")));
        QVERIFY(!CrxPackage::allowedDownloadUrl(QUrl("https://evilgoogleusercontent.com/package")));
        QVERIFY(!CrxPackage::allowedDownloadUrl(QUrl("https://clients2.googleusercontent.com.evil.example/package")));
        QVERIFY(!CrxPackage::allowedDownloadUrl(QUrl("https://localhost/package")));
    }
    void verifiesBothSignaturesAndIdentity() {
        const auto zip = archive(), bytes = package(zip);
        const auto verified = CrxPackage::verify(bytes, id, pin);
        QVERIFY2(verified.error.isEmpty(), qPrintable(verified.error));
        QCOMPARE(verified.archive, zip); QCOMPARE(verified.publicKey, developerBytes);
        QVERIFY(!CrxPackage::verify(bytes, QString(32, 'a'), pin).error.isEmpty());
        QVERIFY(!CrxPackage::verify(bytes, id, CrxPackage::publisherKeyHash()).error.isEmpty());
        QVERIFY(!CrxPackage::verify(package(zip, true, false), id, pin).error.isEmpty());
        QVERIFY(!CrxPackage::verify(package(zip, false, true), id, pin).error.isEmpty());
        QVERIFY(!CrxPackage::verify(package(zip, true, true, true), id, pin).error.isEmpty());
        auto corrupted = bytes; corrupted[corrupted.size() - 1] ^= 1;
        QVERIFY(!CrxPackage::verify(corrupted, id, pin).error.isEmpty());
        auto wrongVersion = bytes; wrongVersion[4] = 2;
        QVERIFY(!CrxPackage::verify(wrongVersion, id, pin).error.isEmpty());
        auto oversized = bytes; qToLittleEndian<quint32>(0x7fffffff, oversized.data() + 8);
        QVERIFY(!CrxPackage::verify(oversized, id, pin).error.isEmpty());
        QVERIFY(!CrxPackage::verify(bytes.first(10), id, pin).error.isEmpty());
    }
    void unpacksWithStableIdentity() {
        QTemporaryDir temporary;
        const auto verified = CrxPackage::verify(package(archive()), id, pin);
        QVERIFY2(CrxPackage::unpack(verified, temporary.path()).isEmpty(), qPrintable(verified.error));
        QFile manifest(temporary.filePath("manifest.json")); QVERIFY(manifest.open(QIODevice::ReadOnly));
        QCOMPARE(QJsonDocument::fromJson(manifest.readAll()).object().value("key").toString(), QString::fromLatin1(developerBytes.toBase64()));
        QVERIFY(QFileInfo::exists(temporary.filePath("content.js")));
    }
    void rejectsUnsafeArchives() {
        for (const auto &path : {"../escape", "/absolute", "folder/../../escape", "folder\\escape", "C:/escape", "MANIFEST.JSON", "folder/../escape"}) {
            QTemporaryDir temporary;
            const auto verified = CrxPackage::verify(package(archive(QString::fromUtf8(path))), id, pin);
            QVERIFY(verified.error.isEmpty());
            const auto extractionError = CrxPackage::unpack(verified, temporary.path());
            QVERIFY2(!extractionError.isEmpty(), path);
            QVERIFY2(QDir(temporary.path()).entryList(QDir::Files | QDir::NoDotAndDotDot).isEmpty(), path);
        }
        QTemporaryDir oversized;
        auto oversizedZip = archive();
        const auto central = oversizedZip.indexOf(QByteArray("PK\x01\x02", 4)); QVERIFY(central >= 0);
        qToLittleEndian<quint32>(65 * 1024 * 1024, oversizedZip.data() + central + 24);
        QVERIFY(!CrxPackage::unpack(CrxPackage::verify(package(oversizedZip), id, pin), oversized.path()).isEmpty());
        QVERIFY(QDir(oversized.path()).entryList(QDir::AllEntries | QDir::NoDotAndDotDot).isEmpty());
        QTemporaryDir symlink;
        QVERIFY(!CrxPackage::unpack(CrxPackage::verify(package(archive("link", true)), id, pin), symlink.path()).isEmpty());
        QTemporaryDir mv2;
        QVERIFY(!CrxPackage::unpack(CrxPackage::verify(package(archive({}, false, 2)), id, pin), mv2.path()).isEmpty());
    }
};
QTEST_GUILESS_MAIN(CrxPackageTest)
#include "crxpackage_test.moc"
