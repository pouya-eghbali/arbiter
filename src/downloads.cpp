#include "downloads.h"
#include "downloadfileactions.h"
#include "fileicons.h"
#include "settings.h"
#include <QtWebEngineQuick/QQuickWebEngineDownloadRequest>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUuid>
#include <cmath>

namespace {
QUrl storedUrl(const QUrl &url) {
    if (url.scheme() != "http" && url.scheme() != "https" && url.scheme() != "file") return {};
    const auto sanitized = url.adjusted(QUrl::RemoveUserInfo | QUrl::RemoveFragment);
    return sanitized.toEncoded().size() <= 8192 ? sanitized : QUrl();
}
}
DownloadSettings::DownloadSettings(QObject *parent) : QObject(parent) {
    const auto fallback = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    const auto saved = Settings().value("downloads/directory", fallback).toString();
    m_directory = !saved.isEmpty() && QFileInfo(saved).isAbsolute() ? QDir::cleanPath(saved) : fallback;
}
void DownloadSettings::setDirectory(const QUrl &directory) {
    if (!directory.isLocalFile() || !QFileInfo(directory.toLocalFile()).isDir()) return;
    const auto path = QFileInfo(directory.toLocalFile()).absoluteFilePath();
    if (path == m_directory) return;
    m_directory = path; Settings().setValue("downloads/directory", path); emit changed();
}
QString DownloadEntry::source() const {
    const auto source = storedUrl(url);
    return source.isEmpty() ? tr("Generated file") : source.toDisplayString().left(512);
}
QString DownloadEntry::status() const {
    switch (state) {
    case QWebEngineDownloadRequest::DownloadRequested: return tr("Starting…");
    case QWebEngineDownloadRequest::DownloadInProgress: return paused ? tr("Paused") : tr("Downloading…");
    case QWebEngineDownloadRequest::DownloadCompleted: return tr("Completed");
    case QWebEngineDownloadRequest::DownloadCancelled: return tr("Cancelled");
    case QWebEngineDownloadRequest::DownloadInterrupted: return tr("Interrupted");
    }
    return {};
}
QUrl DownloadEntry::iconSource() const { return fileTypeIcon(fileName); }
qint64 DownloadEntry::secondsRemaining() const {
    if (!active() || paused || total < 0 || m_bytesPerSecond <= 0) return -1;
    return qint64(std::ceil(qMax(qint64(0), total - received) / m_bytesPerSecond));
}
void DownloadEntry::sampleTransfer(qint64 nowMs) {
    const bool running = active() && !paused;
    if (!running || !m_wasRunning || m_sampleTime < 0 || received < m_sampleBytes) {
        m_bytesPerSecond = 0; m_sampleTime = m_progressTime = nowMs; m_sampleBytes = received;
        m_wasRunning = running; return;
    }
    const qint64 elapsed = nowMs - m_sampleTime;
    if (elapsed < 500) return;
    const qint64 delta = received - m_sampleBytes;
    if (delta > 0) m_progressTime = nowMs;
    const double rate = delta * 1000.0 / elapsed;
    m_bytesPerSecond = nowMs - m_progressTime >= 3000 ? 0
        : m_bytesPerSecond <= 0 ? rate : 0.5 * rate + 0.5 * m_bytesPerSecond;
    m_sampleTime = nowMs; m_sampleBytes = received;
}
Downloads::Downloads(DownloadSettings *preferences, QObject *parent)
    : Downloads(preferences, QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/downloads.json", parent) {}
Downloads::Downloads(DownloadSettings *preferences, const QString &historyPath, QObject *parent)
    : QObject(parent), m_preferences(preferences), m_historyPath(historyPath) {
    m_clock.start();
    m_rateTimer.setInterval(1000);
    connect(&m_rateTimer, &QTimer::timeout, this, [this] {
        for (auto *entry : m_items) if (entry->active()) {
            entry->sampleTransfer(m_clock.elapsed()); emit entry->changed();
        }
        if (!activeCount()) m_rateTimer.stop();
    });
    m_saveTimer.setSingleShot(true); m_saveTimer.setInterval(500);
    connect(&m_saveTimer, &QTimer::timeout, this, &Downloads::save);
    if (m_historyPath.isEmpty()) return;
    QFile file(m_historyPath);
    if (!file.exists()) return;
    if (!file.open(QIODevice::ReadOnly) || file.size() > 16 * 1024 * 1024) { m_error = tr("Could not read download history."); return; }
    QJsonParseError error; const auto document = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !document.isArray()) { m_error = tr("Could not read download history."); return; }
    for (const auto &value : document.array()) {
        if (m_items.size() >= 500) break;
        const auto json = value.toObject(); const auto path = json.value("path").toString();
        if (path.isEmpty() || !QFileInfo(path).isAbsolute()) continue;
        auto *entry = new DownloadEntry(this);
        entry->id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        entry->path = path; entry->fileName = QFileInfo(path).fileName(); entry->url = storedUrl(QUrl(json.value("url").toString()));
        entry->started = json.value("started").toString(); entry->received = qMax(qint64(0), json.value("received").toVariant().toLongLong());
        entry->total = json.value("total").toVariant().toLongLong(); entry->warning = json.value("warning").toString();
        const auto state = json.value("state").toInt();
        entry->state = state == QWebEngineDownloadRequest::DownloadCompleted ? QWebEngineDownloadRequest::DownloadCompleted
            : state == QWebEngineDownloadRequest::DownloadCancelled ? QWebEngineDownloadRequest::DownloadCancelled : QWebEngineDownloadRequest::DownloadInterrupted;
        if (entry->state == QWebEngineDownloadRequest::DownloadInterrupted) entry->error = tr("This download did not finish. Open its source page to download it again.");
        m_items.append(entry);
    }
}
Downloads::~Downloads() {
    for (auto *entry : m_items) if (entry->request && entry->active()) entry->request->cancel();
    save();
}
QVariantList Downloads::items() const { QVariantList values; for (auto *entry : m_items) values.append(QVariant::fromValue(entry)); return values; }
int Downloads::activeCount() const { int count = 0; for (auto *entry : m_items) if (entry->active()) ++count; return count; }
DownloadEntry *Downloads::find(const QString &id) const { for (auto *entry : m_items) if (entry->id == id) return entry; return nullptr; }
QString Downloads::safeFileName(QString name) {
    name = name.mid(qMax(name.lastIndexOf('/'), name.lastIndexOf('\\')) + 1);
    name.remove(QRegularExpression(QStringLiteral("[<>:\"/\\\\|?*\\x00-\\x1f\\x7f]")));
    name = name.trimmed(); while (name.endsWith('.') || name.endsWith(' ')) name.chop(1);
    if (name.isEmpty() || name == "." || name == "..") name = QStringLiteral("download");
    const auto stem = name.section('.', 0, 0).toUpper();
    if (QRegularExpression(QStringLiteral("^(CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])$")).match(stem).hasMatch()) name.prepend('_');
    // Bound UTF-8 path component size while preserving a short extension.
    const auto suffix = QFileInfo(name).suffix().left(16);
    if (name.toUtf8().size() > 180) {
        auto base = QFileInfo(name).completeBaseName();
        while (base.toUtf8().size() > 150) base.chop(1);
        name = base + (suffix.isEmpty() ? QString() : '.' + suffix);
    }
    return name;
}
QString Downloads::reservePath(const QString &directory, const QString &name) {
    if (directory.isEmpty() || !QFileInfo(directory).isAbsolute() || !QDir().mkpath(directory)) return {};
    const auto safe = safeFileName(name); const QFileInfo info(safe);
    for (int attempt = 0; attempt < 10000; ++attempt) {
        const auto candidate = attempt == 0 ? safe : info.completeBaseName() + QStringLiteral(" (%1)").arg(attempt)
            + (info.suffix().isEmpty() ? QString() : '.' + info.suffix());
        const auto path = QDir(directory).filePath(candidate);
        QFile file(path);
        if (file.open(QIODevice::WriteOnly | QIODevice::NewOnly)) return path;
        if (!QFileInfo::exists(path)) return {};
    }
    return {};
}
void Downloads::trackProfile(QQuickWebEngineProfile *profile) {
    if (!profile || profile->property("arbiterDownloadsTracked").toBool()) return;
    profile->setProperty("arbiterDownloadsTracked", true);
    profile->setDownloadPath(m_preferences->path());
    connect(m_preferences, &DownloadSettings::changed, profile, [profile, preferences = m_preferences] { profile->setDownloadPath(preferences->path()); });
    connect(profile, &QQuickWebEngineProfile::downloadRequested, this, [this](QQuickWebEngineDownloadRequest *request) { receive(request); });
}
void Downloads::receive(QWebEngineDownloadRequest *request) {
    if (!request || m_blocked) { if (request) request->cancel(); return; }
    auto *entry = new DownloadEntry(this);
    entry->id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    entry->url = request->url(); entry->started = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    entry->request = request;
    const bool savingPage = request->isSavePageDownload() && request->state() != QWebEngineDownloadRequest::DownloadRequested;
    entry->path = savingPage ? QDir(request->downloadDirectory()).filePath(request->downloadFileName())
                            : reservePath(m_preferences->path(), request->suggestedFileName());
    entry->fileName = entry->path.isEmpty() ? safeFileName(request->suggestedFileName()) : QFileInfo(entry->path).fileName();
    entry->reserved = !savingPage && !entry->path.isEmpty();
    m_items.prepend(entry);
    connect(request, &QWebEngineDownloadRequest::stateChanged, entry, [this, entry] { update(entry); });
    connect(request, &QWebEngineDownloadRequest::receivedBytesChanged, entry, [this, entry] { update(entry); });
    connect(request, &QWebEngineDownloadRequest::totalBytesChanged, entry, [this, entry] { update(entry); });
    connect(request, &QWebEngineDownloadRequest::isFinishedChanged, entry, [this, entry] { update(entry); });
    connect(request, &QWebEngineDownloadRequest::isPausedChanged, entry, [this, entry] { update(entry); });
    connect(request, &QObject::destroyed, entry, [this, entry] {
        entry->request = nullptr;
        if (!entry->completed() && entry->reserved) {
            const QFileInfo file(entry->path); if (file.isFile() && file.size() == 0) QFile::remove(entry->path);
            entry->reserved = false;
        }
        if (entry->active()) { entry->state = QWebEngineDownloadRequest::DownloadInterrupted; entry->error = tr("The browser profile was closed before the download finished."); }
        entry->sampleTransfer(m_clock.elapsed());
        emit entry->changed(); emit changed(); m_saveTimer.start();
    });
    emit itemsChanged();
    if (entry->path.isEmpty()) {
        request->cancel(); entry->error = tr("Could not create a file in the download folder. Choose another folder in Settings → Downloads.");
        entry->state = QWebEngineDownloadRequest::DownloadInterrupted; emit entry->changed(); emit changed(); m_saveTimer.start(); emit started(); return;
    }
    if (!savingPage) {
        request->setDownloadDirectory(QFileInfo(entry->path).absolutePath()); request->setDownloadFileName(entry->fileName);
        request->accept();
    }
    update(entry); emit started();
}
void Downloads::update(DownloadEntry *entry) {
    if (!entry->request) return;
    const auto previous = entry->state;
    entry->state = entry->request->state(); entry->received = entry->request->receivedBytes(); entry->total = entry->request->totalBytes();
    entry->paused = entry->request->isPaused(); entry->error = entry->request->interruptReasonString();
    entry->sampleTransfer(m_clock.elapsed());
    if (entry->active() && !m_rateTimer.isActive()) m_rateTimer.start();
    if (entry->state != QWebEngineDownloadRequest::DownloadInterrupted) entry->error.clear();
    if (entry->completed() && previous != entry->state) entry->warning = markDownloadedFile(entry->path, storedUrl(entry->url));
    if (!entry->active() && !entry->completed() && !entry->canRetry() && entry->reserved) {
        const QFileInfo file(entry->path); if (file.isFile() && file.size() == 0) QFile::remove(entry->path);
        entry->reserved = false;
    }
    emit entry->changed(); emit changed();
    if (!m_saveTimer.isActive()) m_saveTimer.start();
}
void Downloads::cancel(const QString &id) { if (auto *entry = find(id); entry && entry->request && entry->active()) entry->request->cancel(); }
void Downloads::pause(const QString &id) { if (auto *entry = find(id); entry && entry->request && entry->active()) entry->request->pause(); }
void Downloads::resume(const QString &id) { if (auto *entry = find(id); entry && entry->request && entry->active() && entry->paused) entry->request->resume(); }
void Downloads::retry(const QString &id) {
    auto *entry = find(id); if (!entry || !entry->canRetry()) return;
    entry->request->resume();
    update(entry);
}
void Downloads::open(const QString &id) {
    auto *entry = find(id); if (!entry || !entry->completed()) return;
    if (!QFileInfo(entry->path).isFile() || !QDesktopServices::openUrl(QUrl::fromLocalFile(entry->path))) {
        entry->error = tr("The file is missing or could not be opened."); emit entry->changed();
    }
}
void Downloads::showFolder(const QString &id) {
    auto *entry = find(id); if (!entry || entry->path.isEmpty()) return;
    if (!revealDownloadedFile(entry->path)) { entry->error = tr("Could not open this folder."); emit entry->changed(); }
}
void Downloads::clearFinished() {
    for (qsizetype i = m_items.size() - 1; i >= 0; --i) if (!m_items[i]->active()) { auto *entry = m_items.takeAt(i); if (entry->request) { entry->request->cancel(); entry->request->deleteLater(); } entry->deleteLater(); }
    emit itemsChanged(); emit changed(); save();
}
void Downloads::save() {
    if (m_historyPath.isEmpty()) return;
    QJsonArray values;
    for (auto *entry : m_items) {
        if (values.size() >= 500) break;
        values.append(QJsonObject{{"path", entry->path}, {"url", storedUrl(entry->url).toString(QUrl::FullyEncoded)}, {"started", entry->started},
            {"state", int(entry->state)}, {"received", entry->received}, {"total", entry->total}, {"warning", entry->warning}});
    }
    QDir().mkpath(QFileInfo(m_historyPath).absolutePath());
    QSaveFile file(m_historyPath); const auto bytes = QJsonDocument(values).toJson();
    if (!file.open(QIODevice::WriteOnly) || !file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner)
        || file.write(bytes) != bytes.size() || !file.commit()) { m_error = tr("Could not save download history."); emit changed(); }
}
