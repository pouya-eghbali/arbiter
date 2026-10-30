#include "qrscanner.h"
#include <Barcode.h>
#include <QCoreApplication>
#include <QImage>
#include <QThreadPool>
#include <QVideoFrame>
#include <QtCore/qpermissions.h>
#include <ReadBarcode.h>
#include <ReaderOptions.h>
void QRScanner::requestPermission() {
    QCameraPermission permission;
    if (qApp->checkPermission(permission) == Qt::PermissionStatus::Granted) {
        m_allowed = true;
        emit allowedChanged();
        return;
    }
    qApp->requestPermission(permission, this, [this](const QPermission &result) {
        m_allowed = result.status() == Qt::PermissionStatus::Granted;
        emit allowedChanged();
    });
}
void QRScanner::setVideoSink(QVideoSink *sink) {
    if (m_sink == sink)
        return;
    if (m_sink)
        disconnect(m_sink, nullptr, this, nullptr);
    m_sink = sink;
    emit videoSinkChanged();
    if (!sink)
        return;
    connect(sink, &QVideoSink::videoFrameChanged, this, [this](const QVideoFrame &frame) {
        if (m_working || m_found || (m_last.isValid() && m_last.elapsed() < 200))
            return;
        auto image = frame.toImage();
        if (image.isNull())
            return;
        if (image.width() > 1280 || image.height() > 1280)
            image = image.scaled(1280, 1280, Qt::KeepAspectRatio, Qt::FastTransformation);
        image = image.convertToFormat(QImage::Format_Grayscale8);
        m_working = true;
        m_last.start();
        QPointer<QRScanner> self(this);
        QThreadPool::globalInstance()->start([self, image] {
            auto result = ZXing::ReadBarcode(
                ZXing::ImageView(image.constBits(), image.width(), image.height(), ZXing::ImageFormat::Lum,
                                 image.bytesPerLine()),
                ZXing::ReaderOptions().setFormats(ZXing::BarcodeFormat::QRCode).setTryHarder(true));
            const auto text = result.isValid() ? QString::fromStdString(result.text()) : QString();
            QMetaObject::invokeMethod(qApp, [self, text] {
                if (!self)
                    return;
                self->m_working = false;
                if (text.startsWith("arbiter-pair://v1?") && text.size() <= 16000) {
                    self->m_found = true;
                    emit self->scanned(text);
                }
            });
        });
    });
}
