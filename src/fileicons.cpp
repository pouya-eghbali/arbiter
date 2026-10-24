#include "fileicons.h"
#include <QBuffer>
#include <QCache>
#include <QFileInfo>
#include <QIcon>
#include <QMimeDatabase>
#include <QPainter>
#ifdef Q_OS_WIN
#include <QFileIconProvider>
#endif

QUrl fileTypeIcon(const QString &fileName)
{
    // Cache by type, never by destination: do not read downloaded content or
    // probe a remote/missing destination merely to draw its icon.
    static QCache<QString, QUrl> cache(128);
    QString extension = QFileInfo(fileName).suffix().toLower();
    if (extension.size() > 64) extension.clear();
    if (auto *cached = cache.object(extension)) return *cached;
    QImage image;
#ifdef Q_OS_MACOS
    image = nativeFileTypeIcon(extension);
#elif defined(Q_OS_WIN)
    QFileIconProvider provider;
    image = provider.icon(QFileInfo("arbiter-file-icon." + extension)).pixmap(96, 96).toImage();
#else
    const auto mime = QMimeDatabase().mimeTypeForFile("file." + extension, QMimeDatabase::MatchExtension);
    QIcon icon = QIcon::fromTheme(mime.iconName());
    if (icon.isNull()) icon = QIcon::fromTheme(mime.genericIconName());
    if (icon.isNull()) icon = QIcon::fromTheme("text-x-generic");
    image = icon.pixmap(96, 96).toImage();
#endif
    if (image.isNull()) {
        image = QImage(96, 96, QImage::Format_ARGB32_Premultiplied); image.fill(Qt::transparent);
        QPainter painter(&image); painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(QPen(QColor("#8793a6"), 5));
        painter.drawRoundedRect(QRectF(22, 12, 52, 72), 6, 6);
        painter.drawLine(33, 42, 63, 42); painter.drawLine(33, 55, 63, 55);
    }
    QByteArray bytes; QBuffer buffer(&bytes); buffer.open(QIODevice::WriteOnly); image.save(&buffer, "PNG");
    const QUrl result(QStringLiteral("data:image/png;base64,") + QString::fromLatin1(bytes.toBase64()));
    cache.insert(extension, new QUrl(result));
    return result;
}
