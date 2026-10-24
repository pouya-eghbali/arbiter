#include "extensioncatalog.h"
#include "crxpackage.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <algorithm>
#include <utility>

namespace {
QByteArray manifestBytes(const QWebEngineExtensionInfo &info) {
    QFile file(QDir(info.path()).filePath(QStringLiteral("manifest.json")));
    if (!file.open(QIODevice::ReadOnly) || file.size() > 1024 * 1024) return {};
    return file.readAll();
}
QJsonObject manifest(const QWebEngineExtensionInfo &info) {
    return QJsonDocument::fromJson(manifestBytes(info)).object();
}
QUrl extensionPage(const QWebEngineExtensionInfo &info, const QString &path) {
    if (!info.isEnabled() || path.isEmpty()) return {};
    const QUrl base(QStringLiteral("chrome-extension://") + info.id() + '/');
    const QUrl url = base.resolved(QUrl(path));
    return url.scheme() == base.scheme() && url.host() == base.host() && url.userInfo().isEmpty() ? url : QUrl();
}
}
ExtensionCatalog::ExtensionCatalog(QQuickWebEngineProfile *profile) : QObject(profile), m_profile(profile) {
    if (!profile || profile->isOffTheRecord()) return;
    m_statePath = QDir(profile->persistentStoragePath()).filePath(QStringLiteral("arbiter-extension-consent.json"));
    QFile state(m_statePath);
    if (state.open(QIODevice::ReadOnly) && state.size() < 1024 * 1024)
        m_approvals = QJsonDocument::fromJson(state.readAll()).object();
    m_manager = profile->extensionManager();
    if (!m_manager) return;
    m_store = new WebStoreInstall(this);
    connect(m_store, &WebStoreInstall::changed, this, [this] { m_status = m_store->status(); emit changed(); });
    connect(m_store, &WebStoreInstall::failed, this, [this](const QString &error) { m_busy = false; m_status = error; emit changed(); });
    connect(m_store, &WebStoreInstall::ready, this, [this](const QString &path, const QString &id) {
        m_pendingStoreId = id;
        m_manager->installExtension(path);
    });
    connect(m_manager, &QWebEngineExtensionManager::loadFinished, this, [this](const auto &info) { loaded(info, false); });
    connect(m_manager, &QWebEngineExtensionManager::installFinished, this, [this](const auto &info) { loaded(info, true); });
    connect(m_manager, &QWebEngineExtensionManager::uninstallFinished, this, [this](const auto &info) {
        m_busy = false;
        m_status = info.error().isEmpty() ? tr("Extension removed.") : info.error();
        refresh();
    });
    // Profiles can have finished loading extensions before the catalog is opened.
    for (const auto &info : m_manager->extensions()) loaded(info, false);
    refresh();
}
QString ExtensionCatalog::fingerprint(const QWebEngineExtensionInfo &info) const {
    const auto bytes = manifestBytes(info);
    return bytes.isEmpty() ? QString() : QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
}
QWebEngineExtensionInfo ExtensionCatalog::find(const QString &id) const {
    if (m_manager) for (const auto &info : m_manager->extensions())
        if (info.id() == id && info.isInstalled()) return info;
    return {};
}
void ExtensionCatalog::loaded(const QWebEngineExtensionInfo &info, bool installed) {
    if (installed) {
        m_busy = false;
        if (m_store) m_store->release();
        const auto expected = std::exchange(m_pendingStoreId, {});
        if (!expected.isEmpty() && info.error().isEmpty() && info.id() != expected) {
            m_manager->setExtensionEnabled(info, false);
            m_status = tr("The installed extension's identity did not match the verified store package. It has been left disabled.");
            refresh(); return;
        }
    }
    if (!info.error().isEmpty()) m_status = info.error();
    else if (info.isInstalled()) {
        const auto hash = fingerprint(info);
        if (!hash.isEmpty() && m_approvals.value(info.id()).toString() == hash)
            m_manager->setExtensionEnabled(info, true);
        else {
            m_manager->setExtensionEnabled(info, false);
            if (installed) m_status = tr("Installed. Review its permissions to enable it.");
        }
    }
    refresh();
    if (installed && info.error().isEmpty()) requestEnable(info.id());
}
bool ExtensionCatalog::saveApprovals(const QJsonObject &approvals) {
    QDir().mkpath(QFileInfo(m_statePath).absolutePath());
    QSaveFile file(m_statePath);
    if (!file.open(QIODevice::WriteOnly)) { m_status = tr("Could not save extension preferences."); emit changed(); return false; }
    file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    const auto bytes = QJsonDocument(approvals).toJson();
    if (file.write(bytes) != bytes.size() || !file.commit()) {
        m_status = tr("Could not save extension preferences."); emit changed(); return false;
    }
    m_approvals = approvals;
    return true;
}
void ExtensionCatalog::refresh() {
    m_items.clear();
    if (m_manager) for (const auto &info : m_manager->extensions()) {
        if (!info.isInstalled()) continue; // Keep Qt's built-in PDF/Hangouts untouched.
        const auto json = manifest(info);
        QStringList permissions;
        for (const auto &key : {"permissions", "host_permissions"})
            for (const auto &entry : json.value(QLatin1String(key)).toArray()) permissions.append(entry.toString());
        for (const auto &script : json.value(QStringLiteral("content_scripts")).toArray())
            for (const auto &entry : script.toObject().value(QStringLiteral("matches")).toArray()) permissions.append(entry.toString());
        permissions.removeDuplicates(); permissions.sort();
        m_items.append(QVariantMap{{"id", info.id()}, {"name", info.name()}, {"description", info.description()},
            {"version", json.value(QStringLiteral("version")).toString()}, {"enabled", info.isEnabled()},
            {"permissions", permissions}, {"hasPopup", !info.actionPopupUrl().isEmpty()},
            {"hasOptions", !optionsUrl(info.id()).isEmpty()}});
    }
    std::sort(m_items.begin(), m_items.end(), [](const auto &a, const auto &b) {
        return a.toMap().value("name").toString().localeAwareCompare(b.toMap().value("name").toString()) < 0;
    });
    emit changed();
}
void ExtensionCatalog::install(const QUrl &source) {
    if (!m_manager || m_busy || !m_review.isEmpty()) return;
    const QFileInfo file(source.toLocalFile());
    if (!source.isLocalFile() || !file.exists() || !(file.isDir() || file.suffix().compare(QStringLiteral("zip"), Qt::CaseInsensitive) == 0)) {
        m_status = tr("Choose an unpacked Manifest V3 extension folder or a ZIP with manifest.json at its root."); emit changed(); return;
    }
    m_busy = true; m_status = tr("Installing extension…"); emit changed();
    m_manager->installExtension(file.canonicalFilePath());
}
QString ExtensionCatalog::storeItemId(const QString &linkOrId) const { return CrxPackage::storeId(linkOrId); }
void ExtensionCatalog::installFromStore(const QString &linkOrId) {
    if (!m_store || m_busy || !m_review.isEmpty()) return;
    const auto id = CrxPackage::storeId(linkOrId);
    if (id.isEmpty()) { m_status = tr("Paste a Chrome Web Store extension link or its 32-character extension ID."); emit changed(); return; }
    if (!find(id).id().isEmpty()) { m_status = tr("This extension is already installed in this profile."); emit changed(); return; }
    m_busy = true; m_store->start(id);
}
void ExtensionCatalog::cancelInstall() { if (m_store) m_store->cancel(); }
void ExtensionCatalog::requestEnable(const QString &id) {
    if (!m_manager || m_busy) return;
    const auto info = find(id);
    if (info.id().isEmpty() || info.isEnabled()) return;
    m_reviewHash = fingerprint(info);
    if (m_reviewHash.isEmpty()) { m_status = tr("Could not read this extension's manifest."); emit changed(); return; }
    refresh();
    for (const auto &item : m_items) if (item.toMap().value("id").toString() == id) m_review = item.toMap();
    emit changed();
}
void ExtensionCatalog::approve() {
    if (!m_manager || m_review.isEmpty()) return;
    const auto info = find(m_review.value("id").toString());
    if (info.id().isEmpty() || fingerprint(info) != m_reviewHash) {
        cancelReview(); m_status = tr("The extension changed. Review it again before enabling it."); emit changed(); return;
    }
    auto approvals = m_approvals; approvals.insert(info.id(), m_reviewHash);
    if (!saveApprovals(approvals)) return;
    m_manager->setExtensionEnabled(info, true);
    m_review.clear(); m_reviewHash.clear(); m_status = tr("Extension enabled. Reload existing pages to apply content scripts."); refresh();
}
void ExtensionCatalog::cancelReview() { m_review.clear(); m_reviewHash.clear(); emit changed(); }
void ExtensionCatalog::disable(const QString &id) {
    if (!m_manager || m_busy) return;
    const auto info = find(id); if (info.id().isEmpty()) return;
    auto approvals = m_approvals; approvals.remove(id);
    if (!saveApprovals(approvals)) return;
    m_manager->setExtensionEnabled(info, false);
    if (m_review.value("id").toString() == id) cancelReview();
    m_status = tr("Extension disabled. Reload pages to remove previously injected content."); refresh();
}
void ExtensionCatalog::remove(const QString &id) {
    if (!m_manager || m_busy) return;
    const auto info = find(id); if (info.id().isEmpty()) return;
    auto approvals = m_approvals; approvals.remove(id);
    if (!saveApprovals(approvals)) return;
    m_manager->setExtensionEnabled(info, false);
    cancelReview(); m_busy = true; m_status = tr("Removing extension…"); emit changed();
    m_manager->uninstallExtension(info);
}
QUrl ExtensionCatalog::popupUrl(const QString &id) const {
    const auto info = find(id);
    return info.isEnabled() ? extensionPage(info, info.actionPopupUrl().toString()) : QUrl();
}
QUrl ExtensionCatalog::optionsUrl(const QString &id) const {
    const auto info = find(id); const auto json = manifest(info);
    auto path = json.value(QStringLiteral("options_ui")).toObject().value(QStringLiteral("page")).toString();
    if (path.isEmpty()) path = json.value(QStringLiteral("options_page")).toString();
    return extensionPage(info, path);
}
