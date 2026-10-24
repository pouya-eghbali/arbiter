#include <QFile>
#include <QApplication>
#include <QTimer>
#include <QTcpServer>
#include <QTcpSocket>
#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QWebEngineSettings>
#include <QWebEngineView>

// Manual network smoke test. Uses an ephemeral profile, never browser user data.
class ProbePage : public QWebEnginePage {
public:
    using QWebEnginePage::QWebEnginePage;
protected:
    void javaScriptConsoleMessage(JavaScriptConsoleMessageLevel, const QString &message,
                                  int, const QString &) override {
        qInfo().noquote() << message;
        if (message.startsWith("PROBE_PASS") || message.startsWith("PROBE_FAIL")) {
            const int result = message.startsWith("PROBE_PASS") ? 0 : 1;
            QTimer::singleShot(0, qApp, [result] { QCoreApplication::exit(result); });
        }
    }
};

int main(int argc, char **argv) {
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
    QApplication app(argc, argv);
#ifdef ARBITER_PATCHED_WEBENGINE
    if (qWebEngineArbiterWebAuthnAbi() != 1) return 2;
#endif
    app.setApplicationName("ArbiterMediaProbe");
    QWebEngineProfile profile; // Off the record: no persistent cookies/CDM data.
    ProbePage page(&profile);
    QWebEngineView view;
    view.setPage(&page);
    view.resize(720, 480);
    view.setWindowTitle("Arbiter media test — public demo content");
    view.show();
    page.settings()->setAttribute(QWebEngineSettings::PlaybackRequiresUserGesture, false);
    QFile html(QStringLiteral(PROBE_HTML));
    if (!html.open(QIODevice::ReadOnly)) return 2;
    const QByteArray body = html.readAll();
    QTcpServer server;
    if (!server.listen(QHostAddress::LocalHost)) return 2;
    QObject::connect(&server, &QTcpServer::newConnection, &server, [&] {
        while (auto *socket = server.nextPendingConnection()) {
            QObject::connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
            QObject::connect(socket, &QTcpSocket::readyRead, socket, [socket, body] {
                if (socket->property("responded").toBool()) return;
                socket->setProperty("responded", true);
                socket->readAll();
                socket->write("HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\n"
                              "Cache-Control: no-store\r\nConnection: close\r\nContent-Length: "
                              + QByteArray::number(body.size()) + "\r\n\r\n" + body);
                socket->disconnectFromHost();
            });
        }
    });
    QTimer::singleShot(90000, &app, [] {
        qCritical("PROBE_FAIL timeout");
        QCoreApplication::exit(1);
    });
    page.load(QUrl(QString("http://127.0.0.1:%1/").arg(server.serverPort())));
    return app.exec();
}
