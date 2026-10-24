#include "splitlayout.h"
#include "tabs.h"
#include <QSet>
#include <cmath>
#include <algorithm>

namespace { constexpr qreal gap=8, paneWidth=200, paneHeight=160; }
SplitLayout::SplitLayout(Tabs *tabs,QObject *parent) : QObject(parent),m_tabs(tabs) {
    connect(tabs,&Tabs::currentIndexChanged,this,[this] { synchronize(true); });
    connect(tabs,&Tabs::workspaceChanged,this,[this] { synchronize(false); });
    connect(tabs,&QAbstractItemModel::rowsRemoved,this,[this] { synchronize(false); });
    synchronize(true);
}
SplitLayout::Tree &SplitLayout::tree() { return m_trees[m_tabs->workspaceId()]; }
const SplitLayout::Tree *SplitLayout::current() const {
    auto it=m_trees.constFind(m_tabs->workspaceId()); return it==m_trees.cend() ? nullptr : &it.value();
}
int SplitLayout::row(int tab,const QString &workspace) const {
    for(int i=0;i<m_tabs->count();++i)
        if((workspace.isEmpty() || m_tabs->workspaceAt(i)==workspace) && m_tabs->idAt(i)==tab) return i;
    return -1;
}
int SplitLayout::firstLeaf(const Tree &t,int id) const {
    while(id && t.nodes.value(id).first) id=t.nodes.value(id).first;
    return id;
}
int SplitLayout::prune(Tree &t,int id,const QString &workspace) {
    if(!id || !t.nodes.contains(id)) return 0;
    const auto n=t.nodes.value(id);
    if(!n.first) { if(row(n.tab,n.workspace.isEmpty() ? workspace : n.workspace)>=0) return id; t.nodes.remove(id); return 0; }
    const int a=prune(t,n.first,workspace),b=prune(t,n.second,workspace);
    if(!a || !b) { t.nodes.remove(id); return a ? a : b; }
    t.nodes[id].first=a; t.nodes[id].second=b; return id;
}
void SplitLayout::synchronize(bool selection) {
    if(m_mutating) return;
    m_mutating=true;
    for(auto it=m_trees.begin();it!=m_trees.end();++it) {
        auto &t=it.value(); t.root=prune(t,t.root,it.key());
        if(!t.nodes.contains(t.active) || t.nodes.value(t.active).first) t.active=firstLeaf(t,t.root);
    }
    auto &t=tree();
    const int selected=m_tabs->currentIndex();
    const int tab=selected<0 ? 0 : m_tabs->data(m_tabs->index(selected),Tabs::Id).toInt();
    if(!t.root && tab && m_tabs->workspaceAt(selected)==m_tabs->workspaceId()) {
        const int id=m_nextId++; t.nodes.insert(id,{id,tab,0,0,true,.5,m_tabs->workspaceAt(selected)}); t.root=t.active=id;
    } else if(t.root && selection && tab) {
        int visible=0;
        for(const auto &n:t.nodes) if(!n.first && n.tab==tab) { visible=n.id; break; }
        if(visible) t.active=visible;
        else { t.nodes[t.active].tab=tab; t.nodes[t.active].workspace=m_tabs->workspaceAt(selected); }
    } else if(t.root) {
        const int activeRow=row(t.nodes.value(t.active).tab);
        if(activeRow>=0) m_tabs->setCurrentIndex(activeRow);
    }
    m_mutating=false; publish();
}
void SplitLayout::publish(bool session) {
    m_geometryDirty=true; m_minimum.clear();
    emit changed(); if(session) emit sessionChanged();
}
bool SplitLayout::multiple() const { const auto *t=current(); return t && t->nodes.value(t->root).first; }
int SplitLayout::activePane() const { const auto *t=current(); return t ? t->active : 0; }
QSizeF SplitLayout::minimum(const Tree &t,int id) const {
    if(m_minimum.contains(id)) return m_minimum.value(id);
    const auto n=t.nodes.value(id);
    QSizeF size(paneWidth,paneHeight);
    if(n.first) {
        const auto a=minimum(t,n.first),b=minimum(t,n.second);
        size=n.horizontal ? QSizeF(a.width()+gap+b.width(),qMax(a.height(),b.height()))
            : QSizeF(qMax(a.width(),b.width()),a.height()+gap+b.height());
    }
    m_minimum.insert(id,size); return size;
}
qreal SplitLayout::minimumWidth() const { auto *t=current(); return t && multiple() ? minimum(*t,t->root).width() : 0; }
qreal SplitLayout::minimumHeight() const { auto *t=current(); return t && multiple() ? minimum(*t,t->root).height() : 0; }
void SplitLayout::setViewport(qreal w,qreal h) {
    if(!std::isfinite(w) || !std::isfinite(h) || w<=0 || h<=0 || (w==m_width && h==m_height)) return;
    m_width=w; m_height=h; publish(false);
}
void SplitLayout::flatten(const Tree &t,int id,const QRectF &r,QVariantList &panes,QVariantList &handles) const {
    const auto n=t.nodes.value(id);
    if(!n.first) {
        panes.append(QVariantMap{{"paneId",id},{"tabId",n.tab},{"x",r.x()},{"y",r.y()},{"width",r.width()},{"height",r.height()}}); return;
    }
    const auto a=minimum(t,n.first),b=minimum(t,n.second);
    const qreal length=(n.horizontal ? r.width() : r.height())-gap;
    const qreal minA=n.horizontal ? a.width() : a.height(),minB=n.horizontal ? b.width() : b.height();
    const qreal size=std::clamp(length*n.ratio,minA,qMax(minA,length-minB));
    QRectF first=r,second=r,handle=r;
    if(n.horizontal) { first.setWidth(size); second.setLeft(r.x()+size+gap); handle.setLeft(r.x()+size); handle.setWidth(gap); }
    else { first.setHeight(size); second.setTop(r.y()+size+gap); handle.setTop(r.y()+size); handle.setHeight(gap); }
    handles.append(QVariantMap{{"handleId",id},{"horizontal",n.horizontal},{"x",handle.x()},{"y",handle.y()},
        {"width",handle.width()},{"height",handle.height()},{"origin",n.horizontal ? r.x() : r.y()},{"length",length}});
    flatten(t,n.first,first,panes,handles); flatten(t,n.second,second,panes,handles);
}
void SplitLayout::ensureGeometry() const {
    if(!m_geometryDirty) return;
    m_panes.clear(); m_handles.clear(); m_geometry.clear();
    const auto *t=current();
    if(t && t->root) flatten(*t,t->root,{0,0,qMax(m_width,minimumWidth()),qMax(m_height,minimumHeight())},m_panes,m_handles);
    for(const auto &value:m_panes) { const auto p=value.toMap(); m_geometry.insert(p.value("tabId").toInt(),p); }
    m_geometryDirty=false;
}
QVariantList SplitLayout::panes() const { ensureGeometry(); return m_panes; }
QVariantList SplitLayout::handles() const { ensureGeometry(); return m_handles; }
QVariantMap SplitLayout::geometry(int tab) const { ensureGeometry(); return m_geometry.value(tab); }
void SplitLayout::split(bool horizontal) {
    auto &t=tree(); if(!t.root || m_mutating) return;
    const int source=row(t.nodes.value(t.active).tab); if(source<0) return;
    m_mutating=true;
    const int added=m_tabs->addToWorkspace(QUrl("about:blank"),m_tabs->workspaceAt(source),m_tabs->profileAt(source));
    m_mutating=false;
    splitWith(m_tabs->idAt(added),horizontal);
}
bool SplitLayout::splitWith(int tab,bool horizontal,bool before) {
    auto &t=tree(); const int added=row(tab);
    if(m_mutating || !t.root || added<0) return false;
    for(const auto &node:t.nodes) if(!node.first && node.tab==tab) return false;
    m_mutating=true;
    const int id=t.active,a=m_nextId++,b=m_nextId++;
    auto previous=t.nodes.value(id); previous.id=before ? b : a;
    t.nodes.insert(previous.id,previous);
    const int inserted=before ? a : b;
    t.nodes.insert(inserted,{inserted,tab,0,0,true,.5,m_tabs->workspaceAt(added)});
    t.nodes[id]={id,0,a,b,horizontal,.5,{}}; t.active=inserted;
    m_tabs->setCurrentIndex(added); m_mutating=false; publish(); return true;
}
void SplitLayout::activate(int pane) {
    auto &t=tree(); if(!t.nodes.contains(pane) || t.nodes.value(pane).first) return;
    const int index=row(t.nodes.value(pane).tab); if(index<0) return;
    t.active=pane; m_mutating=true; m_tabs->setCurrentIndex(index); m_mutating=false; publish();
}
void SplitLayout::remove(int pane) {
    auto &t=tree(); if(!multiple() || !t.nodes.contains(pane) || t.nodes.value(pane).first) return;
    int parent=0,sibling=0;
    for(const auto &n:t.nodes) if(n.first==pane || n.second==pane) { parent=n.id; sibling=n.first==pane ? n.second : n.first; break; }
    if(!parent) return;
    if(t.root==parent) t.root=sibling;
    else for(auto &n:t.nodes) { if(n.first==parent) n.first=sibling; if(n.second==parent) n.second=sibling; }
    t.nodes.remove(pane); t.nodes.remove(parent);
    if(t.active==pane) t.active=firstLeaf(t,sibling);
    activate(t.active);
}
void SplitLayout::single() {
    auto &t=tree(); if(!t.active) return;
    const auto n=t.nodes.value(t.active); t.nodes.clear(); t.nodes.insert(n.id,n); t.root=n.id; publish();
}
void SplitLayout::cycle(int direction) {
    const auto p=panes(); if(p.isEmpty()) return;
    int index=0; for(int i=0;i<p.size();++i) if(p[i].toMap().value("paneId").toInt()==activePane()) index=i;
    activate(p[(index+direction%p.size()+p.size())%p.size()].toMap().value("paneId").toInt());
}
bool SplitLayout::nextHiddenTab() { return cycleTab(1); }
bool SplitLayout::cycleTab(int direction) {
    const auto *t=current();
    if(!direction || m_mutating || !t || !t->active || m_tabs->currentIndex()<0) return false;
    QSet<int> visible;
    for(const auto &n:t->nodes) if(!n.first) visible.insert(n.tab);
    const int count=m_tabs->count(),start=m_tabs->currentIndex();
    const auto workspace=m_tabs->workspaceAt(start);
    for(int offset=1;offset<count;++offset) {
        const int candidate=(start+(direction<0 ? -offset : offset)+count)%count;
        if(m_tabs->workspaceAt(candidate)==workspace && !visible.contains(m_tabs->idAt(candidate))) {
            // Selection replaces the active leaf while retaining all pane IDs
            // and divider proportions through synchronize(true).
            m_tabs->setCurrentIndex(candidate);
            return true;
        }
    }
    return false;
}
void SplitLayout::resize(int id,qreal x,qreal y) {
    if(!std::isfinite(x) || !std::isfinite(y)) return;
    auto &t=tree(); if(!t.nodes.contains(id) || !t.nodes.value(id).first) return;
    for(const auto &value:handles()) {
        const auto h=value.toMap(); if(h.value("handleId").toInt()!=id) continue;
        auto &n=t.nodes[id]; const qreal length=h.value("length").toReal(); if(length<=0) return;
        const auto a=minimum(t,n.first),b=minimum(t,n.second);
        const qreal minA=n.horizontal ? a.width() : a.height(),minB=n.horizontal ? b.width() : b.height();
        n.ratio=std::clamp(((n.horizontal ? x : y)-h.value("origin").toReal())/length,minA/length,qMax(minA/length,1-minB/length));
        publish(); return;
    }
}
void SplitLayout::swap(int a,int b) {
    auto &t=tree(); if(a==b || !t.nodes.contains(a) || !t.nodes.contains(b) || t.nodes.value(a).first || t.nodes.value(b).first) return;
    std::swap(t.nodes[a].tab,t.nodes[b].tab);
    std::swap(t.nodes[a].workspace,t.nodes[b].workspace);
    // Keep focus with the page being dragged rather than its old position.
    if(t.active==a) t.active=b; else if(t.active==b) t.active=a;
    activate(t.active);
}
QVariantMap SplitLayout::snapshot() const {
    QVariantMap result;
    for(auto it=m_trees.cbegin();it!=m_trees.cend();++it) {
        if(!it->root) continue;
        QVariantList nodes;
        auto ids=it->nodes.keys(); std::sort(ids.begin(),ids.end());
        for(int id:ids) { const auto n=it->nodes.value(id); nodes.append(QVariantMap{{"id",n.id},{"tab",n.tab},{"first",n.first},{"second",n.second},{"horizontal",n.horizontal},{"ratio",n.ratio},{"workspace",n.workspace}}); }
        result[it.key()]=QVariantMap{{"root",it->root},{"active",it->active},{"nodes",nodes}};
    }
    return result;
}
void SplitLayout::restore(const QVariantMap &data) {
    m_trees.clear();
    for(auto it=data.cbegin();it!=data.cend();++it) {
        const auto saved=it.value().toMap(); Tree t; bool valid=true;
        const auto nodes=saved.value("nodes").toList(); if(nodes.size()>3999) continue;
        for(const auto &value:nodes) {
            const auto n=value.toMap(); const int id=n.value("id").toInt();
            const qreal ratio=n.value("ratio",.5).toReal();
            if(id<=0 || id>10000000 || t.nodes.contains(id) || !std::isfinite(ratio)) { valid=false; break; }
            t.nodes.insert(id,{id,n.value("tab").toInt(),n.value("first").toInt(),n.value("second").toInt(),n.value("horizontal").toBool(),std::clamp(ratio,0.,1.),n.value("workspace",it.key()).toString()});
            m_nextId=qMax(m_nextId,id+1);
        }
        t.root=saved.value("root").toInt(); t.active=saved.value("active").toInt();
        QSet<int> visited,tabs; QList<int> pending{t.root};
        while(valid && !pending.isEmpty()) {
            const int id=pending.takeLast();
            if(!t.nodes.contains(id) || visited.contains(id)) { valid=false; break; }
            visited.insert(id); const auto n=t.nodes.value(id);
            if(n.first && n.second && !n.tab) { pending.append(n.first); pending.append(n.second); }
            else if(!n.first && !n.second && n.tab>0 && !tabs.contains(n.tab)) tabs.insert(n.tab);
            else valid=false;
        }
        if(valid && visited.size()==t.nodes.size()) m_trees.insert(it.key(),t);
    }
    m_mutating=false;
    synchronize(false);
}
void SplitLayout::forgetWorkspace(const QString &workspace) {
    m_trees.remove(workspace); publish();
}
