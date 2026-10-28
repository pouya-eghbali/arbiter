#include "updatetracker.h"
#include "settings.h"
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QRegularExpression>

namespace {
constexpr qint64 maximumPayload=256*1024;
struct Version { QStringList core, prerelease; };
std::optional<Version> parse(QString text) {
    if(text.startsWith('v')) text.remove(0,1);
    if(text.size()>128) return {};
    static const QRegularExpression pattern(QStringLiteral(
        "^(0|[1-9][0-9]*)\\.(0|[1-9][0-9]*)\\.(0|[1-9][0-9]*)"
        "(?:-([0-9A-Za-z-]+(?:\\.[0-9A-Za-z-]+)*))?(?:\\+[0-9A-Za-z-]+(?:\\.[0-9A-Za-z-]+)*)?\\z"));
    const auto match=pattern.match(text);
    if(!match.hasMatch()) return {};
    Version version{{match.captured(1),match.captured(2),match.captured(3)}, {}};
    if(!match.captured(4).isEmpty()) version.prerelease=match.captured(4).split('.');
    static const QRegularExpression numeric(QStringLiteral("^[0-9]+$"));
    for(const auto &part:version.prerelease)
        if(numeric.match(part).hasMatch() && part.size()>1 && part.startsWith('0')) return {};
    return version;
}
int compareNumber(const QString &a,const QString &b) {
    if(a.size()!=b.size()) return a.size()<b.size() ? -1 : 1;
    return QString::compare(a,b,Qt::CaseSensitive);
}
}

std::optional<int> UpdateTracker::compareVersions(const QString &left,const QString &right) {
    const auto a=parse(left),b=parse(right);
    if(!a || !b) return {};
    for(int i=0;i<3;++i) if(const auto order=compareNumber(a->core[i],b->core[i])) return order;
    if(a->prerelease.isEmpty() || b->prerelease.isEmpty())
        return a->prerelease.isEmpty()==b->prerelease.isEmpty() ? 0 : a->prerelease.isEmpty() ? 1 : -1;
    static const QRegularExpression numeric(QStringLiteral("^[0-9]+$"));
    for(int i=0;i<qMin(a->prerelease.size(),b->prerelease.size());++i) {
        const auto &x=a->prerelease[i], &y=b->prerelease[i];
        const bool xn=numeric.match(x).hasMatch(),yn=numeric.match(y).hasMatch();
        const auto order=xn && yn ? compareNumber(x,y) : xn!=yn ? (xn ? -1 : 1) : QString::compare(x,y,Qt::CaseSensitive);
        if(order) return order;
    }
    return a->prerelease.size()==b->prerelease.size() ? 0 : a->prerelease.size()<b->prerelease.size() ? -1 : 1;
}

UpdateTracker::UpdateTracker(QObject *parent):UpdateTracker(QCoreApplication::applicationVersion(),
    QUrl(QStringLiteral("https://api.github.com/repos/pouya-eghbali/arbiter/releases/latest")),parent) {}

UpdateTracker::~UpdateTracker() {
    if(m_reply) { m_reply->disconnect(this); m_reply->abort(); }
}

UpdateTracker::UpdateTracker(const QString &installedVersion,const QUrl &endpoint,QObject *parent)
    :QObject(parent),m_current(installedVersion),m_endpoint(endpoint) {
    Settings settings;
    m_automatic=settings.value("Updates/automatic",true).toBool();
    m_lastAttempt=QDateTime::fromString(settings.value("Updates/lastAttempt").toString(),Qt::ISODate);
    m_lastChecked=QDateTime::fromString(settings.value("Updates/lastChecked").toString(),Qt::ISODate);
    const auto cached=settings.value("Updates/latestTag").toString();
    if(const auto version=parse(cached); version && version->prerelease.isEmpty()) {
        m_tag=cached;
        m_latest=cached.startsWith('v') ? cached.mid(1) : cached;
        updateState();
    }
    m_schedule.setInterval(60*60*1000);
    connect(&m_schedule,&QTimer::timeout,this,&UpdateTracker::checkAutomatically);
    m_deadline.setSingleShot(true);
    m_deadline.setInterval(20000);
    connect(&m_deadline,&QTimer::timeout,this,[this] { if(m_reply) { fail(tr("The update check timed out. Try again later.")); m_reply->abort(); } });
}
void UpdateTracker::setAutomatic(bool enabled) {
    if(m_automatic==enabled) return;
    Settings settings; settings.setValue("Updates/automatic",enabled);
    if(settings.status()!=QSettings::NoError) { fail(tr("Could not save the update preference.")); return; }
    m_automatic=enabled;
    if(m_started) {
        if(enabled) { m_schedule.start(); QTimer::singleShot(10000,this,&UpdateTracker::checkAutomatically); }
        else m_schedule.stop();
    }
    emit changed();
}
void UpdateTracker::start() {
    if(m_started) return;
    m_started=true;
    if(m_automatic) { m_schedule.start(); QTimer::singleShot(10000,this,&UpdateTracker::checkAutomatically); }
}
void UpdateTracker::checkAutomatically() {
    if(!m_automatic) return;
    const auto elapsed=m_lastAttempt.secsTo(QDateTime::currentDateTimeUtc());
    if(!m_lastAttempt.isValid() || elapsed<0 || elapsed>=24*60*60) check();
}
QUrl UpdateTracker::releaseUrl() const {
    if(m_tag.isEmpty()) return {};
    return QUrl(QStringLiteral("https://github.com/pouya-eghbali/arbiter/releases/tag/")+QString::fromLatin1(QUrl::toPercentEncoding(m_tag)));
}
void UpdateTracker::updateState() {
    const auto order=compareVersions(m_latest,m_current);
    m_state=!order ? "error" : *order>0 ? "available" : *order<0 ? "ahead" : "current";
    if(!order) m_error=tr("Could not compare the installed and released versions.");
}
void UpdateTracker::fail(const QString &error) { m_error=error; m_state="error"; emit changed(); }
void UpdateTracker::check() {
    if(m_reply) return;
    m_lastAttempt=QDateTime::currentDateTimeUtc();
    Settings().setValue("Updates/lastAttempt",m_lastAttempt.toString(Qt::ISODate));
    m_state="checking"; m_error.clear(); m_payload.clear(); emit changed();
    QNetworkRequest request(m_endpoint);
    request.setRawHeader("User-Agent","Arbiter-Update-Check");
    request.setRawHeader("Accept","application/vnd.github+json");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::ManualRedirectPolicy);
    request.setAttribute(QNetworkRequest::CookieLoadControlAttribute,QNetworkRequest::Manual);
    request.setAttribute(QNetworkRequest::CookieSaveControlAttribute,QNetworkRequest::Manual);
    request.setAttribute(QNetworkRequest::AuthenticationReuseAttribute,QNetworkRequest::Manual);
    request.setTransferTimeout(15000);
    auto *reply=m_network.get(request); m_reply=reply;
    reply->setReadBufferSize(maximumPayload+1);
    connect(reply,&QIODevice::readyRead,this,[this,reply] {
        m_payload+=reply->readAll();
        if(m_payload.size()>maximumPayload) { fail(tr("The update service returned an invalid response.")); reply->abort(); }
    });
    connect(reply,&QNetworkReply::finished,this,[this,reply] {
        m_deadline.stop(); m_reply.clear(); reply->deleteLater();
        if(m_state=="error") return;
        m_payload+=reply->readAll();
        const auto status=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if(status==404) { fail(tr("No stable release is available yet.")); return; }
        if(status==403 || status==429) { fail(tr("GitHub is limiting update checks. Try again later.")); return; }
        if(reply->error()!=QNetworkReply::NoError || status!=200) { fail(tr("Could not check for updates. Check your connection and try again.")); return; }
        const auto document=QJsonDocument::fromJson(m_payload);
        const auto object=document.object();
        const auto tag=object.value("tag_name").toString();
        const auto version=parse(tag);
        if(m_payload.size()>maximumPayload || !document.isObject() || !version || !version->prerelease.isEmpty()
            || !object.value("draft").isBool() || object.value("draft").toBool()
            || !object.value("prerelease").isBool() || object.value("prerelease").toBool()) {
            fail(tr("The update service returned an invalid response.")); return;
        }
        m_latest=tag.startsWith('v') ? tag.mid(1) : tag;
        m_tag=tag;
        m_lastChecked=QDateTime::currentDateTimeUtc();
        Settings settings; settings.setValue("Updates/latestTag",m_tag); settings.setValue("Updates/lastChecked",m_lastChecked.toString(Qt::ISODate));
        updateState(); emit changed();
    });
    m_deadline.start();
}
