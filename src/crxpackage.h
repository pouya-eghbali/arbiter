#pragma once
#include <QByteArray>
#include <QString>
#include <QUrl>

namespace CrxPackage {
inline constexpr qsizetype MaximumDownload = 64 * 1024 * 1024;
struct Verified {
    QByteArray archive;
    QByteArray publicKey;
    QString error;
};
QString storeId(const QString &linkOrId);
QUrl downloadUrl(const QString &id, const QString &chromiumVersion);
bool allowedDownloadUrl(const QUrl &url);
QByteArray publisherKeyHash();
// The caller supplies its trusted publisher pin; production uses the Chromium
// Web Store publisher pin above. Tests can sign their own package fixtures.
Verified verify(const QByteArray &crx, const QString &id, const QByteArray &publisherHash);
QString unpack(const Verified &package, const QString &directory);
}
