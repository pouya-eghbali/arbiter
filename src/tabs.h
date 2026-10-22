#pragma once

#include <QAbstractListModel>
#include <QUrl>

// Page objects live in QML. Stable IDs keep metadata updates independent of
// row changes, so closing a tab cannot update a different page by accident.
class Tabs : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int currentIndex READ currentIndex WRITE setCurrentIndex NOTIFY currentIndexChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)
public:
    enum Role { Id = Qt::UserRole, InitialUrl, Title, Url };
    explicit Tabs(QObject *parent = nullptr);
    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    int currentIndex() const { return m_currentIndex; }
    int count() const { return m_tabs.size(); }
    void setCurrentIndex(int index);
    Q_INVOKABLE int add(const QUrl &url = QUrl(QStringLiteral("about:blank")), bool activate = true);
    Q_INVOKABLE void close(int index);
    Q_INVOKABLE void cycle(int direction);
    Q_INVOKABLE bool reopen();
    Q_INVOKABLE void update(int id, const QString &title, const QUrl &url);
signals:
    void currentIndexChanged();
    void countChanged();
private:
    struct Tab { int id; QUrl initialUrl; QString title; QUrl url; };
    QList<Tab> m_tabs;
    QList<Tab> m_closed;
    int m_currentIndex = -1;
    int m_nextId = 1;
};
