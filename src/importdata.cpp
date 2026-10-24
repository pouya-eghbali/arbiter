#include "importdata.h"
#include "passwordmanager.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QStringDecoder>
#include <QUuid>
#include <QTimeZone>
#include <algorithm>
#include <cmath>

namespace {
constexpr int maxPasswords=10000,maxVisits=10000;
QByteArray readFile(const QString &path,QString &error) {
    QFile file(path);
    if(!file.open(QIODevice::ReadOnly) || file.size()>32*1024*1024) { error=QStringLiteral("Could not read the import file (maximum 32 MB)."); return {}; }
    const auto bytes=file.readAll();
    if(file.error()!=QFileDevice::NoError) error=QStringLiteral("The import file could not be read completely.");
    return bytes;
}
// RFC 4180, including doubled quotes, embedded newlines, CRLF and UTF-8 BOM.
bool csv(const QString &text,QList<QStringList> &rows) {
    QString field; QStringList row; bool quoted=false,closed=false;
    for(qsizetype i=0;i<text.size();++i) {
        const auto ch=text[i];
        if(quoted) {
            if(ch=='"') { if(i+1<text.size() && text[i+1]=='"') { field+='"'; ++i; } else { quoted=false; closed=true; } }
            else field+=ch;
        } else if(ch==',' || ch=='\n' || ch=='\r') {
            row.append(field); field.clear(); closed=false;
            if(ch!=',') {
                if(ch=='\r' && i+1<text.size() && text[i+1]=='\n') ++i;
                if(row.size()!=1 || !row.first().isEmpty()) rows.append(row);
                row.clear();
                if(rows.size()>maxPasswords+1) return false;
            }
        } else if(closed) return false;
        else if(ch=='"') { if(!field.isEmpty()) return false; quoted=true; }
        else field+=ch;
        if(field.size()>1024*1024 || row.size()>128) return false;
    }
    if(quoted) return false;
    if(!field.isEmpty() || closed || !row.isEmpty()) { row.append(field); rows.append(row); }
    return rows.size()<=maxPasswords+1;
}
void passwords(const QString &path,ImportPreview &out) {
    auto bytes=readFile(path,out.error); if(!out.error.isEmpty()) return;
    if(bytes.startsWith("\xef\xbb\xbf")) bytes.remove(0,3);
    QStringDecoder decoder(QStringDecoder::Utf8); const QString text=decoder.decode(bytes);
    QList<QStringList> rows;
    if(decoder.hasError() || !csv(text,rows) || rows.isEmpty()) { out.error=QStringLiteral("Invalid UTF-8 password CSV, or more than 10,000 logins. Choose a browser password export."); return; }
    auto header=rows.takeFirst(); for(auto &name:header) name=name.trimmed().toLower();
    const int url=header.indexOf("url"),user=header.indexOf("username"),secret=header.indexOf("password");
    if(url<0 || user<0 || secret<0 || header.count("url")!=1 || header.count("username")!=1 || header.count("password")!=1) {
        out.error=QStringLiteral("Password CSV must contain URL, Username and Password columns (Chrome, Edge, Brave, Firefox or Safari exports)."); return;
    }
    QSet<QString> seen;
    for(const auto &row:rows) {
        if(row.size()!=header.size()) { ++out.skippedPasswords; continue; }
        const QUrl site(row[url].trimmed()); const auto origin=PasswordManager::originFor(site);
        const auto account=origin+QChar(0)+row[user];
        if(!PasswordManager::canStore(site,row[user],row[secret]) || seen.contains(account)) { ++out.skippedPasswords; continue; }
        seen.insert(account); out.passwords.append({QUrl(origin),row[user],row[secret]});
    }
}
void appendVisit(ImportPreview &out,QSet<QString> &seen,const QString &address,const QString &title,qint64 ms) {
    QUrl url(address);
    if(!url.isValid() || url.host().isEmpty() || (url.scheme()!="http" && url.scheme()!="https")
       || address.size()>8192 || ms<0 || ms>QDateTime::currentMSecsSinceEpoch()) { ++out.skippedVisits; return; }
    url.setUserInfo({});
    const auto key=url.toString(QUrl::FullyEncoded)+QChar(0)+QString::number(ms);
    if(seen.contains(key)) { ++out.skippedVisits; return; }
    seen.insert(key); out.visits.append({url,title.left(1024),QDateTime::fromMSecsSinceEpoch(ms,QTimeZone::UTC)});
}
void historyJson(const QByteArray &bytes,ImportPreview &out) {
    QJsonParseError error;
    const auto document=QJsonDocument::fromJson(bytes,&error);
    const auto root=document.object();
    const auto key=root.contains("history") ? QStringLiteral("history") : QStringLiteral("Browser History");
    if(error.error!=QJsonParseError::NoError || !root.value(key).isArray()) { out.error=QStringLiteral("Choose Safari History.json or Chrome Takeout BrowserHistory.json, or a supported browser history database."); return; }
    QSet<QString> seen;
    for(const auto &value:root.value(key).toArray()) {
        const auto row=value.toObject(); bool ok=false;
        const auto stamp=row.value("time_usec");
        qint64 micro=stamp.isString() ? stamp.toString().toLongLong(&ok) : stamp.toInteger(-1);
        if(!stamp.isString()) ok=stamp.isDouble() && micro>=0;
        if(!ok) { ++out.skippedVisits; continue; }
        appendVisit(out,seen,row.value("url").toString(),row.value("title").toString(),micro/1000);
    }
}
void historyDatabase(const QString &path,ImportPreview &out) {
    const auto name=QUuid::createUuid().toString();
    {
        auto db=QSqlDatabase::addDatabase("QSQLITE",name);
        db.setDatabaseName(path); db.setConnectOptions("QSQLITE_OPEN_READONLY;QSQLITE_BUSY_TIMEOUT=1500");
        if(!db.open()) out.error=QStringLiteral("Could not open browser history. Close the source browser, then try again, or choose an exported file.");
        else {
            const auto tables=db.tables(); QString sql; enum Clock { UnixMicros,ChromeMicros,SafariSeconds } clock=UnixMicros;
            if(tables.contains("urls") && tables.contains("visits")) { sql="SELECT u.url,u.title,v.visit_time FROM visits v JOIN urls u ON u.id=v.url ORDER BY v.visit_time DESC LIMIT 10000"; clock=ChromeMicros; }
            else if(tables.contains("moz_places") && tables.contains("moz_historyvisits")) sql="SELECT p.url,p.title,v.visit_date FROM moz_historyvisits v JOIN moz_places p ON p.id=v.place_id ORDER BY v.visit_date DESC LIMIT 10000";
            else if(tables.contains("history_items") && tables.contains("history_visits")) { sql="SELECT i.url,v.title,v.visit_time FROM history_visits v JOIN history_items i ON i.id=v.history_item ORDER BY v.visit_time DESC LIMIT 10000"; clock=SafariSeconds; }
            if(sql.isEmpty()) out.error=QStringLiteral("This is not a supported browser history database.");
            else {
                QSqlQuery query(db); QSet<QString> seen;
                if(!query.exec(sql)) out.error=QStringLiteral("Could not read browser history. Close the source browser and try again. This database may use an unsupported format.");
                else while(query.next()) {
                    bool ok=false; qint64 ms=0;
                    if(clock==SafariSeconds) {
                        const double seconds=query.value(2).toDouble(&ok);
                        if(!std::isfinite(seconds) || seconds<0 || seconds>1e11) ok=false;
                        if(ok) ms=qint64(seconds*1000)+978307200000LL;
                    } else {
                        const auto micro=query.value(2).toLongLong(&ok);
                        // Chromium: microseconds since 1601; Firefox: since 1970.
                        ms=micro/1000-(clock==ChromeMicros ? 11644473600000LL : 0);
                    }
                    if(!ok) { ++out.skippedVisits; continue; }
                    appendVisit(out,seen,query.value(0).toString(),query.value(1).toString(),ms);
                }
                if(query.lastError().isValid() && out.error.isEmpty()) out.error=QStringLiteral("Browser history could not be read completely. No data was imported.");
            }
            db.close();
        }
    }
    QSqlDatabase::removeDatabase(name);
}
}
ImportPreview BrowserImport::read(const QString &passwordFile,const QString &historyFile) {
    ImportPreview out;
    if(!passwordFile.isEmpty()) passwords(passwordFile,out);
    if(out.error.isEmpty() && !historyFile.isEmpty()) {
        QFile file(historyFile);
        if(!file.open(QIODevice::ReadOnly)) out.error=QStringLiteral("Could not read the selected history file.");
        else if(file.peek(16)==QByteArray("SQLite format 3\0",16)) { file.close(); historyDatabase(historyFile,out); }
        else { file.close(); const auto bytes=readFile(historyFile,out.error); if(out.error.isEmpty()) historyJson(bytes,out); }
    }
    std::sort(out.visits.begin(),out.visits.end(),[](const auto &a,const auto &b) { return a.time>b.time; });
    if(out.visits.size()>maxVisits) { out.skippedVisits+=out.visits.size()-maxVisits; out.visits.resize(maxVisits); }
    if(!out.error.isEmpty()) { out.passwords.clear(); out.visits.clear(); }
    return out;
}
