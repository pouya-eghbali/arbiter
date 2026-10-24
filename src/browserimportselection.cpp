#include "browserimportselection.h"
#include "browserimporter.h"
#include <QTimer>
#include <QSet>

BrowserImportSelection::BrowserImportSelection(BrowserImporter *importer,QObject *parent):QObject(parent),m_importer(importer) {
    connect(importer,&BrowserImporter::accessFinished,this,[this](bool allowed) {
        if(m_accessBrowser.isEmpty()) return;
        if(!allowed) for(int i=0;i<m_browsers.size();++i) {
            auto row=m_browsers[i].toMap();
            if(row.value("name")==m_accessBrowser) { row["selected"]=false; m_browsers[i]=row; }
        }
        m_accessBrowser.clear();
        refresh(true,false);
    });
    connect(importer,&BrowserImporter::operationFinished,this,[this](bool success) {
        if(!m_busy || m_current<0) return;
        const auto source=m_importer->profiles().value(m_current).toMap();
        m_results.append(QVariantMap{{"label",source.value("label")},{"success",success},
            {"message",success ? m_importer->message() : !m_importer->error().isEmpty() ? m_importer->error() : m_importer->message()}});
        if(!success) m_failed.append(m_current);
        m_current=-1;
        emit changed();
        const auto generation=m_generation;
        QTimer::singleShot(0,this,[this,generation] { if(m_busy && generation==m_generation) next(); });
    });
}
int BrowserImportSelection::selectedCount() const {
    int count=0; for(const auto &value:m_browsers) if(value.toMap().value("selected").toBool()) ++count; return count;
}
QString BrowserImportSelection::progress() const {
    if(!m_busy) return {};
    const auto label=m_importer->profiles().value(m_current).toMap().value("label").toString();
    return m_cancelled ? tr("Stopping after the current operation… Cancel any system unlock prompt that is still open.")
        : tr("Importing profile %1 of %2 · %3").arg(m_position).arg(m_queue.size()).arg(label);
}
void BrowserImportSelection::refresh(bool preserveSelection,bool rescan) {
    if(m_busy || m_importer->busy()) return;
    QSet<QString> selected;
    if(preserveSelection) for(const auto &value:m_browsers) {
        const auto row=value.toMap();
        if(row.value("selected").toBool()) selected.insert(row.value("name").toString());
    }
    if(rescan) m_importer->refreshProfiles();
    m_browsers.clear(); m_results.clear(); m_failed.clear(); m_done=false;
    const auto profiles=m_importer->profiles();
    for(int index=0;index<profiles.size();++index) {
        const auto source=profiles[index].toMap();
        if(!source.contains("directory")) continue;
        QString name=source.value("browser").toString();
        if(name.isEmpty()) name=source.value("label").toString().section(QStringLiteral(" — "),0,0);
        if(name.isEmpty()) continue;
        int group=-1;
        for(int i=0;i<m_browsers.size();++i) if(m_browsers[i].toMap().value("name").toString()==name) { group=i; break; }
        QVariantMap row=group>=0 ? m_browsers[group].toMap() : QVariantMap{{"name",name},{"selected",selected.contains(name)},{"passwords",false},{"history",false},{"accessRequired",false}};
        auto indices=row.value("profiles").toList(); indices.append(index); row["profiles"]=indices;
        auto labels=row.value("labels").toStringList(); labels.append(source.value("label").toString()); row["labels"]=labels;
        row["passwords"]=row.value("passwords").toBool() || source.value("passwordSupport").toBool();
        row["history"]=row.value("history").toBool() || source.value("historySupport").toBool() || source.value("file").toUrl().isLocalFile();
        row["accessRequired"]=row.value("accessRequired").toBool() || source.value("accessRequired").toBool();
        if(group<0) m_browsers.append(row); else m_browsers[group]=row;
    }
    emit changed();
}
void BrowserImportSelection::toggle(int index,QWindow *window) {
    if(m_importer->busy() || m_busy || index<0 || index>=m_browsers.size()) return;
    auto row=m_browsers[index].toMap(); row["selected"]=!row.value("selected").toBool(); m_browsers[index]=row;
    m_done=false; m_results.clear(); m_failed.clear(); emit changed();
    if(row.value("selected").toBool() && row.value("accessRequired").toBool()) {
        m_accessBrowser=row.value("name").toString();
        m_importer->requestBrowserAccess(window,m_accessBrowser);
    }
}
void BrowserImportSelection::start() {
    if(m_busy || m_importer->busy()) return;
    QList<int> profiles;
    for(const auto &value:m_browsers) {
        const auto row=value.toMap(); if(!row.value("selected").toBool()) continue;
        if(row.value("accessRequired").toBool()) return;
        for(const auto &profile:row.value("profiles").toList()) profiles.append(profile.toInt());
    }
    if(!profiles.isEmpty()) begin(profiles);
}
void BrowserImportSelection::retryFailed() {
    if(m_busy || m_importer->busy() || m_failed.isEmpty()) return;
    begin(m_failed);
}
void BrowserImportSelection::begin(const QList<int> &profiles) {
    ++m_generation; m_queue=profiles; m_failed.clear(); m_results.clear(); m_position=0; m_current=-1;
    m_done=false; m_cancelled=false; m_busy=true; emit changed(); next();
}
void BrowserImportSelection::next() {
    if(m_current>=0) return;
    if(m_cancelled || m_position>=m_queue.size()) { finish(); return; }
    m_current=m_queue[m_position++];
    m_importer->setDirect(true);
    m_importer->selectProfile(m_current);
    m_importer->setImportPasswords(true); m_importer->setImportHistory(true);
    emit changed(); m_importer->importFromBrowser();
}
void BrowserImportSelection::finish() {
    if(m_cancelled) for(int i=m_position;i<m_queue.size();++i) m_failed.append(m_queue[i]);
    ++m_generation; m_busy=false; m_done=true; m_current=-1; emit changed(); emit completed(!m_cancelled && m_failed.isEmpty());
}
void BrowserImportSelection::cancel() {
    if(!m_busy) return;
    m_cancelled=true;
    if(m_current>=0) m_importer->cancel(); else finish();
    emit changed();
}
