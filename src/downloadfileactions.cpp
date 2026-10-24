#include "downloadfileactions.h"
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QUuid>
#ifdef Q_OS_MACOS
#include <sys/xattr.h>
#endif

QString markDownloadedFile(const QString &path, const QUrl &source) {
#ifdef Q_OS_MACOS
    Q_UNUSED(source);
    const auto quarantine = QByteArray("0081;") + QByteArray::number(QDateTime::currentSecsSinceEpoch(),16)
        + ";Arbiter;" + QUuid::createUuid().toByteArray(QUuid::WithoutBraces);
    if (setxattr(QFile::encodeName(path).constData(), "com.apple.quarantine", quarantine.constData(), quarantine.size(), 0, 0) != 0)
        return QStringLiteral("Saved, but macOS download security metadata could not be written.");
#elif defined(Q_OS_WIN)
    QFile zone(path + QStringLiteral(":Zone.Identifier"));
    const auto bytes = QByteArray("[ZoneTransfer]\r\nZoneId=3\r\nHostUrl=") + source.adjusted(QUrl::RemoveUserInfo).toEncoded() + "\r\n";
    if (!zone.open(QIODevice::WriteOnly) || zone.write(bytes) != bytes.size())
        return QStringLiteral("Saved, but Windows download security metadata could not be written.");
#else
    Q_UNUSED(path); Q_UNUSED(source);
#endif
    return {};
}
bool revealDownloadedFile(const QString &path) {
#ifdef Q_OS_MACOS
    if (QFileInfo::exists(path)) return QProcess::startDetached(QStringLiteral("/usr/bin/open"), {QStringLiteral("-R"), path});
#elif defined(Q_OS_WIN)
    if (QFileInfo::exists(path)) return QProcess::startDetached(QStringLiteral("explorer.exe"), {QStringLiteral("/select,"), QDir::toNativeSeparators(path)});
#endif
    return QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath()));
}
