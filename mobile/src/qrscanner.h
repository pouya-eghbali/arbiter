#pragma once
#include <QElapsedTimer>
#include <QObject>
#include <QPointer>
#include <QVideoSink>
#include <QtQml/qqmlregistration.h>
class QRScanner : public QObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QVideoSink *videoSink READ videoSink WRITE setVideoSink NOTIFY videoSinkChanged)
    Q_PROPERTY(bool allowed READ allowed NOTIFY allowedChanged)
  public:
    using QObject::QObject;
    QVideoSink *videoSink() const {
        return m_sink;
    }
    void setVideoSink(QVideoSink *);
    bool allowed() const {
        return m_allowed;
    }
    Q_INVOKABLE void requestPermission();
  signals:
    void videoSinkChanged();
    void allowedChanged();
    void scanned(QString invitation);

  private:
    QPointer<QVideoSink> m_sink;
    QElapsedTimer m_last;
    bool m_working = false, m_allowed = false, m_found = false;
};
