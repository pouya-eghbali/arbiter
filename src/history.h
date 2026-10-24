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
public:
    enum Role { VisitId = Qt::UserRole, PageTitle, PageUrl, VisitedAt, Day };
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
    bool enabled() const { return m_enabled; }
    void setEnabled(bool value);
    QString error() const { return m_error; }
    Q_INVOKABLE QString record(const QUrl &url, const QString &title, bool offTheRecord = false);
    Q_INVOKABLE void updateTitle(const QString &id, const QUrl &url, const QString &title);
    Q_INVOKABLE bool remove(const QString &id);
    Q_INVOKABLE bool clear();
    int importVisits(const QList<ImportedVisit> &visits);
signals:
    void searchChanged();
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
    bool m_enabled = true;
};
