#include "mobilemodel.h"
#include "mobileplatform.h"
#include "nativepage.h"
#include <QBuffer>
#include <QClipboard>
#include <QColor>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLocale>
#include <QMutexLocker>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUrlQuery>
#include <QUuid>
#ifndef Q_OS_IOS
extern "C" {
void *arbiter_adblock_new(const char *);
void arbiter_adblock_free(void *);
bool arbiter_adblock_check(const void *, const char *, const char *, const char *, const char *);
}
#endif
MobileModel *MobileModel::instance = nullptr;
// JNI callbacks may finish after main returns; keep only this guard process-lived.
QReadWriteLock &MobileModel::lifetimeLock() {
    static auto guard = new QReadWriteLock;
    return *guard;
}
bool MobileModel::filterRequest(const QUrl &url, const QUrl &source, const QByteArray &type,
                                const QByteArray &method) {
    QReadLocker guard(&lifetimeLock());
    return instance && instance->blockRequest(url, source, type, method);
}
namespace {
bool validSetting(const QString &name, const QJsonValue &value) {
    if (QStringList{"theme", "language", "searchEngine"}.contains(name)) {
        if (!value.isString())
            return false;
        const auto v = value.toString();
        if (name == "theme")
            return QStringList{"system", "light", "dark"}.contains(v);
        if (name == "language")
            return QStringList{"system", "en", "de", "fr", "it", "es", "pt", "ja", "zh"}.contains(v);
        return QStringList{"DuckDuckGo", "Google", "Bing", "Brave", "Ecosia", "Startpage"}.contains(v);
    }
    if (name.endsWith("Seconds"))
        return value.isDouble() && value.toDouble() >= 0 && value.toDouble() <= 86400 &&
               value.toDouble() == value.toInt();
    if (name.endsWith("Accent") || name.endsWith("Background"))
        return value.isString() && QColor(value.toString()).isValid();
    if (name.endsWith("Image"))
        return value.isString() && (value.toString().isEmpty() || QUrl(value.toString()).isLocalFile());
    if (name == "adblockDisabledHosts") {
        if (!value.isArray() || value.toArray().size() > 10000)
            return false;
        for (auto host : value.toArray())
            if (!host.isString() || host.toString().size() > 253)
                return false;
        return true;
    }
    return value.isBool();
}
QString id() {
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}
qint64 now() {
    return QDateTime::currentMSecsSinceEpoch();
}
} // namespace
MobileModel::MobileModel(QObject *parent, const QString &directory) : QObject(parent), m_vault(this) {
    m_storePath = (directory.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                                       : directory) +
                  "/browser-session-v1.json";
    {
        QWriteLocker guard(&lifetimeLock());
        instance = this;
    }
    m_privateContext = id();
    connect(this, &MobileModel::changed, this, [this] {
        m_screens.synchronize(m_tabs);
        bool removed = false;
        for (const auto &key : m_previews.keys())
            if (tabIndex(key) < 0) {
                m_previews.remove(key);
                removed = true;
            }
        if (removed)
            emit previewsChanged();
    });
    restore();
    m_screens.synchronize(m_tabs);
    m_activity.start();
    m_saveTimer.setSingleShot(true);
    m_saveTimer.setInterval(150);
    connect(&m_saveTimer, &QTimer::timeout, this, &MobileModel::persist);
    m_pendingTimer.setSingleShot(true);
    m_pendingTimer.setInterval(45000);
    connect(&m_pendingTimer, &QTimer::timeout, this, [this] { m_pending = {}; });
    m_sleepTimer.setInterval(30000);
    connect(&m_sleepTimer, &QTimer::timeout, this, &MobileModel::sleepIdle);
    m_sleepTimer.start();
    m_privateTimer.setInterval(1000);
    connect(&m_privateTimer, &QTimer::timeout, this, [this] {
        const int seconds = m_settings.value("privateLockSeconds").toInt(60);
        if (m_private && !m_locked && seconds > 0 && m_activity.elapsed() >= seconds * 1000)
            lockPrivate();
    });
    m_privateTimer.start();
    m_syncTimer.setInterval(20000);
    connect(&m_syncTimer, &QTimer::timeout, this, [this] { backgroundSync(); });
    m_syncTimer.start();
    connect(qGuiApp, &QGuiApplication::applicationStateChanged, this, [this](Qt::ApplicationState state) {
        if (state == Qt::ApplicationSuspended || (state != Qt::ApplicationActive && !verificationActive)) {
            lockPrivate();
            m_vault.lock();
            m_focus = {};
            m_pending = {};
            emit changed();
        }
        if (state == Qt::ApplicationActive)
            backgroundSync();
    });
    connect(qGuiApp, &QCoreApplication::aboutToQuit, this, &MobileModel::persist);
    m_vault.setCooldown(m_settings.value("passwordUnlockSeconds").toInt(300));
    MobilePlatform::setLanguage(m_settings.value("language").toString("system"));
    MobilePlatform::configureBackgroundSync(onboardingDone() &&
                                            m_settings.value("backgroundSync").toBool(true));
#ifndef Q_OS_IOS
    QByteArray rules;
    for (const auto &name : {"easylist.txt", "easyprivacy.txt"}) {
        QFile f(QString(":/adblock/") + name);
        if (f.open(QIODevice::ReadOnly))
            rules += f.readAll() + "\n";
    }
    m_rules = arbiter_adblock_new(rules.constData());
#endif
}
MobileModel::~MobileModel() {
    QWriteLocker lifetime(&lifetimeLock());
    instance = nullptr;
#ifndef Q_OS_IOS
    QMutexLocker lock(&m_rulesMutex);
    if (m_rules)
        arbiter_adblock_free(m_rules);
#endif
}
QString MobileModel::backend() const {
#ifdef ARBITER_MOBILE_DESIGN_PREVIEW
    if (property("_designPreview").toBool())
        return "preview";
#endif
#ifdef ARBITER_MOBILE_WEBENGINE
    return "webengine";
#else
    return "webview";
#endif
}
QVariantList MobileModel::tabs() const {
    return m_tabs.toVariantList();
}
QVariantList MobileModel::workspaces() const {
    return m_workspaces.toVariantList();
}
QVariantList MobileModel::history() const {
    return m_history.toVariantList();
}
QVariantList MobileModel::downloads() const {
    return m_downloads.toVariantList();
}
int MobileModel::tabIndex(const QString &key) const {
    for (int i = 0; i < m_tabs.size(); ++i)
        if (m_tabs[i].toObject().value("id") == key)
            return i;
    return -1;
}
QJsonObject MobileModel::page(const QString &key) const {
    auto index = tabIndex(key);
    return index < 0 ? QJsonObject() : m_tabs[index].toObject();
}
void MobileModel::restore() {
    m_settings = {
        {"theme", "system"},           {"language", "system"},         {"searchEngine", "DuckDuckGo"},
        {"lightAccent", "#2868b8"},    {"darkAccent", "#ab4dff"},      {"lightBackground", "#edf2f7"},
        {"darkBackground", "#101720"}, {"adblockEnabled", true},       {"backgroundSync", true},
        {"savePasswords", true},       {"suggestPasswords", true},     {"tabSleepSeconds", 600},
        {"privateLockSeconds", 60},    {"passwordUnlockSeconds", 300}, {"onboardingDone", false}};
    QFile file(m_storePath);
    QJsonObject data;
    if (file.size() < 8 * 1024 * 1024 && file.open(QIODevice::ReadOnly))
        data = QJsonDocument::fromJson(file.readAll()).object();
    else
        data = QJsonDocument::fromJson(MobilePlatform::legacySession().toUtf8()).object();
    auto values = data.value("settings").toObject();
    for (const auto &name : {"lightAccent", "darkAccent", "lightBackground", "darkBackground"})
        if (values.value(name).isDouble())
            values[name] = QColor::fromRgba(quint32(values.value(name).toDouble())).name(QColor::HexArgb);
    for (auto i = values.begin(); i != values.end(); ++i)
        if ((m_settings.contains(i.key()) || i.key().endsWith("Image") ||
             i.key() == "adblockDisabledHosts") &&
            validSetting(i.key(), i.value()))
            m_settings[i.key()] = i.value();
    for (const auto &name : {"lightAccent", "darkAccent", "lightBackground", "darkBackground"})
        if (m_settings.value(name).isDouble())
            m_settings[name] =
                QColor::fromRgba(quint32(m_settings.value(name).toDouble())).name(QColor::HexArgb);
    m_tabs = data.value("tabs").toArray();
    m_workspaces = data.value("workspaces").toArray();
    m_history = data.value("history").toArray();
    m_downloads = data.value("downloads").toArray();
    m_workspace = data.value("workspaceId").toString();
    m_active = data.value("activeId").toString();
    if (m_workspaces.isEmpty()) {
        m_workspace = id();
        m_workspaces.append(QJsonObject{
            {"id", m_workspace}, {"name", tr("Personal")}, {"color", "#ab4dff"}, {"isolated", false}});
    }
    if (m_workspace.isEmpty())
        m_workspace = m_workspaces[0].toObject().value("id").toString();
    for (int i = m_tabs.size() - 1; i >= 0; --i) {
        auto t = m_tabs[i].toObject();
        if (t.value("private").toBool()) {
            m_tabs.removeAt(i);
            continue;
        }
        QString profile = "shared";
        for (const auto &w : m_workspaces)
            if (w.toObject().value("id") == t.value("workspace") && w.toObject().value("isolated").toBool())
                profile = t.value("workspace").toString();
        t["profile"] = profile;
        m_tabs[i] = t;
    }
    ensureTab();
}
void MobileModel::persist() {
    QJsonArray normal;
    for (const auto &v : m_tabs) {
        auto t = v.toObject();
        if (t.value("private").toBool())
            continue;
        for (auto k : {"loading", "audio", "dirty", "progress", "back", "forward"})
            t.remove(k);
        normal.append(t);
    }
    QDir().mkpath(QFileInfo(m_storePath).absolutePath());
    QSaveFile file(m_storePath);
    if (!file.open(QIODevice::WriteOnly))
        return;
    file.write(QJsonDocument(QJsonObject{{"v", 1},
                                         {"tabs", normal},
                                         {"workspaces", m_workspaces},
                                         {"history", m_history},
                                         {"downloads", m_downloads},
                                         {"settings", m_settings},
                                         {"activeId", m_private ? QString() : m_active},
                                         {"workspaceId", m_workspace}})
                   .toJson(QJsonDocument::Compact));
    file.commit();
}
void MobileModel::ensureTab() {
    auto current = page(m_active);
    if (!current.isEmpty() && current.value("workspace") == m_workspace &&
        current.value("private").toBool() == m_private)
        return;
    for (const auto &v : m_tabs) {
        auto t = v.toObject();
        if (t.value("workspace") == m_workspace && t.value("private").toBool() == m_private) {
            m_active = t.value("id").toString();
            return;
        }
    }
    addTab();
}
QString MobileModel::addTab(const QString &address) {
    m_active = id();
    QUrl url = address.isEmpty() ? QUrl() : addressUrl(address);
    QString profile = "shared";
    for (const auto &w : m_workspaces)
        if (w.toObject().value("id") == m_workspace && w.toObject().value("isolated").toBool())
            profile = m_workspace;
    if (m_private)
        profile = "private-" + m_privateContext + "-" + profile;
    m_tabs.append(QJsonObject{{"profile", profile},
                              {"id", m_active},
                              {"workspace", m_workspace},
                              {"private", m_private},
                              {"url", url.toString()},
                              {"title", tr("New tab")},
                              {"lastUsed", double(now())},
                              {"sleeping", false}});
    m_saveTimer.start();
    emit changed();
    return m_active;
}
void MobileModel::selectTab(const QString &key) {
    auto index = tabIndex(key);
    if (index < 0)
        return;
    auto t = m_tabs[index].toObject();
    m_active = key;
    m_workspace = t.value("workspace").toString();
    m_private = t.value("private").toBool();
    t["sleeping"] = false;
    t["lastUsed"] = double(now());
    m_tabs[index] = t;
    m_activity.restart();
    m_focus = {};
    m_saveTimer.start();
    emit changed();
}
void MobileModel::closeTab(const QString &key) {
    auto index = tabIndex(key);
    if (index < 0)
        return;
    m_previews.remove(key);
    emit previewsChanged();
    m_tabs.removeAt(index);
    bool remainingPrivate = false;
    for (auto value : m_tabs)
        if (value.toObject().value("private").toBool())
            remainingPrivate = true;
    if (!remainingPrivate) {
        m_privateContext = id();
        m_locked = false;
    }
    if (m_active == key)
        m_active = {};
    ensureTab();
    m_saveTimer.start();
    emit changed();
}
QUrl MobileModel::addressUrl(const QString &input) const {
    const auto text = input.trimmed();
    if (text.isEmpty())
        return {};
    const bool local = text.startsWith("localhost") || text.startsWith("127.") || text.startsWith("[::1]");
    auto candidate =
        QUrl(text.contains("://") ? text : (local ? "http://" : "https://") + text, QUrl::StrictMode);
    if (!text.contains(QRegularExpression("\\s")) &&
        (candidate.scheme() == "https" || candidate.scheme() == "http") && !candidate.host().isEmpty() &&
        candidate.userInfo().isEmpty() && (candidate.host().contains('.') || local || text.contains("://")))
        return candidate;
    QUrl search;
    auto engine = m_settings.value("searchEngine").toString();
    search = QUrl(engine == "Google"  ? "https://www.google.com/search"
                  : engine == "Bing"  ? "https://www.bing.com/search"
                  : engine == "Brave" ? "https://search.brave.com/search"
                                      : "https://duckduckgo.com/");
    QUrlQuery query;
    query.addQueryItem("q", text);
    search.setQuery(query);
    return search;
}
void MobileModel::navigate(const QString &input) {
    if (m_private && m_locked)
        return;
    const auto url = addressUrl(input);
    if (!url.isValid() || url.isEmpty())
        return;
    auto index = tabIndex(m_active);
    if (index < 0)
        return;
    auto t = m_tabs[index].toObject();
    t["url"] = url.toString();
    t["sleeping"] = false;
    t["dirty"] = false;
    t["lastUsed"] = double(now());
    m_tabs[index] = t;
    m_focus = {};
    m_pending = {};
    m_activity.restart();
    m_saveTimer.start();
    emit changed();
    emit pageAction(m_active, "navigate", url.toString());
}
void MobileModel::captureActive() {
    if (m_private && m_locked)
        return;
    if (auto page = NativePage::pages.value(m_active))
        page->capture();
    else
        emit pageAction(m_active, "capture", QString());
}
void MobileModel::setPreview(const QString &key, const QImage &image) {
    const auto tab = page(key);
    if (tab.isEmpty() || (tab.value("private").toBool() && m_locked) || image.isNull() ||
        image.width() > 8192 || image.height() > 8192)
        return;
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    image.scaledToWidth(qMin(640, image.width()), Qt::SmoothTransformation).save(&buffer, "PNG");
    m_previews[key] = "data:image/png;base64," + QString::fromLatin1(bytes.toBase64());
    emit previewsChanged();
}
void MobileModel::cycleTab(int direction) {
    QStringList ids;
    for (auto value : m_tabs) {
        auto t = value.toObject();
        if (t.value("workspace") == m_workspace && t.value("private").toBool() == m_private)
            ids.append(t.value("id").toString());
    }
    if (ids.size() < 2)
        return;
    const auto current = ids.indexOf(m_active);
    selectTab(ids[(current + (direction < 0 ? -1 : 1) + ids.size()) % ids.size()]);
}
void MobileModel::findText(const QString &text, bool backwards) {
    if (auto page = NativePage::pages.value(m_active))
        page->find(text.left(4096), backwards);
    else
        emit pageAction(
            m_active, "find",
            QString::fromUtf8(QJsonDocument(QJsonObject{{"query", text.left(4096)}, {"backwards", backwards}})
                                  .toJson(QJsonDocument::Compact)));
}
void MobileModel::updatePage(const QString &key, const QString &type, const QVariantMap &values) {
    if (type == "preview") {
        const auto tab = page(key);
        const auto image = values.value("image").toString();
        if (!tab.isEmpty() && !(tab.value("private").toBool() && m_locked) &&
            image.startsWith("data:image/png;base64,") && image.size() < 8 * 1024 * 1024) {
            m_previews[key] = image;
            emit previewsChanged();
        }
        return;
    }
    auto index = tabIndex(key);
    if (index < 0)
        return;
    auto t = m_tabs[index].toObject();
    const auto data = QJsonObject::fromVariantMap(values);
    const auto origin = [](QUrl url) {
        if ((url.scheme() == "https" && url.port() == 443) || (url.scheme() == "http" && url.port() == 80))
            url.setPort(-1);
        return QString::fromLatin1(url.adjusted(QUrl::RemovePath | QUrl::RemoveQuery | QUrl::RemoveFragment)
                                       .toEncoded(QUrl::RemovePassword | QUrl::StripTrailingSlash));
    };
    if (type == "location") {
        t["url"] = data.value("url");
        t["back"] = data.value("back");
        t["forward"] = data.value("forward");
        t["dirty"] = false;
    }
    if (type == "title")
        t["title"] = data.value("title");
    if (type == "progress")
        t["progress"] = data.value("progress");
    if (type == "start")
        t["loading"] = true;
    if (type == "activity" && key == m_active) {
        m_activity.restart();
        t["lastUsed"] = double(now());
    }
    if (type == "activityState") {
        if (data.contains("audio"))
            t["audio"] = data.value("audio");
        if (data.contains("dirty"))
            t["dirty"] = data.value("dirty");
    }
    if (type == "credentialFocus" && key == m_active && m_settings.value("suggestPasswords").toBool() &&
        QUrl(t.value("url").toString()).scheme() == "https" &&
        origin(QUrl(t.value("url").toString())) == data.value("origin").toString()) {
        m_focus = data;
        m_focus["tab"] = key;
    }
    if (type == "loginSubmitted" && key == m_active && !m_private &&
        m_settings.value("savePasswords").toBool() && QUrl(t.value("url").toString()).scheme() == "https" &&
        origin(QUrl(t.value("url").toString())) == data.value("origin").toString()) {
        m_pendingTimer.start();
        m_pending = data;
        m_pending["tab"] = key;
        m_pending["time"] = double(now());
    }
    if (type == "finish" || type == "error")
        t["loading"] = false;
    if (type == "error") {
        if (data.value("crashed").toBool())
            t["sleeping"] = true;
        t["error"] = data.value("message");
        emit message(data.value("message").toString());
    }
    m_tabs[index] = t;
    if (type == "finish" && key == m_active)
        QTimer::singleShot(120, this, &MobileModel::captureActive);
    if (type == "finish" && !t.value("private").toBool() &&
        QUrl(t.value("url").toString()).scheme().startsWith("http")) {
        for (int i = m_history.size() - 1; i >= 0; --i)
            if (m_history[i].toObject().value("url") == t.value("url"))
                m_history.removeAt(i);
        m_history.prepend(
            QJsonObject{{"url", t.value("url")}, {"title", t.value("title")}, {"time", double(now())}});
        while (m_history.size() > 5000)
            m_history.removeLast();
    }
    if ((type == "finish" || type == "loginSucceeded") && m_pending.value("tab") == key &&
        now() - qint64(m_pending.value("time").toDouble()) < 45000) {
        QPointer<MobileModel> self(this);
        auto candidate = m_pending;
        if (auto native = NativePage::pages.value(key))
            native->evaluate("globalThis.__arbiterHasPassword?.() ?? false",
                             [self, candidate](QVariant hasPassword) {
                                 if (self && !hasPassword.toBool() && self->m_pending == candidate)
                                     emit self->loginOffered(candidate.value("origin").toString(),
                                                             candidate.value("username").toString());
                             });
#ifdef ARBITER_MOBILE_WEBENGINE
        else
            emit pageAction(key, "inspectLogin", QString());
#endif
    }
    if (type == "loginInspection" && !data.value("hasPassword").toBool() && m_pending.value("tab") == key)
        emit loginOffered(m_pending.value("origin").toString(), m_pending.value("username").toString());
    if (type == "download") {
        auto record = data;
        record["id"] = key + ":" + data.value("id").toString();
        if (!t.value("private").toBool())
            m_downloads.append(record);
    }
    if (type == "downloadProgress") {
        for (int i = 0; i < m_downloads.size(); ++i) {
            auto d = m_downloads[i].toObject();
            if (d.value("id").toString() == key + ":" + data.value("id").toString()) {
                for (auto j = data.begin(); j != data.end(); ++j)
                    if (j.key() != "id")
                        d[j.key()] = j.value();
                m_downloads[i] = d;
                break;
            }
        }
    }
    m_saveTimer.start();
    emit changed();
}
void MobileModel::selectWorkspace(const QString &key) {
    for (const auto &w : m_workspaces)
        if (w.toObject().value("id") == key) {
            m_workspace = key;
            m_active = {};
            ensureTab();
            m_saveTimer.start();
            emit changed();
            return;
        }
}
void MobileModel::addWorkspace(const QString &name, bool isolated) {
    if (name.trimmed().isEmpty())
        return;
    m_workspace = id();
    m_workspaces.append(QJsonObject{{"id", m_workspace},
                                    {"name", name.trimmed().left(80)},
                                    {"color", "#ab4dff"},
                                    {"isolated", isolated}});
    m_active = {};
    ensureTab();
    m_saveTimer.start();
    emit changed();
}
void MobileModel::renameWorkspace(const QString &key, const QString &name) {
    for (int i = 0; i < m_workspaces.size(); ++i) {
        auto w = m_workspaces[i].toObject();
        if (w.value("id") == key && !name.trimmed().isEmpty()) {
            w["name"] = name.trimmed().left(80);
            m_workspaces[i] = w;
        }
    }
    m_saveTimer.start();
    emit changed();
}
void MobileModel::removeWorkspace(const QString &key) {
    if (m_workspaces.size() < 2)
        return;
    for (int i = m_workspaces.size() - 1; i >= 0; --i)
        if (m_workspaces[i].toObject().value("id") == key)
            m_workspaces.removeAt(i);
    for (int i = m_tabs.size() - 1; i >= 0; --i)
        if (m_tabs[i].toObject().value("workspace") == key)
            m_tabs.removeAt(i);
    if (m_workspace == key)
        m_workspace = m_workspaces[0].toObject().value("id").toString();
    ensureTab();
    m_saveTimer.start();
    emit changed();
}
void MobileModel::newPrivateTab() {
    bool existing = false;
    for (auto value : m_tabs) {
        const auto tab = value.toObject();
        if (tab.value("private").toBool() && tab.value("workspace") == m_workspace)
            existing = true;
    }
    if (!m_private) {
        togglePrivate();
        if (existing)
            addTab();
    } else
        addTab();
}
void MobileModel::togglePrivate() {
    bool existing = false;
    for (auto value : m_tabs)
        if (value.toObject().value("private").toBool())
            existing = true;
    m_private = !m_private;
    m_active = {};
    m_focus = {};
    m_pending = {};
    ensureTab();
    if (m_private && !existing)
        m_locked = false;
    MobilePlatform::secureScreen(m_private);
    m_activity.restart();
    emit changed();
}
void MobileModel::lockPrivate() {
    bool existing = false;
    for (auto value : m_tabs)
        if (value.toObject().value("private").toBool())
            existing = true;
    if (!existing)
        return;
    m_locked = true;
    for (auto value : m_tabs) {
        auto tab = value.toObject();
        if (tab.value("private").toBool())
            m_previews.remove(tab.value("id").toString());
    }
    emit previewsChanged();
    m_pending = {};
    m_focus = {};
    emit changed();
}
void MobileModel::unlockPrivate() {
    verificationActive = true;
    QPointer<MobileModel> self(this);
    MobilePlatform::verify(tr("Unlock private browsing"), [self](bool approved) {
        if (!self)
            return;
        self->verificationActive = false;
        if (approved) {
            self->m_locked = false;
            self->m_activity.restart();
            emit self->changed();
        }
    });
}
void MobileModel::setSetting(const QString &name, const QVariant &value) {
    if (!m_settings.contains(name) &&
        !QStringList{"lightImage", "darkImage", "adblockDisabledHosts"}.contains(name))
        return;
    const auto setting = QJsonValue::fromVariant(value);
    if (!validSetting(name, setting))
        return;
    {
        QMutexLocker lock(&m_rulesMutex);
        m_settings[name] = setting;
    }
    m_vault.setCooldown(m_settings.value("passwordUnlockSeconds").toInt(300));
    if (name == "language")
        MobilePlatform::setLanguage(value.toString());
    if (name == "backgroundSync" || name == "onboardingDone")
        MobilePlatform::configureBackgroundSync(onboardingDone() &&
                                                m_settings.value("backgroundSync").toBool(true));
    for (auto page : NativePage::pages)
        if (page)
            page->configure();
    m_saveTimer.start();
    emit settingsChanged();
}
bool MobileModel::blockingFor(const QUrl &url) const {
    return m_settings.value("adblockEnabled").toBool(true) &&
           !m_settings.value("adblockDisabledHosts").toArray().contains(url.host().toLower());
}
bool MobileModel::blockRequest(const QUrl &url, const QUrl &source, const QByteArray &type,
                               const QByteArray &method) {
#ifdef Q_OS_IOS
    Q_UNUSED(url);
    Q_UNUSED(source);
    Q_UNUSED(type);
    Q_UNUSED(method);
    return false;
#else
    QMutexLocker lock(&m_rulesMutex);
    return blockingFor(source) && m_rules &&
           arbiter_adblock_check(m_rules, url.toEncoded().constData(), source.toEncoded().constData(),
                                 type.constData(), method.constData());
#endif
}
void MobileModel::toggleSiteBlocking() {
    const auto host = QUrl(page(m_active).value("url").toString()).host().toLower();
    if (host.isEmpty())
        return;
    auto hosts = m_settings.value("adblockDisabledHosts").toArray();
    if (hosts.contains(host)) {
        for (int i = hosts.size() - 1; i >= 0; --i)
            if (hosts[i] == host)
                hosts.removeAt(i);
    } else {
        hosts.append(host);
    }
    setSetting("adblockDisabledHosts", hosts.toVariantList());
    emit pageAction(m_active, "reload", QString());
}
QVariantList MobileModel::suggestions(const QString &query) const {
    QVariantList out;
    QSet<QString> seen;
    for (const auto &list : {m_tabs, m_history})
        for (const auto &v : list) {
            auto d = v.toObject();
            if (d.value("private").toBool() && !m_private)
                continue;
            auto url = d.value("url").toString();
            if (url.isEmpty() || seen.contains(url) ||
                (!query.isEmpty() && !url.contains(query, Qt::CaseInsensitive) &&
                 !d.value("title").toString().contains(query, Qt::CaseInsensitive)))
                continue;
            seen.insert(url);
            out.append(QVariantMap{
                {"url", url}, {"title", d.value("title").toString()}, {"tab", d.value("id").toString()}});
            if (out.size() >= 10)
                return out;
        }
    if (query.isEmpty())
        for (const auto &site :
             QList<QPair<QString, QString>>{{"Wikipedia", "https://www.wikipedia.org"},
                                            {"GitHub", "https://github.com"},
                                            {"YouTube", "https://www.youtube.com"},
                                            {"Hacker News", "https://news.ycombinator.com"}})
            if (!seen.contains(site.second))
                out.append(QVariantMap{{"title", site.first}, {"url", site.second}});
    return out;
}
void MobileModel::clearHistory(const QString &query) {
    for (int i = m_history.size() - 1; i >= 0; --i) {
        auto v = m_history[i].toObject();
        if (query.isEmpty() || v.value("url").toString().contains(query, Qt::CaseInsensitive) ||
            v.value("title").toString().contains(query, Qt::CaseInsensitive))
            m_history.removeAt(i);
    }
    m_saveTimer.start();
    emit changed();
}
void MobileModel::removeHistory(const QString &url) {
    for (int i = m_history.size() - 1; i >= 0; --i)
        if (m_history[i].toObject().value("url") == url)
            m_history.removeAt(i);
    m_saveTimer.start();
    emit changed();
}
void MobileModel::downloadEvent(const QString &type, const QVariantMap &data, bool privateDownload) {
    if (privateDownload)
        return;
    auto record = QJsonObject::fromVariantMap(data);
    const auto key = record.value("id").toString();
    if (key.isEmpty())
        return;
    if (type == "download") {
        for (const auto &v : m_downloads)
            if (v.toObject().value("id") == key)
                return;
        record["time"] = double(now());
        m_downloads.prepend(record);
    } else {
        for (int i = 0; i < m_downloads.size(); ++i) {
            auto d = m_downloads[i].toObject();
            if (d.value("id") != key)
                continue;
            for (auto j = record.begin(); j != record.end(); ++j)
                d[j.key()] = j.value();
            const auto received = qint64(d.value("received").toDouble()),
                       total = qint64(d.value("total").toDouble()),
                       speed = qint64(d.value("speed").toDouble());
            QString status = d.value("complete").toBool() ? tr("Completed")
                             : d.value("error").toBool()  ? tr("Download failed")
                                                          : tr("Downloading…");
            status += " · " + QLocale().formattedDataSize(received);
            if (total > 0)
                status += " / " + QLocale().formattedDataSize(total);
            if (speed > 0 && !d.value("complete").toBool()) {
                status += " · " + QLocale().formattedDataSize(speed) + "/s";
                if (total > received)
                    status += " · " + tr("%1 s remaining").arg((total - received) / speed);
            }
            d["status"] = status;
            m_downloads[i] = d;
            break;
        }
    }
    m_saveTimer.start();
    emit changed();
}
void MobileModel::share() {
    auto url = QUrl(page(m_active).value("url").toString());
    if (url.scheme().startsWith("http"))
        MobilePlatform::share(url);
}
void MobileModel::openDownload(const QString &key) {
    for (const auto &v : m_downloads)
        if (v.toObject().value("id") == key)
            QDesktopServices::openUrl(QUrl(v.toObject().value("path").toString()));
}
void MobileModel::saveLogin(const QString &username) {
    if (m_private || m_pending.isEmpty() || m_pending.value("tab") != m_active ||
        now() - qint64(m_pending.value("time").toDouble()) > 45000)
        return;
    m_vault.savePassword(m_pending.value("origin").toString(), username,
                         m_pending.value("password").toString());
    m_pending = {};
}
void MobileModel::dismissLogin() {
    m_pending = {};
}
void MobileModel::passwordAccounts() {
    if (m_focus.value("tab") != m_active) {
        emit message(tr("Tap a username or password field first."));
        return;
    }
    connect(
        &m_vault, &MobileVault::changed, this,
        [this] {
            if (!m_vault.unlocked())
                return;
            QVariantList accounts;
            for (const auto &v : m_vault.entries()) {
                auto d = v.toMap();
                if (d.value("kind") == "password" &&
                    d.value("origin").toString() == m_focus.value("origin").toString())
                    accounts.append(v);
            }
            emit accountsOffered(accounts);
        },
        Qt::SingleShotConnection);
    m_vault.unlock();
}
void MobileModel::fillPassword(const QString &key) {
    const auto tab = m_active;
    const auto origin = m_focus.value("origin").toString();
    m_vault.readPassword(key, origin, [this, tab, origin](QString user, QString password) {
        if (tab != m_active || m_focus.value("origin") != origin)
            return;
        const auto script =
            "(()=>{if(location.origin!==" +
            QString::fromUtf8(QJsonDocument(QJsonArray{origin}).toJson(QJsonDocument::Compact))
                .mid(1)
                .chopped(1) +
            ")return false;return globalThis.__arbiterFill?.(" +
            QString::fromUtf8(
                QJsonDocument(QJsonObject{{"origin", origin}, {"username", user}, {"password", password}})
                    .toJson(QJsonDocument::Compact)) +
            ")??false})()";
        if (auto native = NativePage::pages.value(tab))
            native->evaluate(script);
        else
            emit pageAction(tab, "script", script);
    });
}
void MobileModel::suggestPassword() {
    if (m_focus.value("tab") != m_active)
        return;
    const auto origin = m_focus.value("origin").toString();
    const auto script =
        "globalThis.__arbiterFill?.(" +
        QString::fromUtf8(QJsonDocument(QJsonObject{{"origin", origin},
                                                    {"generate", true},
                                                    {"password", MobileVault::generatePassword()}})
                              .toJson(QJsonDocument::Compact)) +
        ")";
    if (auto native = NativePage::pages.value(m_active))
        native->evaluate(script);
    else
        emit pageAction(m_active, "script", script);
}
void MobileModel::copy(const QString &text) {
    QGuiApplication::clipboard()->setText(text);
}
void MobileModel::chooseBackground(const QUrl &url, bool dark) {
    if (!url.isLocalFile())
        return;
    QFile file(url.toLocalFile());
    if (file.size() > 16 * 1024 * 1024 || !file.open(QIODevice::ReadOnly)) {
        emit message(tr("Could not open image"));
        return;
    }
    auto path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) +
                (dark ? "/dark-background" : "/light-background") + QFileInfo(file).suffix().prepend('.');
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile output(path);
    if (!output.open(QIODevice::WriteOnly) || output.write(file.readAll()) < 0 || !output.commit())
        return;
    setSetting(dark ? "darkImage" : "lightImage", QUrl::fromLocalFile(path).toString());
}
void MobileModel::setDefaultBrowser() {
    emit defaultBrowserRequested();
}
void MobileModel::enableCredentialProvider() {
    emit credentialProviderRequested();
}
QString MobileModel::formScript(bool privateMode) const {
    QFile file(":/browser/forms.js");
    if (!file.open(QIODevice::ReadOnly))
        return {};
    QString script =
        "globalThis.__arbiterPreferences=" +
        QString::fromUtf8(
            QJsonDocument(
                QJsonObject{{"savePasswords", m_settings.value("savePasswords").toBool() && !privateMode},
                            {"suggestPasswords", m_settings.value("suggestPasswords").toBool()}})
                .toJson(QJsonDocument::Compact)) +
        ";\n" + QString::fromUtf8(file.readAll());
#ifdef ARBITER_MOBILE_WEBENGINE
    QFile channel(":/qtwebchannel/qwebchannel.js");
    if (channel.open(QIODevice::ReadOnly))
        return QString::fromUtf8(channel.readAll()) +
               "\nnew "
               "QWebChannel(qt.webChannelTransport,channel=>{globalThis.__arbiterSend=message=>channel."
               "objects.arbiterForms.submit(message);" +
               script + "});";
#endif
    return script;
}
void MobileModel::sleepIdle() {
    auto seconds = m_settings.value("tabSleepSeconds").toInt(600);
    if (seconds <= 0)
        return;
    bool changedAny = false;
    for (int i = 0; i < m_tabs.size(); ++i) {
        auto t = m_tabs[i].toObject();
        if (t.value("id") == m_active || t.value("sleeping").toBool() || t.value("audio").toBool() ||
            t.value("dirty").toBool() || t.value("loading").toBool() ||
            now() - qint64(t.value("lastUsed").toDouble()) < seconds * 1000)
            continue;
        if (auto native = NativePage::pages.value(t.value("id").toString())) {
            const auto key = t.value("id").toString();
            QPointer<MobileModel> self(this);
            MobilePlatform::mediaPlaying(native, [self, key](bool playing) {
                if (!self || playing || self->m_active == key)
                    return;
                const auto index = self->tabIndex(key);
                if (index < 0)
                    return;
                auto tab = self->m_tabs[index].toObject();
                const auto seconds = self->m_settings.value("tabSleepSeconds").toInt(600);
                if (seconds <= 0 || tab.value("audio").toBool() || tab.value("dirty").toBool() ||
                    tab.value("loading").toBool() ||
                    now() - qint64(tab.value("lastUsed").toDouble()) < seconds * 1000)
                    return;
                tab["sleeping"] = true;
                self->m_tabs[index] = tab;
                emit self->changed();
            });
            continue;
        }
        t["sleeping"] = true;
        m_tabs[i] = t;
        changedAny = true;
    }
    if (changedAny)
        emit changed();
}
void MobileModel::backgroundSync(std::function<void(bool)> done) {
#ifdef ARBITER_MOBILE_DESIGN_PREVIEW
    if (property("_designPreview").toBool()) {
        if (done)
            done(false);
        return;
    }
#endif
    if (m_syncing || !onboardingDone()) {
        if (done)
            done(false);
        return;
    }
    m_syncing = true;
    auto worker = new MobileVault(this);
    m_backgroundWorker = worker;
    worker->backgroundSync([this, worker, done = std::move(done)](bool success) {
        m_syncing = false;
        m_backgroundWorker = nullptr;
        worker->lock();
        worker->deleteLater();
        if (done)
            done(success);
    });
}
