#include "tabs.h"

Tabs::Tabs(QObject *parent) : QAbstractListModel(parent) { add(); }
int Tabs::rowCount(const QModelIndex &parent) const { return parent.isValid() ? 0 : count(); }
QVariant Tabs::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= count()) return {};
    const auto &tab = m_tabs[index.row()];
    switch (role) {
    case Id: return tab.id;
    case InitialUrl: return tab.initialUrl;
    case Title: return tab.title;
    case Url: return tab.url;
    default: return {};
    }
}
QHash<int, QByteArray> Tabs::roleNames() const
{
    return {{Id, "tabId"}, {InitialUrl, "initialUrl"}, {Title, "tabTitle"}, {Url, "tabUrl"}};
}
void Tabs::setCurrentIndex(int index)
{
    if (index < 0 || index >= count() || index == m_currentIndex) return;
    m_currentIndex = index;
    emit currentIndexChanged();
}
int Tabs::add(const QUrl &url, bool activate)
{
    const int row = count();
    const QUrl initial = url.isEmpty() ? QUrl(QStringLiteral("about:blank")) : url;
    beginInsertRows({}, row, row);
    m_tabs.append({m_nextId++, initial, {}, initial});
    endInsertRows();
    emit countChanged();
    if (activate || m_currentIndex < 0) setCurrentIndex(row);
    return row;
}
void Tabs::close(int row)
{
    if (row < 0 || row >= count()) return;
    m_closed.append(m_tabs[row]);
    if (m_closed.size() > 20) m_closed.removeFirst();
    beginRemoveRows({}, row, row);
    m_tabs.removeAt(row);
    // Removing an earlier row preserves the currently selected page.
    if (row < m_currentIndex) --m_currentIndex;
    else if (row == m_currentIndex) m_currentIndex = qMin(row, count() - 1);
    endRemoveRows();
    emit countChanged();
    emit currentIndexChanged();
}
void Tabs::cycle(int direction)
{
    if (count()) setCurrentIndex((m_currentIndex + direction % count() + count()) % count());
}
bool Tabs::reopen()
{
    if (m_closed.isEmpty()) return false;
    const auto closed = m_closed.takeLast();
    const int row = add(closed.url);
    update(m_tabs[row].id, closed.title, closed.url);
    return true;
}
void Tabs::update(int id, const QString &title, const QUrl &url)
{
    for (int row = 0; row < count(); ++row) {
        auto &tab = m_tabs[row];
        if (tab.id != id) continue;
        if (tab.title == title && tab.url == url) return;
        tab.title = title;
        tab.url = url;
        emit dataChanged(index(row), index(row), {Title, Url});
        return;
    }
}
