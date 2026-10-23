#include "history.h"
#include <QDir>
#include <QFile>
#include <QLocale>
#include <QSettings>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QUuid>

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
    m_enabled = QSettings().value("History/enabled", true).toBool();
    if (!QDir().mkpath(directory)) { fail(tr("Could not create history storage.")); return; }
    QFile::setPermissions(directory, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
    m_db = QSqlDatabase::addDatabase("QSQLITE", QUuid::createUuid().toString(QUuid::WithoutBraces));
    m_db.setDatabaseName(directory + "/history.sqlite");
    m_db.setConnectOptions("QSQLITE_BUSY_TIMEOUT=3000");
    if (!m_db.open()) { fail(tr("Could not open history storage. Existing data has been preserved.")); return; }
    QFile::setPermissions(m_db.databaseName(), QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    QSqlQuery query(m_db);
    if (!query.exec("PRAGMA secure_delete=ON") || !query.exec("CREATE TABLE IF NOT EXISTS visits (id INTEGER PRIMARY KEY AUTOINCREMENT, url TEXT NOT NULL, title TEXT NOT NULL, visited INTEGER NOT NULL)")) {
        fail(tr("Could not initialize history storage.")); return;
    }
    if (!query.exec("SELECT id, url, title, visited FROM visits ORDER BY id DESC LIMIT 10000")) { fail(tr("Could not read browsing history.")); return; }
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
    case Day: {
        const auto date = entry.time.date();
        if (date == QDate::currentDate()) return tr("Today");
        if (date == QDate::currentDate().addDays(-1)) return tr("Yesterday");
        return QLocale().toString(date, QLocale::LongFormat);
    }
    default: return {};
    }
}
QHash<int, QByteArray> History::roleNames() const {
    return {{VisitId,"visitId"}, {PageTitle,"pageTitle"}, {PageUrl,"pageUrl"}, {VisitedAt,"visitedAt"}, {Day,"day"}};
}
bool History::fail(const QString &message) { m_error = message; emit errorChanged(); return false; }
void History::filter(bool resetting) {
    if (!resetting) beginResetModel();
    m_visible.clear();
    const auto text = m_search.trimmed();
    for (int i = 0; i < m_entries.size(); ++i)
        if (text.isEmpty() || m_entries[i].title.contains(text, Qt::CaseInsensitive)
                || m_entries[i].url.toDisplayString().contains(text, Qt::CaseInsensitive)) m_visible.append(i);
    endResetModel(); emit countChanged();
}
void History::setSearch(const QString &value) {
    if (value == m_search) return;
    m_search = value; filter(); emit searchChanged();
}
void History::setEnabled(bool value) {
    if (value == m_enabled) return;
    m_enabled = value; QSettings().setValue("History/enabled", value); emit enabledChanged();
}
QString History::record(const QUrl &input, const QString &title, bool offTheRecord) {
    const auto url = historyUrl(input);
    if (!m_enabled || offTheRecord || url.isEmpty() || !m_error.isEmpty() || !m_db.isOpen()) return {};
    const auto time = QDateTime::currentDateTime();
    QSqlQuery query(m_db);
    if (!m_db.transaction()) { fail(tr("Could not save browsing history.")); return {}; }
    query.prepare("INSERT INTO visits (url,title,visited) VALUES (?,?,?)");
    query.addBindValue(url.toString(QUrl::FullyEncoded)); query.addBindValue(title.left(1024)); query.addBindValue(time.toMSecsSinceEpoch());
    if (!query.exec()) { m_db.rollback(); fail(tr("Could not save browsing history.")); return {}; }
    const auto id = query.lastInsertId().toString();
    if (!query.exec("DELETE FROM visits WHERE id NOT IN (SELECT id FROM visits ORDER BY id DESC LIMIT 10000)") || !m_db.commit()) {
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
