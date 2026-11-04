#include <QFile>
#include <QApplication>
#include <QTimer>
#include <QTcpServer>
#include <QTcpSocket>
#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QWebEngineSettings>
#include <QWebEngineView>
#include <QCommandLineParser>

// Manual media smoke tests. Uses an ephemeral profile, never browser user data.
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
    QCommandLineParser arguments;
    arguments.addHelpOption();
    arguments.addOption({"media-file", "Test local H.264/AAC playback without network or DRM.", "path"});
    arguments.addOption({"widevine-path", "Existing CDM path for the network/DRM probe.", "path"});
    arguments.process(app);
    const bool local = arguments.isSet("media-file");
    QByteArray media;
    if (local) {
        QFile fixture(arguments.value("media-file"));
        if (!fixture.open(QIODevice::ReadOnly)) { qCritical("Cannot read local media fixture"); return 2; }
        media = fixture.readAll();
    }
#ifdef ARBITER_PATCHED_WEBENGINE
    if (qWebEngineArbiterWebAuthnAbi() != 1) return 2;
#endif
    app.setApplicationName("ArbiterMediaProbe");
    QWebEngineProfile profile; // Off the record: no persistent cookies/CDM data.
    ProbePage page(&profile);
    QWebEngineView view;
    view.setPage(&page);
    view.resize(720, 480);
    view.setWindowTitle(local ? "Arbiter codec test — local media" : "Arbiter media test — public demo content");
    view.show();
    page.settings()->setAttribute(QWebEngineSettings::PlaybackRequiresUserGesture, false);
    QFile html(local ? QStringLiteral(":/media-probe/local-playback.html") : QStringLiteral(":/media-probe/probe.html"));
    if (!html.open(QIODevice::ReadOnly)) return 2;
    const QByteArray body = html.readAll();
    QTcpServer server;
    if (!server.listen(QHostAddress::LocalHost)) return 2;
    QObject::connect(&server, &QTcpServer::newConnection, &server, [&] {
        while (auto *socket = server.nextPendingConnection()) {
            QObject::connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
            QObject::connect(socket, &QTcpSocket::readyRead, socket, [socket, body, media] {
                if (socket->property("responded").toBool()) return;
                socket->setProperty("responded", true);
                const auto request = socket->readAll();
                const bool video = request.startsWith("GET /fixture.mp4 ");
                const QByteArray &response = video ? media : body;
                socket->write("HTTP/1.1 200 OK\r\nContent-Type: " + QByteArray(video ? "video/mp4" : "text/html; charset=utf-8") + "\r\n"
                              "Cache-Control: no-store\r\nConnection: close\r\nContent-Length: "
                              + QByteArray::number(response.size()) + "\r\n\r\n" + response);
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
