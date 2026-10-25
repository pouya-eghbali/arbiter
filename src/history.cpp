#include "history.h"
#include <QDir>
#include <QFile>
#include <QLocale>
#include "settings.h"
#include <QSqlQuery>
#include <QStandardPaths>
#include <QUuid>
#include <QHash>
#include <QRegularExpression>
#include <QSet>
#include <algorithm>

namespace {
constexpr int limit = 10000;
QUrl historyUrl(QUrl url) {
    if (!url.isValid() || url.host().isEmpty() || (url.scheme() != "https" && url.scheme() != "http")) return {};
    url.setUserInfo(QString());
    return url.toString(QUrl::FullyEncoded).size() <= 8192 ? url : QUrl();
}
}
History::History(QObject *parent) : History(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
    + QStringLiteral("/history/Default"), parent) {}
History::History(const QString &directory, QObject *parent) : QAbstractListModel(parent) {
    if (directory.isEmpty()) { m_enabled=false; return; } // Private windows have no history database.
    m_enabled = Settings().value("History/enabled", true).toBool();
    if (!QDir().mkpath(directory)) { fail(tr("Could not create history storage.")); return; }
    QFile::setPermissions(directory, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
    m_db = QSqlDatabase::addDatabase("QSQLITE", QUuid::createUuid().toString(QUuid::WithoutBraces));
    m_db.setDatabaseName(directory + "/history.sqlite");
    m_db.setConnectOptions("QSQLITE_BUSY_TIMEOUT=3000");
    if (!m_db.open()) { fail(tr("Could not open history storage. Existing data has been preserved.")); return; }
    QFile::setPermissions(m_db.databaseName(), QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    QSqlQuery query(m_db);
    if (!query.exec("PRAGMA secure_delete=ON") || !query.exec("CREATE TABLE IF NOT EXISTS visits (id INTEGER PRIMARY KEY AUTOINCREMENT, url TEXT NOT NULL, title TEXT NOT NULL, visited INTEGER NOT NULL)")
        || !query.exec("CREATE INDEX IF NOT EXISTS visits_url_visited ON visits(url,visited)")) {
        fail(tr("Could not initialize history storage.")); return;
    }
    if (!query.exec("SELECT id, url, title, visited FROM visits ORDER BY visited DESC,id DESC LIMIT 10000")) { fail(tr("Could not read browsing history.")); return; }
    while (query.next()) {
        const auto url = historyUrl(QUrl(query.value(1).toString()));
        if (!url.isEmpty()) m_entries.append({query.value(0).toString(), query.value(2).toString(), url, QDateTime::fromMSecsSinceEpoch(query.value(3).toLongLong())});
    }
    filter();
}
History::~History() {
    const auto connection = m_db.connectionName();
    m_db.close(); m_db = QSqlDatabase();
    if (!connection.isEmpty()) QSqlDatabase::removeDatabase(connection);
}
int History::rowCount(const QModelIndex &parent) const { return parent.isValid() ? 0 : count(); }
QVariant History::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= count()) return {};
    const auto &entry = m_entries[m_visible[index.row()]];
    switch (role) {
    case VisitId: return entry.id;
    case PageTitle: return entry.title.isEmpty() ? entry.url.host() : entry.title;
    case PageUrl: return entry.url.toString(QUrl::FullyEncoded);
    case VisitedAt: return entry.time;
    case Website: return entry.url.host();
    case Day: {
        const auto date = entry.time.toLocalTime().date();
        if (date == QDate::currentDate()) return tr("Today");
        if (date == QDate::currentDate().addDays(-1)) return tr("Yesterday");
        return QLocale().toString(date, QLocale::LongFormat);
    }
    default: return {};
    }
}
QHash<int, QByteArray> History::roleNames() const {
    return {{VisitId,"visitId"}, {PageTitle,"pageTitle"}, {PageUrl,"pageUrl"}, {VisitedAt,"visitedAt"}, {Day,"day"}, {Website,"website"}};
}
bool History::fail(const QString &message) { m_error = message; emit errorChanged(); return false; }
void History::filter(bool resetting) {
    if (!resetting) beginResetModel();
    m_visible.clear();
    const auto text = m_search.trimmed();
    for (int i = 0; i < m_entries.size(); ++i) {
        const auto &entry = m_entries[i];
        const auto host = entry.url.host();
        const auto date = entry.time.toLocalTime().date();
        if (!m_website.isEmpty() && host.compare(m_website, Qt::CaseInsensitive) != 0
            && !host.endsWith("." + m_website, Qt::CaseInsensitive)) continue;
        if (m_fromDate.isValid() && date < m_fromDate) continue;
        if (m_toDate.isValid() && date > m_toDate) continue;
        if (text.isEmpty() || entry.title.contains(text, Qt::CaseInsensitive)
            || entry.url.toDisplayString().contains(text, Qt::CaseInsensitive)) m_visible.append(i);
    }
    endResetModel(); emit countChanged();
}
void History::setSearch(const QString &value) {
    if (value == m_search) return;
    m_search = value; filter(); emit searchChanged(); emit filtersChanged();
}
void History::setWebsite(const QString &value) {
    auto site = value.trimmed().toLower();
    if (!site.isEmpty()) {
        const auto url = QUrl(site.contains("://") ? site : "https://" + site);
        // Invalid host input should match nothing, never silently expose all visits.
        site = url.host().isEmpty() ? site : url.host();
        while (site.endsWith('.')) site.chop(1);
        if (site.isEmpty()) site = QStringLiteral("<invalid>");
    }
    if (site == m_website) return;
    m_website = site; filter(); emit filtersChanged();
}
void History::resetFilters() {
    if (!hasFilters()) return;
    m_search.clear(); m_website.clear(); m_fromDate = {}; m_toDate = {};
    filter(); emit searchChanged(); emit filtersChanged();
}
bool History::setDateRange(const QString &from, const QString &through) {
    const auto first = QDate::fromString(from, Qt::ISODate);
    const auto last = QDate::fromString(through, Qt::ISODate);
    if ((!from.isEmpty() && (!first.isValid() || first.toString(Qt::ISODate) != from))
        || (!through.isEmpty() && (!last.isValid() || last.toString(Qt::ISODate) != through))
        || (first.isValid() && last.isValid() && first > last)) return false;
    if (first == m_fromDate && last == m_toDate) return true;
    m_fromDate = first; m_toDate = last; filter(); emit filtersChanged(); return true;
}
QStringList History::matchingIds() const {
    QStringList ids;
    for (const int i : m_visible) ids.append(m_entries[i].id);
    return ids;
}
void History::setEnabled(bool value) {
    if (!m_db.isValid()) return; // In-memory private-session placeholder is always disabled.
    if (value == m_enabled) return;
    m_enabled = value; Settings().setValue("History/enabled", value); emit enabledChanged();
}
QString History::record(const QUrl &input, const QString &title, bool offTheRecord) {
    const auto url = historyUrl(input);
    if (!m_enabled || offTheRecord || url.isEmpty() || !m_error.isEmpty() || !m_db.isOpen()) return {};
    const auto time = QDateTime::currentDateTime();
    QSqlQuery query(m_db);
    if (!m_db.transaction()) { fail(tr("Could not save browsing history.")); return {}; }
    query.prepare("INSERT INTO visits (url,title,visited) VALUES (?,?,?)");
    query.addBindValue(url.toString(QUrl::FullyEncoded)); query.addBindValue(title.isNull() ? QStringLiteral("") : title.left(1024)); query.addBindValue(time.toMSecsSinceEpoch());
    if (!query.exec()) { m_db.rollback(); fail(tr("Could not save browsing history.")); return {}; }
    const auto id = query.lastInsertId().toString();
    if (!query.exec("DELETE FROM visits WHERE id NOT IN (SELECT id FROM visits ORDER BY visited DESC,id DESC LIMIT 10000)") || !m_db.commit()) {
        m_db.rollback(); fail(tr("Could not save browsing history.")); return {};
    }
    beginResetModel();
    m_entries.prepend({id, title.left(1024), url, time});
    if (m_entries.size() > limit) m_entries.removeLast();
    filter(true); return id;
}
void History::updateTitle(const QString &id, const QUrl &url, const QString &title) {
    if (!m_enabled || !m_error.isEmpty() || title.isEmpty()) return;
    for (auto &entry : m_entries) {
        if (entry.id != id || entry.url != historyUrl(url) || entry.title == title.left(1024)) continue;
        QSqlQuery query(m_db); query.prepare("UPDATE visits SET title=? WHERE id=?");
        query.addBindValue(title.left(1024)); query.addBindValue(id);
        if (!query.exec()) { fail(tr("Could not update browsing history.")); return; }
        beginResetModel(); entry.title = title.left(1024); filter(true); return;
    }
}
bool History::remove(const QString &id) {
    if (!m_error.isEmpty() || !m_db.isOpen()) return false;
    QSqlQuery query(m_db); query.prepare("DELETE FROM visits WHERE id=?"); query.addBindValue(id);
    if (!query.exec()) return fail(tr("Could not delete this history entry."));
    beginResetModel();
    m_entries.removeIf([&](const Entry &entry) { return entry.id == id; }); filter(true); return true;
}
bool History::clear() {
    if (!m_error.isEmpty() || !m_db.isOpen()) return false;
    QSqlQuery query(m_db);
    if (!query.exec("DELETE FROM visits")) return fail(tr("Could not clear browsing history."));
    beginResetModel(); m_entries.clear(); filter(true); return true;
}
bool History::removeVisits(const QStringList &ids) {
    if (!m_error.isEmpty() || !m_db.isOpen()) return false;
    if (ids.isEmpty()) return true;
    // Delete a snapshot of the confirmed results, even if new visits arrive.
    const QSet<QString> selected(ids.cbegin(), ids.cend());
    if (!m_db.transaction()) return fail(tr("Could not delete matching history."));
    QSqlQuery query(m_db); query.prepare("DELETE FROM visits WHERE id=?");
    for (const auto &id : selected) {
        query.bindValue(0, id);
        if (!query.exec()) { m_db.rollback(); return fail(tr("Could not delete matching history.")); }
    }
    if (!m_db.commit()) { m_db.rollback(); return fail(tr("Could not delete matching history.")); }
    beginResetModel();
    m_entries.removeIf([&](const Entry &entry) { return selected.contains(entry.id); });
    filter(true); return true;
}

int History::importVisits(const QList<ImportedVisit> &visits) {
    if(!m_error.isEmpty() || !m_db.isOpen()) return -1;
    if(!m_db.transaction()) { fail(tr("Could not start history import.")); return -1; }
    QSqlQuery query(m_db); int imported=0;
    query.prepare("INSERT INTO visits(url,title,visited) SELECT ?,?,? WHERE NOT EXISTS (SELECT 1 FROM visits WHERE url=? AND visited=?)");
    for(const auto &visit:visits) {
        const auto url=historyUrl(visit.url); const auto ms=visit.time.toMSecsSinceEpoch();
        if(url.isEmpty() || !visit.time.isValid() || ms<0 || ms>QDateTime::currentMSecsSinceEpoch()) continue;
        const auto address=url.toString(QUrl::FullyEncoded);
        query.bindValue(0,address); query.bindValue(1,visit.title.isNull() ? QStringLiteral("") : visit.title.left(1024)); query.bindValue(2,ms);
        query.bindValue(3,address); query.bindValue(4,ms);
        if(!query.exec()) { m_db.rollback(); fail(tr("History import failed. Existing history was preserved.")); return -1; }
        imported+=query.numRowsAffected();
    }
    if(!query.exec("DELETE FROM visits WHERE id NOT IN (SELECT id FROM visits ORDER BY visited DESC,id DESC LIMIT 10000)") || !m_db.commit()) {
        m_db.rollback(); fail(tr("History import failed. Existing history was preserved.")); return -1;
    }
    if(!query.exec("SELECT id,url,title,visited FROM visits ORDER BY visited DESC,id DESC LIMIT 10000")) { fail(tr("Could not reload imported history.")); return -1; }
    beginResetModel(); m_entries.clear();
    while(query.next()) m_entries.append({query.value(0).toString(),query.value(2).toString(),QUrl(query.value(1).toString()),QDateTime::fromMSecsSinceEpoch(query.value(3).toLongLong())});
    filter(true); return imported;
}

QVariantList History::suggestions(const QString &query,int maximum) const {
    maximum=qBound(0,maximum,20);
    if(maximum==0) return {};
    struct Suggestion { QString title,url; int count=0,order=0,score=0; };
    QList<Suggestion> entries; QHash<QString,int> byUrl;
    const auto terms=query.trimmed().left(512).split(QRegularExpression("\\s+"),Qt::SkipEmptyParts);
    for(int i=0;i<m_entries.size();++i) {
        const auto &entry=m_entries[i];
        const auto url=entry.url.adjusted(QUrl::RemoveFragment|QUrl::RemoveUserInfo).toString(QUrl::FullyEncoded);
        if(byUrl.contains(url)) { ++entries[byUrl.value(url)].count; continue; }
        const auto searchable=entry.title+" "+entry.url.toDisplayString(QUrl::RemoveUserInfo);
        if(!std::all_of(terms.begin(),terms.end(),[&](const QString &term) { return searchable.contains(term,Qt::CaseInsensitive); })) continue;
        byUrl[url]=entries.size(); entries.append({entry.title.isEmpty() ? entry.url.host() : entry.title,url,1,i,0});
    }
    for(auto &entry:entries) entry.score=entry.count*20+1000/(entry.order+10);
    std::stable_sort(entries.begin(),entries.end(),[](const auto &a,const auto &b) { return a.score>b.score; });
    QVariantList result;
    for(const auto &entry:entries) {
        result.append(QVariantMap{{"title",entry.title},{"url",entry.url}});
        if(result.size()>=maximum) break;
    }
    return result;
}
