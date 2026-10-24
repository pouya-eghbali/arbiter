#include "adblocker.h"
#include "settings.h"
#include <QWebEngineUrlRequestInfo>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QSaveFile>
#include <QStandardPaths>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QFutureWatcher>
#include <QtConcurrent>
#include <QTimer>
#include <QDateTime>
extern "C" {
void *arbiter_adblock_new(const char *);
void arbiter_adblock_free(void *);
bool arbiter_adblock_check(const void *,const char *,const char *,const char *,const char *);
char *arbiter_adblock_cosmetic(const void *,const char *,const char *,const char *);
void arbiter_adblock_string_free(char *);
}
AdBlockEngine::AdBlockEngine(const QByteArray &rules) : m_engine(arbiter_adblock_new(rules.constData())) {}
AdBlockEngine::~AdBlockEngine() { arbiter_adblock_free(m_engine); }
bool AdBlockEngine::blocks(const QUrl &url,const QUrl &source,const QByteArray &type,const QByteArray &method) const {
    if (!m_engine) return false;
    QMutexLocker lock(&m_mutex);
    return arbiter_adblock_check(m_engine,url.toEncoded().constData(),source.toEncoded().constData(),type.constData(),method.constData());
}
QStringList AdBlockEngine::cosmetic(const QUrl &url,const QStringList &classes,const QStringList &ids) const {
    if (!m_engine) return {};
    QMutexLocker lock(&m_mutex);
    char *result=arbiter_adblock_cosmetic(m_engine,url.toEncoded().constData(),QJsonDocument(QJsonArray::fromStringList(classes)).toJson(QJsonDocument::Compact).constData(),QJsonDocument(QJsonArray::fromStringList(ids)).toJson(QJsonDocument::Compact).constData());
    if (!result) return {};
    const auto entries=QJsonDocument::fromJson(QByteArray(result)).array();
    arbiter_adblock_string_free(result);
    QStringList selectors; for (const auto &entry:entries) selectors.append(entry.toString());
    return selectors;
}
QString AdBlocker::cosmeticScript() const {
    QFile file(":/adblock/adblock.js"); return file.open(QIODevice::ReadOnly) ? QString::fromUtf8(file.readAll()) : QString();
}
QByteArray AdBlocker::bundledRules() {
    QByteArray rules;
    for (const auto &name:{"easylist.txt","easyprivacy.txt"}) { QFile file(QString(":/adblock/")+name); if (file.open(QIODevice::ReadOnly)) rules+=file.readAll()+"\n"; }
    return rules;
}
QString AdBlocker::cacheFile() const { return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)+"/adblock/rules.txt"; }
AdBlocker::AdBlocker(QObject *parent) : AdBlocker(QByteArray(),parent) {}
AdBlocker::AdBlocker(const QByteArray &customRules,QObject *parent) : QObject(parent) {
    if (!customRules.isEmpty()) {
        m_engine=std::make_shared<AdBlockEngine>(customRules);
        m_status=tr("Using supplied filter rules.");
        return;
    }
    m_enabled=Settings().value("AdBlock/enabled",true).toBool();
    QFile cache(cacheFile());
    const bool cached=cache.size()>100 && cache.size()<16*1024*1024 && cache.open(QIODevice::ReadOnly);
    const QByteArray rules=cached ? cache.readAll() : bundledRules();
    if (!rules.isEmpty()) m_engine=std::make_shared<AdBlockEngine>(rules);
    m_status=ready() ? (cached ? tr("Using downloaded EasyList + EasyPrivacy.") : tr("Using bundled EasyList + EasyPrivacy.")) : tr("Filter lists could not be loaded.");
    if (enabled() && (!cached || QFileInfo(cache).lastModified().daysTo(QDateTime::currentDateTime())>=4))
        QTimer::singleShot(1500,this,&AdBlocker::updateLists);
}
bool AdBlocker::ready() const { auto current=engine(); return current && current->valid(); }
std::shared_ptr<AdBlockEngine> AdBlocker::engine() const { QMutexLocker lock(&m_mutex); return m_engine; }
void AdBlocker::setEnabled(bool value) {
    if (m_enabled.exchange(value)==value) return;
    Settings().setValue("AdBlock/enabled",value); emit enabledChanged();
    if (value && !m_updating && !QFileInfo::exists(cacheFile())) updateLists();
}
void AdBlocker::updateLists() {
    if (m_updating) return;
    m_updating=true; m_status=tr("Updating filter lists…"); emit listsChanged();
    struct Download { int remaining=2; QByteArray lists[2]; bool failed=false; };
    auto state=std::make_shared<Download>();
    for (int i=0;i<2;++i) {
        QNetworkRequest request(QUrl(i==0 ? "https://easylist.to/easylist/easylist.txt" : "https://easylist.to/easylist/easyprivacy.txt"));
        request.setTransferTimeout(20000); request.setMaximumRedirectsAllowed(3);
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::SameOriginRedirectPolicy);
        auto *reply=m_network.get(request);
        connect(reply,&QNetworkReply::readyRead,reply,[reply] { if (reply->bytesAvailable()>8*1024*1024) reply->abort(); });
        connect(reply,&QNetworkReply::finished,this,[this,reply,state,i] {
            const QByteArray bytes=reply->readAll();
            if (reply->error()!=QNetworkReply::NoError || bytes.size()>8*1024*1024 || !bytes.startsWith("[Adblock") || !bytes.contains("! Title:")) state->failed=true;
            else state->lists[i]=bytes;
            reply->deleteLater();
            if (--state->remaining) return;
            if (state->failed) { m_updating=false; m_status=tr("Update failed; keeping the existing filter lists."); emit listsChanged(); return; }
            const QByteArray rules=state->lists[0]+"\n"+state->lists[1];
            auto *watcher=new QFutureWatcher<std::shared_ptr<AdBlockEngine>>(this);
            connect(watcher,&QFutureWatcherBase::finished,this,[this,watcher,rules] {
                auto fresh=watcher->result(); watcher->deleteLater();
                if (fresh && fresh->valid()) {
                    { QMutexLocker lock(&m_mutex); m_engine=std::move(fresh); }
                    QDir().mkpath(QFileInfo(cacheFile()).absolutePath()); QSaveFile cache(cacheFile());
                    const bool saved=cache.open(QIODevice::WriteOnly) && cache.write(rules)==rules.size() && cache.commit();
                    m_status=saved ? tr("EasyList + EasyPrivacy updated.") : tr("Lists updated for this session; the cache could not be saved.");
                } else m_status=tr("Update failed; keeping the existing filter lists.");
                m_updating=false; emit listsChanged();
            });
            watcher->setFuture(QtConcurrent::run([rules] { return std::make_shared<AdBlockEngine>(rules); }));
        });
    }
}
void ViewAdBlock::setManager(QObject *object) {
    auto *manager=qobject_cast<AdBlocker *>(object); if (m_manager==manager) return;
    if (m_manager) disconnect(m_manager,nullptr,this,nullptr);
    m_manager=manager;
    if (manager) {
        connect(manager,&AdBlocker::enabledChanged,this,&ViewAdBlock::stateChanged);
        connect(manager,&AdBlocker::listsChanged,this,&ViewAdBlock::stateChanged);
        connect(manager,&QObject::destroyed,this,[this] { m_manager=nullptr; emit stateChanged(); });
    }
    emit managerChanged(); emit stateChanged();
}
void ViewAdBlock::setPaused(bool value) { if(m_paused.exchange(value)!=value) emit stateChanged(); }
bool ViewAdBlock::enabled() const { return m_manager && m_manager->enabled() && !paused() && m_manager->ready(); }
void ViewAdBlock::resetCount() { m_blocked=0; emit blockedChanged(); }
QStringList ViewAdBlock::cosmetic(const QUrl &url,const QStringList &classes,const QStringList &ids) const {
    if (!enabled() || (url.scheme()!="https" && url.scheme()!="http")) return {};
    auto engine=m_manager->engine(); return engine->cosmetic(url,classes.mid(0,2048),ids.mid(0,2048));
}
void ViewAdBlock::interceptRequest(QWebEngineUrlRequestInfo &info) {
    if (!enabled()) return;
    using Info=QWebEngineUrlRequestInfo;
    const auto type=info.resourceType();
    // Do not prevent a user's top-level navigation.
    if (type==Info::ResourceTypeMainFrame || type==Info::ResourceTypeNavigationPreloadMainFrame) return;
    QByteArray kind="other";
    switch (type) {
    case Info::ResourceTypeSubFrame: case Info::ResourceTypeNavigationPreloadSubFrame: kind="subdocument"; break;
    case Info::ResourceTypeStylesheet: kind="stylesheet"; break;
    case Info::ResourceTypeScript: case Info::ResourceTypeWorker: case Info::ResourceTypeSharedWorker: case Info::ResourceTypeServiceWorker: case Info::ResourceTypeJson: kind="script"; break;
    case Info::ResourceTypeImage: case Info::ResourceTypeFavicon: kind="image"; break;
    case Info::ResourceTypeFontResource: kind="font"; break;
    case Info::ResourceTypeMedia: kind="media"; break;
    case Info::ResourceTypeXhr: kind="xmlhttprequest"; break;
    case Info::ResourceTypePing: kind="ping"; break;
    case Info::ResourceTypeWebSocket: kind="websocket"; break;
    case Info::ResourceTypeObject: kind="object"; break;
    default: break;
    }
    const QUrl source=info.firstPartyUrl().isEmpty() ? info.initiator() : info.firstPartyUrl();
    auto engine=m_manager->engine();
    if (engine->blocks(info.requestUrl(),source,kind,info.requestMethod())) {
        info.block(true);
        ++m_blocked;
        if (!m_notificationPending.exchange(true))
            QMetaObject::invokeMethod(this,[this] { m_notificationPending=false; emit blockedChanged(); },Qt::QueuedConnection);
    }
}
