#include "workspaces.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUuid>

Workspaces::Workspaces(Tabs *tabs, QObject *parent) : Workspaces(tabs,
    QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)+"/workspaces",parent) {}
Workspaces::Workspaces(Tabs *tabs, const QString &directory, QObject *parent)
    : QObject(parent), m_tabs(tabs), m_splits(tabs), m_directory(directory) {
    m_splits.suspend();
    QVariantMap savedSplits;
    m_spaces.append({"main",tr("Personal"),QColor("#8dbbff"),false}); m_current="main";
    if (!QDir().mkpath(directory)) { m_writable=false; m_error=tr("Workspaces cannot be saved: could not create storage."); }
    else {
        QFile::setPermissions(directory,QFileDevice::ReadOwner|QFileDevice::WriteOwner|QFileDevice::ExeOwner);
        m_lock=std::make_unique<QLockFile>(directory+"/session.lock");
        if (!m_lock->tryLock(0)) { m_writable=false; m_error=tr("Workspaces are open in another Arbiter process. Changes in this window will not be saved."); }
    }
    QFile file(directory+"/session.json");
    if (file.exists()) {
        QJsonParseError parse;
        const bool readable=file.open(QIODevice::ReadOnly) && file.size()<=32*1024*1024;
        const auto document=readable ? QJsonDocument::fromJson(file.readAll(),&parse) : QJsonDocument();
        const auto object=document.object();
        if(!readable || parse.error!=QJsonParseError::NoError || object.value("version").toInt()!=1 || object.value("workspaces").toArray().isEmpty()) {
            m_writable=false; m_error=tr("Saved workspaces could not be read. The existing file is preserved; changes will not be saved.");
        } else {
            QList<Space> spaces;
            QHash<QString,QString> profiles;
            for (const auto &value:object.value("workspaces").toArray()) {
                if(spaces.size()>=50) break;
                const auto s=value.toObject(); const auto id=s.value("id").toString(); const auto name=s.value("name").toString().simplified().left(48);
                const QColor color(s.value("color").toString());
                if ((id!="main" && QUuid(id).isNull()) || profiles.contains(id) || name.isEmpty() || !color.isValid() || color.alpha()!=255) continue;
                Space space{id,name,color,s.value("isolated").toBool()}; spaces.append(space); profiles[id]=profile(space);
            }
            if (spaces.isEmpty()) { m_writable=false; m_error=tr("Saved workspaces are invalid. The existing file is preserved."); }
            else {
                m_spaces=spaces; m_current=object.value("current").toString();
                if(indexOf(m_current)<0) m_current=m_spaces.first().id;
                m_tabs->restore(object.value("tabs").toArray().toVariantList(),profiles,object.value("selected").toObject().toVariantMap());
                savedSplits=object.value("splits").toObject().toVariantMap();
            }
        }
    }
    m_tabs->setWorkspace(m_current,profile(m_spaces[indexOf(m_current)]));
    for(auto it=savedSplits.begin();it!=savedSplits.end();) {
        if(indexOf(it.key())<0) it=savedSplits.erase(it); else ++it;
    }
    m_splits.restore(savedSplits);
    m_save.setSingleShot(true); m_save.setInterval(350);
    connect(&m_save,&QTimer::timeout,this,&Workspaces::flush);
    connect(m_tabs,&Tabs::sessionChanged,this,&Workspaces::scheduleSave);
    connect(&m_splits,&SplitLayout::sessionChanged,this,&Workspaces::scheduleSave);
    connect(qApp,&QCoreApplication::aboutToQuit,this,&Workspaces::flush);
}
Workspaces::~Workspaces() { if(m_save.isActive()) flush(); }
int Workspaces::indexOf(const QString &id) const { for(int i=0;i<count();++i) if(m_spaces[i].id==id) return i; return -1; }
QString Workspaces::profile(const Space &space) const { return space.isolated ? "Workspace-"+space.id : "Default"; }
QString Workspaces::currentName() const { const int i=indexOf(m_current); return i<0 ? QString() : m_spaces[i].name; }
QColor Workspaces::currentColor() const { const int i=indexOf(m_current); return i<0 ? QColor() : m_spaces[i].color; }
QVariantList Workspaces::choices() const {
    QVariantList result;
    for(const auto &s:m_spaces) {
        int tabs=0; for(int i=0;i<m_tabs->count();++i) if(m_tabs->workspaceAt(i)==s.id) ++tabs;
        result.append(QVariantMap{{"id",s.id},{"name",s.name},{"color",s.color},{"isolated",s.isolated},{"profile",profile(s)},{"tabs",tabs}});
    }
    return result;
}
bool Workspaces::fail(const QString &error) { m_error=error; emit changed(); return false; }
void Workspaces::scheduleSave() { m_save.start(); emit changed(); }
void Workspaces::activate(const QString &id) {
    const int i=indexOf(id); if(i<0) return;
    const bool switching=m_current!=id;
    m_current=id; m_tabs->setWorkspace(id,profile(m_spaces[i]));
    if(switching) emit currentChanged();
    scheduleSave();
}
void Workspaces::cycle(int direction) { activate(m_spaces[(indexOf(m_current)+direction%count()+count())%count()].id); }
QString Workspaces::create(const QString &name, const QColor &color, bool isolated) {
    if(name.simplified().isEmpty() || !color.isValid() || color.alpha()!=255) { fail(tr("Choose a name and an opaque color.")); return {}; }
    if(count()>=50) { fail(tr("You can create up to 50 workspaces.")); return {}; }
    const auto id=QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_spaces.append({id,name.simplified().left(48),color,isolated}); activate(id); return id;
}
bool Workspaces::edit(const QString &id, const QString &name, const QColor &color) {
    const int i=indexOf(id); if(i<0 || name.simplified().isEmpty() || !color.isValid() || color.alpha()!=255) return fail(tr("Choose a name and an opaque color."));
    m_spaces[i].name=name.simplified().left(48); m_spaces[i].color=color; scheduleSave(); return true;
}
bool Workspaces::remove(const QString &id) {
    const int i=indexOf(id); if(i<0 || count()==1) return fail(tr("Keep at least one workspace."));
    if(id==m_current) activate(m_spaces[i==0 ? 1 : i-1].id);
    m_tabs->removeWorkspace(id); m_spaces.removeAt(i); m_splits.forgetWorkspace(id);
    if(m_tabs->currentIndex()<0) m_tabs->add();
    scheduleSave(); return true;
}
void Workspaces::reorder(const QString &id, int direction) {
    const int i=indexOf(id), target=i+direction;
    if(i<0 || target<0 || target>=count()) return;
    m_spaces.move(i,target); scheduleSave();
}
bool Workspaces::moveTab(int row, const QString &id) {
    const int i=indexOf(id); if(i<0) return false;
    if(!m_tabs->moveTo(row,id,profile(m_spaces[i]))) return fail(tr("This workspace has a different sign-in session. Open a copy there instead."));
    activate(id); m_tabs->setCurrentIndex(row); scheduleSave(); return true;
}
bool Workspaces::copyTab(int row, const QString &id) {
    if(row<0 || row>=m_tabs->count() || indexOf(id)<0) return false;
    const auto url=m_tabs->data(m_tabs->index(row),Tabs::Url).toUrl(); activate(id); m_tabs->add(url); return true;
}
int Workspaces::addTab(const QString &id, const QUrl &url, bool activateTab) {
    const int i=indexOf(id); if(i<0) return -1;
    if(activateTab) activate(id);
    const int row=m_tabs->addToWorkspace(url,id,profile(m_spaces[i]));
    if(activateTab) m_tabs->setCurrentIndex(row);
    return row;
}
void Workspaces::flush() {
    m_save.stop(); if(!m_writable) return;
    QJsonArray spaces;
    for(const auto &s:m_spaces) spaces.append(QJsonObject{{"id",s.id},{"name",s.name},{"color",s.color.name()},{"isolated",s.isolated}});
    const QJsonDocument document(QJsonObject{{"version",1},{"current",m_current},{"workspaces",spaces},
        {"tabs",QJsonArray::fromVariantList(m_tabs->snapshot())},{"selected",QJsonObject::fromVariantMap(m_tabs->selections())},
        {"splits",QJsonObject::fromVariantMap(m_splits.snapshot())}});
    QSaveFile file(m_directory+"/session.json");
    if(!file.open(QIODevice::WriteOnly)) { fail(tr("Could not save workspaces.")); return; }
    file.setPermissions(QFileDevice::ReadOwner|QFileDevice::WriteOwner);
    const auto bytes=document.toJson(QJsonDocument::Compact);
    if(file.write(bytes)!=bytes.size() || !file.commit()) fail(tr("Could not save workspaces."));
}
