#pragma once
#include <QAbstractListModel>
#include <QDateTime>
#include <QSqlDatabase>
#include <QUrl>
#include "importdata.h"

class History final : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(QString search READ search WRITE setSearch NOTIFY searchChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(int totalCount READ totalCount NOTIFY countChanged)
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(QString website READ website WRITE setWebsite NOTIFY filtersChanged)
    Q_PROPERTY(QDate fromDate READ fromDate NOTIFY filtersChanged)
    Q_PROPERTY(QDate toDate READ toDate NOTIFY filtersChanged)
    Q_PROPERTY(bool hasFilters READ hasFilters NOTIFY filtersChanged)
    Q_PROPERTY(QStringList matchingIds READ matchingIds NOTIFY countChanged)
public:
    enum Role { VisitId = Qt::UserRole, PageTitle, PageUrl, VisitedAt, Day, Website };
    explicit History(QObject *parent = nullptr);
    explicit History(const QString &directory, QObject *parent = nullptr);
    ~History() override;
    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    int count() const { return m_visible.size(); }
    int totalCount() const { return m_entries.size(); }
    QString search() const { return m_search; }
    void setSearch(const QString &value);
    QString website() const { return m_website; }
    void setWebsite(const QString &value);
    QDate fromDate() const { return m_fromDate; }
    QDate toDate() const { return m_toDate; }
    bool hasFilters() const { return !m_search.trimmed().isEmpty() || !m_website.isEmpty() || m_fromDate.isValid() || m_toDate.isValid(); }
    QStringList matchingIds() const;
    Q_INVOKABLE void resetFilters();
    Q_INVOKABLE bool setDateRange(const QString &from, const QString &through);
    bool enabled() const { return m_enabled; }
    void setEnabled(bool value);
    QString error() const { return m_error; }
    Q_INVOKABLE QVariantList suggestions(const QString &query, int maximum=8) const;
    Q_INVOKABLE QString record(const QUrl &url, const QString &title, bool offTheRecord = false);
    Q_INVOKABLE void updateTitle(const QString &id, const QUrl &url, const QString &title);
    Q_INVOKABLE bool remove(const QString &id);
    Q_INVOKABLE bool clear();
    Q_INVOKABLE bool removeVisits(const QStringList &ids);
    int importVisits(const QList<ImportedVisit> &visits);
signals:
    void searchChanged();
    void filtersChanged();
    void countChanged();
    void enabledChanged();
    void errorChanged();
private:
    struct Entry { QString id; QString title; QUrl url; QDateTime time; };
    void filter(bool resetting = false);
    bool fail(const QString &message);
    QSqlDatabase m_db;
    QList<Entry> m_entries;
    QList<int> m_visible;
    QString m_search, m_error;
    QString m_website;
    QDate m_fromDate, m_toDate;
    bool m_enabled = true;
};
