#include "webstoreinstall.h"
#include "crxpackage.h"
#include <QFutureWatcher>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QtConcurrent>
#include <QtWebEngineCore/qtwebenginecoreglobal.h>

namespace {
struct Prepared {
    QSharedPointer<QTemporaryDir> directory;
    QString error;
};
}
WebStoreInstall::WebStoreInstall(QObject *parent) : QObject(parent) {
    m_deadline.setSingleShot(true); m_deadline.setInterval(120000);
    connect(&m_deadline, &QTimer::timeout, this, [this] { fail(tr("The store download timed out. Try again.")); });
}
WebStoreInstall::~WebStoreInstall() {
    ++m_generation; m_active = false;
    if (m_reply) { disconnect(m_reply, nullptr, this, nullptr); m_reply->abort(); }
}
void WebStoreInstall::start(const QString &id) {
    if (m_active) return;
    release(); ++m_generation; m_id = id; m_active = true; m_redirects = 0;
    m_progress = -1; m_status = tr("Downloading from Chrome Web Store…"); emit changed();
    m_deadline.start(); request(CrxPackage::downloadUrl(id, QString::fromLatin1(qWebEngineChromiumVersion())));
}
void WebStoreInstall::request(const QUrl &url) {
    if (!CrxPackage::allowedDownloadUrl(url)) { fail(tr("The store redirected to an untrusted download address.")); return; }
    m_bytes.clear();
    QNetworkRequest networkRequest(url);
    networkRequest.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    networkRequest.setAttribute(QNetworkRequest::CookieLoadControlAttribute, QNetworkRequest::Manual);
    networkRequest.setAttribute(QNetworkRequest::CookieSaveControlAttribute, QNetworkRequest::Manual);
    networkRequest.setAttribute(QNetworkRequest::AuthenticationReuseAttribute, QNetworkRequest::Manual);
    networkRequest.setTransferTimeout(30000);
    networkRequest.setRawHeader("Accept", "application/x-chrome-extension,application/octet-stream");
    auto *reply = m_network.get(networkRequest); m_reply = reply;
    reply->setReadBufferSize(256 * 1024);
    const auto generation = m_generation;
    connect(reply, &QNetworkReply::readyRead, this, [this, reply, generation] {
        if (!m_active || generation != m_generation || m_reply != reply) return;
        if (reply->header(QNetworkRequest::ContentLengthHeader).toLongLong() > CrxPackage::MaximumDownload
            || reply->bytesAvailable() > CrxPackage::MaximumDownload - m_bytes.size()) {
            fail(tr("The extension package exceeds the 64 MiB download limit.")); return;
        }
        m_bytes.append(reply->readAll());
    });
    connect(reply, &QNetworkReply::downloadProgress, this, [this, reply, generation](qint64 received, qint64 total) {
        if (!m_active || generation != m_generation || m_reply != reply) return;
        if (received > CrxPackage::MaximumDownload || total > CrxPackage::MaximumDownload) {
            fail(tr("The extension package exceeds the 64 MiB download limit.")); return;
        }
        m_progress = total > 0 ? qBound(0.0, double(received) / total, 1.0) : -1;
        emit changed();
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply, generation] {
        reply->deleteLater();
        if (!m_active || generation != m_generation || m_reply != reply) return;
        m_reply = nullptr;
        const int code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (code >= 300 && code < 400) {
            const auto target = reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();
            if (target.isEmpty() || ++m_redirects > 5) { fail(tr("The store returned too many or invalid redirects.")); return; }
            request(reply->url().resolved(target)); return;
        }
        if (reply->error() != QNetworkReply::NoError || code != 200) {
            fail(tr("Could not download this store item (HTTP %1). It may be unavailable, restricted, or require a newer browser. %2").arg(code).arg(reply->errorString())); return;
        }
        if (reply->bytesAvailable() > CrxPackage::MaximumDownload - m_bytes.size()) { fail(tr("The extension package is too large.")); return; }
        m_bytes.append(reply->readAll()); m_deadline.stop(); prepare(std::move(m_bytes));
    });
}
void WebStoreInstall::prepare(QByteArray bytes) {
    m_status = tr("Verifying and unpacking extension…"); m_progress = -1; emit changed();
    const auto generation = m_generation; const auto id = m_id;
    auto *watcher = new QFutureWatcher<Prepared>(this);
    connect(watcher, &QFutureWatcher<Prepared>::finished, this, [this, watcher, generation, id] {
        const auto prepared = watcher->result(); watcher->deleteLater();
        if (!m_active || generation != m_generation) return;
        if (!prepared.error.isEmpty()) { fail(prepared.error); return; }
        m_directory = prepared.directory; m_active = false; m_progress = 1;
        m_status = tr("Installing verified extension…"); emit changed();
        emit ready(m_directory->path(), id);
    });
    watcher->setFuture(QtConcurrent::run([bytes = std::move(bytes), id] {
        const auto verified = CrxPackage::verify(bytes, id, CrxPackage::publisherKeyHash());
        if (!verified.error.isEmpty()) return Prepared{{}, verified.error};
        auto directory = QSharedPointer<QTemporaryDir>::create();
        if (!directory->isValid()) return Prepared{{}, QStringLiteral("Could not create a temporary extension directory.")};
        const auto error = CrxPackage::unpack(verified, directory->path());
        return Prepared{directory, error};
    }));
}
void WebStoreInstall::fail(const QString &error) {
    if (!m_active) return;
    ++m_generation; m_active = false; m_deadline.stop();
    if (m_reply) { auto *reply = m_reply.data(); m_reply = nullptr; reply->abort(); reply->deleteLater(); }
    m_bytes.clear(); m_directory.clear(); m_progress = -1; m_status = error;
    emit changed(); emit failed(error);
}
void WebStoreInstall::cancel() { fail(tr("Extension installation cancelled.")); }
void WebStoreInstall::release() { m_directory.clear(); }
