#pragma once

#include <QAbstractListModel>
#include <QUrl>
#include <QVariantList>
#include <QHash>

// Page objects live in QML. Stable IDs keep metadata updates independent of
// row changes, so closing a tab cannot update a different page by accident.
class Tabs : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int currentIndex READ currentIndex WRITE setCurrentIndex NOTIFY currentIndexChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(int workspaceCount READ workspaceCount NOTIFY workspaceChanged)
    Q_PROPERTY(QString workspaceId READ workspaceId NOTIFY workspaceChanged)
public:
    enum Role { Id = Qt::UserRole, InitialUrl, Title, Url, WorkspaceId, ProfileId, Pinned };
    explicit Tabs(QObject *parent = nullptr);
    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    int currentIndex() const { return m_currentIndex; }
    int count() const { return m_tabs.size(); }
    void setCurrentIndex(int index);
    int workspaceCount() const;
    QString workspaceId() const { return m_workspace; }
    void setWorkspace(const QString &id, const QString &profile);
    Q_INVOKABLE int idAt(int row) const;
    Q_INVOKABLE QVariantList choices() const { return snapshot(); }
    Q_INVOKABLE QString workspaceAt(int row) const;
    Q_INVOKABLE QString profileAt(int row) const;
    Q_INVOKABLE bool pinnedAt(int row) const;
    Q_INVOKABLE void togglePinned(int row);
    bool moveTo(int row, const QString &workspace, const QString &profile);
    void removeWorkspace(const QString &id);
    QVariantList snapshot() const;
    QVariantMap selections() const;
    void restore(const QVariantList &entries, const QHash<QString,QString> &profiles, const QVariantMap &selections);

    Q_INVOKABLE int add(const QUrl &url = QUrl(QStringLiteral("about:blank")), bool activate = true);
    int addToWorkspace(const QUrl &url, const QString &workspace, const QString &profile);
    Q_INVOKABLE void close(int index);
    Q_INVOKABLE void cycle(int direction);
    Q_INVOKABLE bool reopen();
    Q_INVOKABLE void update(int id, const QString &title, const QUrl &url);
signals:
    void currentIndexChanged();
    void countChanged();
    void workspaceChanged();
    void sessionChanged();
private:
    struct Tab { int id; QUrl initialUrl; QString title; QUrl url; QString workspace; QString profile; bool pinned = false; };
    QList<Tab> m_tabs;
    QList<Tab> m_closed;
    QString m_workspace = "main", m_profile = "Default";
    QHash<QString,int> m_selected;
    int firstInWorkspace() const;
    int m_currentIndex = -1;
    int m_nextId = 1;
};
