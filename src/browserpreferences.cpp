#include "browserpreferences.h"
#include "settings.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QImageReader>
#include <QBuffer>
#include <QGuiApplication>
#include <QClipboard>
#include <QMimeData>
#include <QNetworkReply>
#include <QNetworkCookieJar>
#include <QSaveFile>
#include <QStandardPaths>

namespace {
const QStringList engines{"duckduckgo", "google", "bing", "brave", "ecosia", "startpage"};
// Icon requests are independent of page sessions and never retain cookies.
class NoCookies : public QNetworkCookieJar {
    bool setCookiesFromUrl(const QList<QNetworkCookie> &,const QUrl &) override { return false; }
};
}
BrowserPreferences::BrowserPreferences(QObject *parent) : QObject(parent) {
    m_engine=Settings().value("Search/engine","duckduckgo").toString();
    if (!engines.contains(m_engine)) m_engine="duckduckgo";
    m_network.setCookieJar(new NoCookies);
}
void BrowserPreferences::setSearchEngine(const QString &engine) {
    if (!engines.contains(engine) || m_engine==engine) return;
    m_engine=engine;
    Settings().setValue("Search/engine",engine);
    emit searchEngineChanged();
}
QUrl BrowserPreferences::normalizedShareUrl(const QUrl &url) {
    if (!url.isValid()) return {};
    const bool web=(url.scheme()=="https" || url.scheme()=="http") && !url.host().isEmpty();
    const bool file=url.isLocalFile() && !url.path().isEmpty();
    return web || file ? url.adjusted(QUrl::RemoveUserInfo) : QUrl();
}
bool BrowserPreferences::copySharedLink(const QUrl &url) {
    const QUrl link=normalizedShareUrl(url);
    auto *clipboard=QGuiApplication::clipboard();
    if (link.isEmpty() || !clipboard) return false;
    auto *mime=new QMimeData;
    mime->setUrls({link});
    mime->setText(link.toString(QUrl::FullyEncoded));
    clipboard->setMimeData(mime);
    return true;
}
QUrl BrowserPreferences::searchUrl(const QString &text) const {
    const QHash<QString,QString> urls{{"duckduckgo","https://duckduckgo.com/"},{"google","https://www.google.com/search"},
        {"bing","https://www.bing.com/search"},{"brave","https://search.brave.com/search"},
        {"ecosia","https://www.ecosia.org/search"},{"startpage","https://www.startpage.com/sp/search"}};
    QUrl url(urls.value(m_engine));
    const QString key=m_engine=="startpage" ? "query" : "q";
    url.setQuery(key+"="+QString::fromLatin1(QUrl::toPercentEncoding(text)),QUrl::StrictMode);
    return url;
}
QString BrowserPreferences::siteOrigin(const QUrl &url) const {
    if ((url.scheme()!="https" && url.scheme()!="http") || url.host().isEmpty()) return {};
    QUrl origin; origin.setScheme(url.scheme()); origin.setHost(url.host()); origin.setPort(url.port());
    return origin.toString(QUrl::FullyEncoded);
}
QUrl BrowserPreferences::favicon(const QUrl &url,bool allowNetwork) {
    const QString origin=siteOrigin(url);
    if (origin.isEmpty()) return {};
    const QString directory=QStandardPaths::writableLocation(QStandardPaths::CacheLocation)+"/favicons";
    const QString path=directory+"/"+QString::fromLatin1(QCryptographicHash::hash(origin.toUtf8(),QCryptographicHash::Sha256).toHex())+".png";
    if (QFileInfo::exists(path)) return QUrl::fromLocalFile(path);
    if (!allowNetwork || m_pending.contains(origin) || m_failed.contains(origin)) return {};
    QUrl icon(origin); icon.setPath("/favicon.ico");
    QNetworkRequest request(icon);
    request.setTransferTimeout(5000);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setAttribute(QNetworkRequest::AuthenticationReuseAttribute,QNetworkRequest::Manual);
    m_pending.insert(origin);
    auto *reply=m_network.get(request);
    connect(reply,&QNetworkReply::readyRead,reply,[reply] { if (reply->bytesAvailable()>1024*1024) reply->abort(); });
    connect(reply,&QNetworkReply::finished,this,[this,reply,origin,path,directory] {
        m_pending.remove(origin); m_failed.insert(origin);
        if (reply->error()==QNetworkReply::NoError && reply->bytesAvailable()<=1024*1024) {
            QByteArray bytes=reply->readAll(); QBuffer buffer(&bytes); buffer.open(QIODevice::ReadOnly);
            QImageReader reader(&buffer); const QSize size=reader.size();
            if (size.isValid() && size.width()<=1024 && size.height()<=1024) {
                const QImage image=reader.read();
                if (!image.isNull() && QDir().mkpath(directory)) {
                    QSaveFile file(path);
                    if (file.open(QIODevice::WriteOnly) && image.scaled(64,64,Qt::KeepAspectRatio,Qt::SmoothTransformation).save(&file,"PNG") && file.commit()) {
                        m_failed.remove(origin); emit faviconReady(origin);
                    }
                }
            }
        }
        reply->deleteLater();
    });
    return {};
}
