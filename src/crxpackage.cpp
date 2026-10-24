#include "crxpackage.h"
#include <QBuffer>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>
#include <QStringDecoder>
#include <QUrlQuery>
#include <QtEndian>
#include <QtCore/private/qzipreader_p.h>
#include <openssl/evp.h>
#include <openssl/x509.h>
#include <memory>

namespace {
struct Field { quint64 number; int wire; QByteArray bytes; };
bool varint(const QByteArray &data, qsizetype &pos, quint64 &value) {
    value = 0;
    for (int shift = 0; shift < 70 && pos < data.size(); shift += 7) {
        const auto byte = quint8(data[pos++]);
        if (shift == 63 && byte > 1) return false;
        value |= quint64(byte & 127) << shift;
        if (!(byte & 128)) return true;
    }
    return false;
}
bool fields(const QByteArray &data, QList<Field> &result) {
    qsizetype pos = 0;
    while (pos < data.size()) {
        quint64 tag, length;
        if (!varint(data, pos, tag) || !(tag >> 3) || tag >> 3 > 536870911 || result.size() >= 256) return false;
        Field field{tag >> 3, int(tag & 7), {}};
        switch (field.wire) {
        case 0: if (!varint(data, pos, length)) return false; break;
        case 1: if (data.size() - pos < 8) return false; pos += 8; break;
        case 2:
            if (!varint(data, pos, length) || length > quint64(data.size() - pos)) return false;
            field.bytes = data.mid(pos, qsizetype(length)); pos += qsizetype(length); break;
        case 5: if (data.size() - pos < 4) return false; pos += 4; break;
        default: return false;
        }
        result.append(std::move(field));
    }
    return true;
}
QByteArray singleBytes(const QList<Field> &fields, quint64 number) {
    QByteArray value; bool found = false;
    for (const auto &field : fields) if (field.number == number) {
        if (found || field.wire != 2) return {};
        value = field.bytes; found = true;
    }
    return value;
}
QString encodedId(const QByteArray &bytes) {
    QString id;
    for (quint8 byte : bytes) { id += QChar('a' + (byte >> 4)); id += QChar('a' + (byte & 15)); }
    return id;
}
QString validateZip(const QByteArray &bytes, QSet<QString> &directories) {
    // QZipReader normalizes filenames and some store ZIPs omit directory
    // attributes. Validate original central/local names before using it.
    qsizetype end = bytes.lastIndexOf(QByteArray("PK\x05\x06", 4));
    const auto u16 = [&](qsizetype at) { return qFromLittleEndian<quint16>(bytes.constData() + at); };
    const auto u32 = [&](qsizetype at) { return qFromLittleEndian<quint32>(bytes.constData() + at); };
    const auto invalid = QStringLiteral("The extension ZIP has invalid or unsupported metadata.");
    if (end < 0 || bytes.size() - end < 22 || end + 22 + u16(end + 20) != bytes.size()
        || u16(end + 4) || u16(end + 6) || u16(end + 8) != u16(end + 10)
        || !u16(end + 10) || u16(end + 10) > 10000) return invalid;
    const quint64 centralSize = u32(end + 12), centralOffset = u32(end + 16);
    if (centralOffset + centralSize != quint64(end)) return invalid; // No ZIP64/multidisk archives.
    qsizetype pos = qsizetype(centralOffset); QSet<QString> names; quint64 total = 0;
    for (int count = 0; count < u16(end + 10); ++count) {
        if (end - pos < 46 || bytes.mid(pos, 4) != QByteArray("PK\x01\x02", 4)) return invalid;
        const quint64 length = 46 + quint64(u16(pos + 28)) + u16(pos + 30) + u16(pos + 32);
        if (length > quint64(end - pos) || u16(pos + 34) || (u16(pos + 8) & 1)
            || (u16(pos + 10) != 0 && u16(pos + 10) != 8)) return invalid;
        const auto rawName = bytes.mid(pos + 46, u16(pos + 28));
        QStringDecoder decoder(QStringDecoder::Utf8); const QString name = decoder(rawName);
        if (decoder.hasError()) return invalid;
        const QString clean = QDir::cleanPath(name);
        const quint64 size = u32(pos + 24);
        const auto mode = u32(pos + 38) >> 16;
        if (name.isEmpty() || name.contains('\\') || name.contains(':') || name.contains(QChar::Null)
            || name.startsWith('/') || clean == ".." || clean.startsWith("../") || clean == "."
            || (clean != name && clean + '/' != name) || names.contains(clean.normalized(QString::NormalizationForm_C).toCaseFolded())
            || (mode & 0170000) == 0120000 || size > 64 * 1024 * 1024
            || (total += size) > 512 * 1024 * 1024)
            return QStringLiteral("The extension ZIP contains an unsafe path, link, duplicate, or oversized file.");
        names.insert(clean.normalized(QString::NormalizationForm_C).toCaseFolded());
        if (name.endsWith('/')) { if (size) return invalid; directories.insert(clean); }
        const quint64 local = u32(pos + 42);
        if (local + 30 > centralOffset || bytes.mid(local, 4) != QByteArray("PK\x03\x04", 4)
            || u16(local + 6) != u16(pos + 8) || u16(local + 8) != u16(pos + 10)) return invalid;
        const quint64 start = local + 30 + u16(local + 26) + u16(local + 28);
        if (start > centralOffset || start + u32(pos + 20) > centralOffset
            || bytes.mid(local + 30, u16(local + 26)) != rawName) return invalid;
        pos += qsizetype(length);
    }
    return pos == end ? QString() : invalid;
}
bool signatureValid(const QByteArray &key, const QByteArray &signature, bool rsa,
                    const QByteArray &prefix, const QByteArray &archive) {
    const auto *ptr = reinterpret_cast<const unsigned char *>(key.constData());
    std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> publicKey(d2i_PUBKEY(nullptr, &ptr, key.size()), EVP_PKEY_free);
    if (!publicKey || ptr != reinterpret_cast<const unsigned char *>(key.constData()) + key.size()) return false;
    const int type = EVP_PKEY_base_id(publicKey.get());
    if (rsa ? type != EVP_PKEY_RSA || EVP_PKEY_get_bits(publicKey.get()) < 1024
            : type != EVP_PKEY_EC || EVP_PKEY_get_bits(publicKey.get()) != 256) return false;
    if (!rsa) {
        char group[80]; size_t size = 0;
        if (!EVP_PKEY_get_group_name(publicKey.get(), group, sizeof(group), &size) || QByteArray(group) != "prime256v1") return false;
    }
    std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> context(EVP_MD_CTX_new(), EVP_MD_CTX_free);
    return context && EVP_DigestVerifyInit(context.get(), nullptr, EVP_sha256(), nullptr, publicKey.get()) == 1
        && EVP_DigestVerifyUpdate(context.get(), prefix.constData(), prefix.size()) == 1
        && EVP_DigestVerifyUpdate(context.get(), archive.constData(), archive.size()) == 1
        && EVP_DigestVerifyFinal(context.get(), reinterpret_cast<const unsigned char *>(signature.constData()), signature.size()) == 1;
}
}
namespace CrxPackage {
QString storeId(const QString &text) {
    static const QRegularExpression idPattern(QStringLiteral("^[a-p]{32}$"));
    const auto value = text.trimmed();
    if (idPattern.match(value).hasMatch()) return value;
    const QUrl url(value, QUrl::StrictMode);
    if (!url.isValid() || url.scheme() != "https" || !url.userInfo().isEmpty() || (url.port(-1) != -1 && url.port() != 443)) return {};
    const bool current = url.host() == "chromewebstore.google.com";
    const bool legacy = url.host() == "chrome.google.com";
    const auto parts = url.path().split('/', Qt::SkipEmptyParts);
    if (!(current && (parts.size() == 2 || parts.size() == 3) && parts[0] == "detail")
        && !(legacy && (parts.size() == 3 || parts.size() == 4) && parts[0] == "webstore" && parts[1] == "detail")) return {};
    return idPattern.match(parts.last()).hasMatch() ? parts.last() : QString();
}
QUrl downloadUrl(const QString &id, const QString &version) {
    if (storeId(id) != id) return {};
    QUrl url(QStringLiteral("https://clients2.google.com/service/update2/crx"));
    QUrlQuery query;
    query.addQueryItem("response", "redirect"); query.addQueryItem("prodversion", version);
    query.addQueryItem("acceptformat", "crx3"); query.addQueryItem("prod", "chromiumcrx");
    query.addQueryItem("x", "id=" + id + "&installsource=ondemand&uc");
    url.setQuery(query); return url;
}
bool allowedDownloadUrl(const QUrl &url) {
    if (!url.isValid() || url.scheme() != "https" || !url.userInfo().isEmpty() || (url.port(-1) != -1 && url.port() != 443)) return false;
    const auto host = url.host();
    return host == "clients2.google.com" || host == "clients2.googleusercontent.com"
        || host.endsWith(".googleusercontent.com") || host.endsWith(".gvt1.com") || host.endsWith(".gvt2.com");
}
QByteArray publisherKeyHash() {
    // Chromium components/crx_file/crx_verifier.cc: production Web Store key.
    return QByteArray::fromHex("61f7f2a6bfcf74cd0bc1fe2497cc9b04254c658f79f2145392867ea8366367cf");
}
Verified verify(const QByteArray &crx, const QString &id, const QByteArray &publisherHash) {
    const auto fail = [](const char *message) { return Verified{{}, {}, QString::fromUtf8(message)}; };
    if (crx.size() < 12 || crx.size() > MaximumDownload || crx.first(4) != "Cr24"
        || qFromLittleEndian<quint32>(crx.constData() + 4) != 3 || storeId(id) != id || publisherHash.size() != 32)
        return fail("The store returned an invalid CRX3 extension package.");
    const auto length = qFromLittleEndian<quint32>(crx.constData() + 8);
    if (length > 1024 * 1024 || length > quint64(crx.size() - 12)) return fail("Invalid CRX3 header size.");
    const auto header = crx.mid(12, length);
    QList<Field> parsed;
    if (!fields(header, parsed)) return fail("Invalid CRX3 header.");
    const auto signedData = singleBytes(parsed, 10000);
    QList<Field> signedFields;
    if (signedData.isEmpty() || !fields(signedData, signedFields)) return fail("Missing CRX3 signed data.");
    const auto binaryId = singleBytes(signedFields, 1);
    if (binaryId.size() != 16 || encodedId(binaryId) != id) return fail("The downloaded extension does not match the requested store item.");
    const auto archive = crx.mid(12 + length);
    if (!archive.startsWith("PK\x03\x04")) return fail("The CRX3 package has no ZIP archive.");
    QByteArray prefix("CRX3 SignedData\0", 16);
    char size[4]; qToLittleEndian<quint32>(signedData.size(), size); prefix.append(size, 4); prefix.append(signedData);
    bool publisher = false; QByteArray developerKey;
    int proofs = 0;
    for (const auto &field : parsed) if (field.number == 2 || field.number == 3) {
        QList<Field> proof;
        if (++proofs > 16 || field.wire != 2 || !fields(field.bytes, proof)) return fail("Invalid CRX3 signature proof.");
        const auto key = singleBytes(proof, 1), signature = singleBytes(proof, 2);
        if (key.isEmpty() || signature.isEmpty() || !signatureValid(key, signature, field.number == 2, prefix, archive))
            return fail("The extension package signature could not be verified.");
        const auto hash = QCryptographicHash::hash(key, QCryptographicHash::Sha256);
        if (hash.first(16) == binaryId) developerKey = key;
        if (hash == publisherHash) publisher = true;
    }
    if (developerKey.isEmpty() || !publisher) return fail("The package is missing a valid developer or Chrome Web Store signature.");
    return {archive, developerKey, {}};
}
QString unpack(const Verified &package, const QString &directory) {
    if (!package.error.isEmpty() || package.archive.isEmpty() || package.publicKey.isEmpty()) return QStringLiteral("Package verification is required before unpacking.");
    QSet<QString> directories;
    const auto zipError = validateZip(package.archive, directories);
    if (!zipError.isEmpty()) return zipError;
    QBuffer buffer; buffer.setData(package.archive); buffer.open(QIODevice::ReadOnly);
    QZipReader zip(&buffer);
    const auto files = zip.fileInfoList();
    if (zip.status() != QZipReader::NoError || files.isEmpty() || files.size() > 10000) return QStringLiteral("Invalid or oversized extension ZIP archive.");
    QSet<QString> names; qint64 total = 0;
    for (const auto &file : files) {
        const auto name = file.filePath;
        const auto clean = QDir::cleanPath(name);
        if (file.isSymLink || !file.isValid() || name.contains('\\') || name.contains(':') || name.contains(QChar::Null)
            || name.startsWith('/') || clean == ".." || clean.startsWith("../") || clean == "."
            || (clean != name && clean + '/' != name) || file.size < 0 || file.size > 64 * 1024 * 1024
            || (total += file.size) > 512 * 1024 * 1024 || names.contains(clean.normalized(QString::NormalizationForm_C).toCaseFolded()))
            return QStringLiteral("The extension ZIP contains an unsafe path, link, duplicate, or oversized file.");
        names.insert(clean.normalized(QString::NormalizationForm_C).toCaseFolded());
    }
    if (!names.contains("manifest.json")) return QStringLiteral("The extension is missing manifest.json.");
    for (const auto &file : files) {
        const auto destination = QDir(directory).filePath(file.filePath);
        if (file.isDir || directories.contains(QDir::cleanPath(file.filePath))) { if (!QDir().mkpath(destination)) return QStringLiteral("Could not create extension directory for %1.").arg(file.filePath); continue; }
        const auto bytes = zip.fileData(file.filePath);
        if (bytes.size() != file.size || zip.status() != QZipReader::NoError) return QStringLiteral("Could not extract an extension file.");
        if (!QDir().mkpath(QFileInfo(destination).absolutePath())) return QStringLiteral("Could not create extension directory for %1.").arg(file.filePath);
        QFile out(destination);
        if (!out.open(QIODevice::WriteOnly) || out.write(bytes) != bytes.size()) return QStringLiteral("Could not write an extension file.");
        out.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    }
    QFile file(QDir(directory).filePath("manifest.json"));
    if (!file.open(QIODevice::ReadOnly) || file.size() > 1024 * 1024) return QStringLiteral("Could not read the extension manifest.");
    const auto bytes = file.readAll(); file.close();
    QJsonParseError error; auto manifest = QJsonDocument::fromJson(bytes, &error).object();
    if (error.error != QJsonParseError::NoError || manifest.value("manifest_version").toInt() != 3)
        return QStringLiteral("This extension requires an unsupported manifest version. Arbiter supports Manifest V3.");
    // Qt loads the verified archive as unpacked. Preserve its store identity
    // instead of allowing Chromium to derive a new ID from the temporary path.
    manifest.insert("key", QString::fromLatin1(package.publicKey.toBase64()));
    const auto rewritten = QJsonDocument(manifest).toJson();
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate) || file.write(rewritten) != rewritten.size()) return QStringLiteral("Could not preserve the extension's store identity.");
    return {};
}
}
