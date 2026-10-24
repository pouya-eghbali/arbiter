#pragma once
#include <QObject>
#include <QPointer>
#include <QWindow>
#include <QtQml/qqmlregistration.h>
#include <memory>
class QPrinter;
class QPrintDialog;

// Uses a transient native dialog, with Chromium's print PDF kept in memory.
class PagePrinter : public QObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QWindow *host MEMBER m_host)
    Q_PROPERTY(QObject *owner READ owner NOTIFY stateChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY stateChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
public:
    explicit PagePrinter(QObject *parent = nullptr) : QObject(parent) {}
    ~PagePrinter() override;
    QObject *owner() const { return m_owner; }
    bool busy() const { return m_busy; }
    QString error() const { return m_error; }
    Q_INVOKABLE void show(QObject *owner, const QString &title);
    Q_INVOKABLE void submit(int request, const QByteArray &pdf);
    Q_INVOKABLE void cancel();
    Q_INVOKABLE void cancelFor(QObject *owner);
    static QString printDocument(const QByteArray &pdf, QPrinter &printer);
signals:
    void stateChanged();
    void errorChanged();
    void pdfRequested(int request, int pageSize, int orientation);
private:
    void fail(const QString &message);
    QPointer<QWindow> m_host;
    QPointer<QObject> m_owner;
    QPointer<QPrintDialog> m_dialog;
    std::shared_ptr<QPrinter> m_printer;
    QString m_error;
    int m_request = 0;
    bool m_busy = false;
    bool m_spooling = false;
    bool m_dialogRunning = false;
};
