#include "tabs.h"
#include <QSet>

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
    case WorkspaceId: return tab.workspace;
    case ProfileId: return tab.profile;
    case Pinned: return tab.pinned;
    default: return {};
    }
}
QHash<int, QByteArray> Tabs::roleNames() const
{
    return {{Id, "tabId"}, {InitialUrl, "initialUrl"}, {Title, "tabTitle"}, {Url, "tabUrl"}, {WorkspaceId, "workspaceId"}, {ProfileId, "profileId"}, {Pinned, "tabPinned"}};
}
void Tabs::setCurrentIndex(int index)
{
    // The workspace owns the layout; a focused pane may borrow a tab from
    // another workspace without changing that tab's profile or ownership.
    if (index < 0 || index >= count() || index == m_currentIndex) return;
    m_currentIndex = index;
    emit currentIndexChanged();
    emit sessionChanged();
}
int Tabs::add(const QUrl &url, bool activate)
{
    const int row=addToWorkspace(url,m_workspace,m_profile);
    if (activate || m_currentIndex < 0) setCurrentIndex(row);
    return row;
}
int Tabs::addToWorkspace(const QUrl &url, const QString &workspace, const QString &profile)
{
    const int row = count();
    const QUrl initial = url.isEmpty() ? QUrl(QStringLiteral("about:blank")) : url;
    beginInsertRows({}, row, row);
    m_tabs.append({m_nextId++, initial, {}, initial, workspace, profile});
    endInsertRows();
    emit countChanged();
    emit workspaceChanged();
    emit sessionChanged();
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
    else if (row == m_currentIndex) m_currentIndex = firstInWorkspace();
    endRemoveRows();
    emit countChanged();
    emit workspaceChanged();
    emit currentIndexChanged();
    emit sessionChanged();
}
void Tabs::cycle(int direction)
{
    const auto workspace=m_currentIndex>=0 ? workspaceAt(m_currentIndex) : m_workspace;
    QList<int> rows;
    for (int i=0;i<count();++i) if (m_tabs[i].workspace==workspace) rows.append(i);
    if (rows.isEmpty()) return;
    const int current = rows.indexOf(m_currentIndex);
    setCurrentIndex(rows[(qMax(0,current) + direction % rows.size() + rows.size()) % rows.size()]);
}
bool Tabs::reopen()
{
    const auto workspace=m_currentIndex>=0 ? workspaceAt(m_currentIndex) : m_workspace;
    int found = -1;
    for (int i=m_closed.size()-1;i>=0;--i) if (m_closed[i].workspace==workspace) { found=i; break; }
    if (found < 0) return false;
    const auto closed = m_closed.takeAt(found);
    const int row = addToWorkspace(closed.url,closed.workspace,closed.profile);
    setCurrentIndex(row);
    m_tabs[row].pinned = closed.pinned;
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
        emit sessionChanged();
        return;
    }
}

int Tabs::workspaceCount() const {
    int n=0; for (const auto &tab:m_tabs) if(tab.workspace==m_workspace) ++n; return n;
}
int Tabs::firstInWorkspace() const {
    for (int i=0;i<count();++i) if(m_tabs[i].workspace==m_workspace) return i;
    return -1;
}
QString Tabs::workspaceAt(int row) const { return row>=0 && row<count() ? m_tabs[row].workspace : QString(); }
QString Tabs::profileAt(int row) const { return row>=0 && row<count() ? m_tabs[row].profile : QString(); }
bool Tabs::pinnedAt(int row) const { return row>=0 && row<count() && m_tabs[row].pinned; }
void Tabs::setWorkspace(const QString &id, const QString &profile) {
    if (id==m_workspace && profile==m_profile && m_currentIndex>=0) return;
    if (m_currentIndex>=0 && m_currentIndex<count()) m_selected[m_workspace]=m_tabs[m_currentIndex].id;
    m_workspace=id; m_profile=profile;
    m_currentIndex=firstInWorkspace();
    for (int i=0;i<count();++i) if(m_tabs[i].workspace==id && m_tabs[i].id==m_selected.value(id)) m_currentIndex=i;
    if (m_currentIndex<0) add();
    emit workspaceChanged(); emit currentIndexChanged(); emit sessionChanged();
}
void Tabs::togglePinned(int row) {
    if(row<0 || row>=count()) return;
    m_tabs[row].pinned=!m_tabs[row].pinned;
    emit dataChanged(index(row),index(row),{Pinned}); emit sessionChanged();
}
bool Tabs::moveTo(int row, const QString &workspace, const QString &profile) {
    if(row<0 || row>=count() || m_tabs[row].profile!=profile) return false;
    m_tabs[row].workspace=workspace;
    if(row==m_currentIndex) m_currentIndex=firstInWorkspace();
    emit dataChanged(index(row),index(row),{WorkspaceId});
    if (firstInWorkspace()<0) add();
    emit workspaceChanged(); emit currentIndexChanged(); emit sessionChanged(); return true;
}
void Tabs::removeWorkspace(const QString &id) {
    for(int i=count()-1;i>=0;--i) if(m_tabs[i].workspace==id) close(i);
    m_closed.removeIf([&](const Tab &tab){return tab.workspace==id;});
    m_selected.remove(id);
}
QVariantList Tabs::snapshot() const {
    QVariantList result;
    for (const auto &t:m_tabs) {
        QUrl url=t.url; url.setUserInfo(QString());
        result.append(QVariantMap{{"id",t.id},{"workspace",t.workspace},{"url",url.toString()},{"title",t.title.left(1024)},{"pinned",t.pinned}});
    }
    return result;
}
QVariantMap Tabs::selections() const {
    QVariantMap result; for(auto i=m_selected.cbegin();i!=m_selected.cend();++i) result[i.key()]=i.value();
    if(m_currentIndex>=0 && m_currentIndex<count()) result[m_workspace]=m_tabs[m_currentIndex].id;
    return result;
}
void Tabs::restore(const QVariantList &entries, const QHash<QString,QString> &profiles, const QVariantMap &selected) {
    beginResetModel(); m_tabs.clear(); m_closed.clear(); m_selected.clear(); m_currentIndex=-1; m_nextId=1;
    QSet<int> ids;
    for (const auto &value:entries.mid(0,2000)) {
        const auto t=value.toMap(); const auto workspace=t.value("workspace").toString();
        QUrl url(t.value("url").toString()); const int id=t.value("id").toInt();
        if(!profiles.contains(workspace) || id<=0 || id>10000000 || ids.contains(id) || !url.isValid() || url.toString().size()>8192) continue;
        if(url.scheme()!="http" && url.scheme()!="https" && url.scheme()!="file" && url!=QUrl("about:blank")) continue;
        url.setUserInfo(QString()); ids.insert(id); m_nextId=qMax(m_nextId,id+1);
        m_tabs.append({id,url,t.value("title").toString().left(1024),url,workspace,profiles.value(workspace),t.value("pinned").toBool()});
    }
    for(auto i=selected.cbegin();i!=selected.cend();++i) m_selected[i.key()]=i.value().toInt();
    endResetModel(); emit countChanged(); emit workspaceChanged(); emit currentIndexChanged();
}

int Tabs::idAt(int row) const { return row>=0 && row<count() ? m_tabs[row].id : 0; }
