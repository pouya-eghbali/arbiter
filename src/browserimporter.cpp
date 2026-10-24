#include "browserimporter.h"
#include "history.h"
#include "chromevault.h"
#include "passwordmanager.h"
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QFutureWatcher>
#include <QPointer>
#include <QSet>
#include <QTimer>
#include <QStandardPaths>
#include <QWindow>
#include <filesystem>
#include <QtConcurrent/QtConcurrentRun>

#ifdef Q_OS_MACOS
void requestMacBrowserAccess(QObject *,QWindow *,const QUrl &,BrowserImporter::AccessCompletion);
QStringList installedMacBrowsers();
bool macRequiresBrowserDataConsent();
#endif

namespace {
QVariantList discoverProfiles(const QList<QPair<QString,QString>> &extraRoots, QString *warning,const QSet<QString> &authorized) {
    QStringList blocked;
    const auto checkAccess=[&](const QString &name,const QString &root) {
        std::error_code error;
        #ifdef Q_OS_WIN
        const std::filesystem::path path(root.toStdWString());
#else
        const std::filesystem::path path(QFile::encodeName(root).constData());
#endif
        std::filesystem::directory_iterator probe(path,error);
        if(error==std::errc::permission_denied || error==std::errc::operation_not_permitted) blocked.append(name);
    };
    QVariantList result{QVariantMap{{"label",QStringLiteral("Choose a browser profile…")},{"file",QUrl()}}};
    QList<QPair<QString,QString>> roots;
    QStringList firefox;
    const auto home=QDir::homePath();
#ifdef Q_OS_MACOS
    const auto base=home+"/Library/Application Support/";
    roots={{"Chrome",base+"Google/Chrome"},{"Edge",base+"Microsoft Edge"},{"Brave",base+"BraveSoftware/Brave-Browser"},{"Chromium",base+"Chromium"},{"Vivaldi",base+"Vivaldi"},{"Arc",base+"Arc/User Data"}};
    firefox={base+"Firefox"};
#elif defined(Q_OS_WIN)
    const auto local=qEnvironmentVariable("LOCALAPPDATA",home+"/AppData/Local")+"/";
    roots={{"Chrome",local+"Google/Chrome/User Data"},{"Edge",local+"Microsoft/Edge/User Data"},{"Brave",local+"BraveSoftware/Brave-Browser/User Data"},{"Chromium",local+"Chromium/User Data"},{"Vivaldi",local+"Vivaldi/User Data"}};
    firefox={qEnvironmentVariable("APPDATA",home+"/AppData/Roaming")+"/Mozilla/Firefox/Profiles"};
#else
    const auto configured=qEnvironmentVariable("XDG_CONFIG_HOME");
    const auto base=(!configured.isEmpty() && QDir::isAbsolutePath(configured) ? configured : home+"/.config")+"/";
    roots={{"Chrome",base+"google-chrome"},{"Edge",base+"microsoft-edge"},{"Brave",base+"BraveSoftware/Brave-Browser"},{"Chromium",base+"chromium"},{"Vivaldi",base+"vivaldi"}};
    firefox={home+"/.mozilla/firefox",home+"/snap/firefox/common/.mozilla/firefox",home+"/.var/app/org.mozilla.firefox/.mozilla/firefox"};
#endif
#ifdef Q_OS_MACOS
    const auto installed=installedMacBrowsers();
    const bool protectedData=macRequiresBrowserDataConsent();
#else
    const QStringList installed;
    const bool protectedData=false;
#endif
    const auto needsAccess=[&](const QString &browser,const QString &root) {
        const bool protectedRoot=protectedData && !authorized.contains(root) && !extraRoots.contains({browser,root});
        if(!protectedRoot) return false;
        if(installed.contains(browser)) {
            blocked.append(browser);
            result.append(QVariantMap{{"label",browser+BrowserImporter::tr(" — allow access")},{"browser",browser},
                {"directory",root},{"accessRequired",true},{"accessDirectory",QUrl::fromLocalFile(root)},
                {"passwordSupport",browser!="Firefox" && browser!="Safari"},{"historySupport",true}});
        }
        // Don't probe protected browser data just to populate the browser list.
        return true;
    };
    QSet<QString> seen;
    const auto add=[&](const QString &label,const QString &directory,const QString &browser,const QString &history) {
        const auto canonical=QFileInfo(directory).canonicalFilePath();
        const bool vault=QFileInfo::exists(directory+"/Login Data") || QFileInfo::exists(directory+"/Login Data For Account");
        if((!QFileInfo::exists(history) && !vault) || seen.contains(canonical)) return;
        seen.insert(canonical);
        const QString account=browser=="Edge" ? "Microsoft Edge" : browser;
        const QString application=browser=="Chrome" ? "chrome" : browser=="Edge" ? "microsoft-edge" : browser.toLower();
        result.append(QVariantMap{{"label",label},{"file",QFileInfo::exists(history) ? QUrl::fromLocalFile(history) : QUrl()},
            {"directory",directory},{"browser",browser},{"passwordSupport",vault && browser!="Firefox" && browser!="Safari"},
            {"service",account+" Safe Storage"},{"account",account},{"linuxApplication",application},
            {"localState",QFileInfo(directory).dir().filePath("Local State")}});
    };
    for(const auto &extra:extraRoots) {
        if(extra.first=="Firefox") firefox.append(extra.second);
        else roots.append(extra);
    }
    for(const auto &[name,root]:roots) {
        if(needsAccess(name,root)) continue;
        checkAccess(name,root);
        add(name,root,name,root+(name=="Safari" ? "/History.db" : "/History"));
        const auto folders=QDir(root).entryList(QDir::Dirs|QDir::NoDotAndDotDot,QDir::Name);
        for(const auto &folder:folders.mid(0,200)) add(name+" — "+folder,root+"/"+folder,name,root+"/"+folder+"/History");
    }
    for(const auto &root:firefox) {
        if(needsAccess("Firefox",root)) continue;
        checkAccess("Firefox",root);
        add("Firefox",root,"Firefox",root+"/places.sqlite");
        const auto profileRoot=QDir(root+"/Profiles").exists() ? root+"/Profiles" : root;
        const auto folders=QDir(profileRoot).entryList(QDir::Dirs|QDir::NoDotAndDotDot,QDir::Name);
        for(const auto &folder:folders.mid(0,200)) add("Firefox — "+folder,profileRoot+"/"+folder,"Firefox",profileRoot+"/"+folder+"/places.sqlite");
    }
#ifdef Q_OS_MACOS
    if(!needsAccess("Safari",home+"/Library/Safari")) add("Safari",home+"/Library/Safari","Safari",home+"/Library/Safari/History.db");
#endif
    blocked.removeDuplicates();
    *warning=blocked.isEmpty() ? QString() : BrowserImporter::tr("Access needed for %1. Select a browser to allow access to its data folder.").arg(blocked.join(", "));
    return result;
}
}
BrowserImporter::BrowserImporter(PasswordManager *passwords,History *history,QObject *parent,
                                ProfileReader profiles,VaultReader vault,AccessRequester access)
    : QObject(parent),m_accessRequester(access ? std::move(access) : [this](QWindow *window,const QUrl &directory,AccessCompletion completion) {
#ifdef Q_OS_MACOS
        requestMacBrowserAccess(this,window,directory,std::move(completion));
#else
        Q_UNUSED(window); Q_UNUSED(directory);
        completion(true,{});
#endif
    }),m_profileReader(profiles ? std::move(profiles) : [this] { return discoverProfiles(m_extraRoots,&m_discoveryWarning,m_authorizedRoots); }),
      m_vaultReader(vault ? std::move(vault) : ChromeVault::read),m_passwords(passwords),m_history(history) {}
QUrl BrowserImporter::browserAccessDirectory() const {
    if(selectedNeedsAccess()) return m_selected.value("accessDirectory").toUrl();
    for(const auto &value:m_profiles) {
        const auto row=value.toMap();
        if(row.value("accessRequired").toBool()) return row.value("accessDirectory").toUrl();
    }
    return {};
}
QString BrowserImporter::passwordFileLabel() const { return QFileInfo(m_passwordFile).fileName(); }
QString BrowserImporter::historyFileLabel() const { return QFileInfo(m_historyFile).fileName(); }
void BrowserImporter::requestBrowserAccess(QWindow *window,const QString &browser) {
    if(busy()) return;
    QVariantMap target;
    const auto name=browser.isEmpty() ? selectedBrowser() : browser;
    for(const auto &value:m_profiles) {
        const auto row=value.toMap();
        if(row.value("accessRequired").toBool() && (name.isEmpty() || row.value("browser")==name)) { target=row; break; }
    }
    if(target.isEmpty()) { m_discoveryWarning=tr("Choose a browser that needs access first."); emit changed(); return; }
    const auto directory=target.value("accessDirectory").toUrl();
    const auto targetBrowser=target.value("browser").toString();
    m_requestingAccess=true;
    emit changed();
    QPointer<BrowserImporter> self(this);
    m_accessRequester(window,directory,[self,directory,targetBrowser](bool approved,QString error) {
        if(!self) return;
        QMetaObject::invokeMethod(self,[self,directory,targetBrowser,approved,error] {
            if(!self) return;
            self->m_requestingAccess=false;
            bool readable=false;
            if(approved) {
                self->m_authorizedRoots.insert(directory.toLocalFile());
                self->refreshProfiles();
                for(int i=0;i<self->m_profiles.size();++i) {
                    const auto row=self->m_profiles[i].toMap();
                    if(row.value("browser")==targetBrowser && !row.value("accessRequired").toBool()
                        && (row.value("directory").toString()==directory.toLocalFile() || row.value("directory").toString().startsWith(directory.toLocalFile()+"/"))
                        && (row.value("file").toUrl().isLocalFile() || row.value("passwordSupport").toBool())) {
                        self->selectProfile(i); readable=true; break;
                    }
                }
                if(!readable) {
                    self->m_authorizedRoots.remove(directory.toLocalFile());
                    self->refreshProfiles();
                    self->m_discoveryWarning=tr("%1 profiles are still unavailable. Confirm access to %1’s own data folder; access to a parent folder is insufficient.").arg(targetBrowser);
                }
            } else if(!error.isEmpty()) self->m_discoveryWarning=error;
            emit self->changed();
            emit self->accessFinished(readable);
        },Qt::QueuedConnection);
    });
}
void BrowserImporter::refreshProfiles() {
    if(busy()) return;
    const auto error=m_error,message=m_message;
    invalidate(); m_selected={}; m_importPasswords=m_importHistory=false; m_profiles=m_profileReader();
    m_error=error; m_message=message; emit changed();
}

bool BrowserImporter::addProfileDirectory(const QString &browser,const QUrl &directory) {
    const QStringList supported{"Chrome","Firefox","Edge","Brave","Chromium","Vivaldi","Arc","Safari"};
    if(busy() || !directory.isLocalFile() || !supported.contains(browser)) return false;
    const auto path=directory.toLocalFile();
    if(!QFileInfo(path).isDir()) return false;
    const QPair<QString,QString> entry{browser,path};
    if(!m_extraRoots.contains(entry)) m_extraRoots.append(entry);
    refreshProfiles();
    for(const auto &value:m_profiles) {
        const auto row=value.toMap();
        const auto profile=row.value("directory").toString();
        if(row.value("browser")==browser && (profile==path || profile.startsWith(path+"/"))) return true;
    }
    const auto detail=tr("No importable %1 profiles were found in the chosen folder. Select the browser's data folder or a profile containing its history or login database.").arg(browser);
    m_discoveryWarning=m_discoveryWarning.isEmpty() ? detail : m_discoveryWarning+"\n"+detail;
    emit changed();
    return false;
}
void BrowserImporter::invalidate() {
    ++m_generation; m_ready=false; m_preview={}; m_passwordCount=m_historyCount=0; m_error.clear(); m_message.clear();
}
void BrowserImporter::selectProfile(int index) {
    if(busy() || index<0 || index>=m_profiles.size()) return;
    invalidate(); m_selected=m_profiles[index].toMap();
    m_importPasswords=m_wantPasswords && directPasswordsSupported();
    m_importHistory=m_wantHistory && directHistorySupported();
    emit changed();
}
void BrowserImporter::setPasswordFile(const QUrl &url) {
    if(busy()) return;
    invalidate(); m_passwordFile=url.isLocalFile() ? url.toLocalFile() : QString(); emit changed();
}
void BrowserImporter::setHistoryFile(const QUrl &url) {
    if(busy()) return;
    invalidate(); m_historyFile=url.isLocalFile() ? url.toLocalFile() : QString(); emit changed();
}
void BrowserImporter::setDirect(bool direct) {
    if(busy() || m_direct==direct) return;
    invalidate(); m_direct=direct;
    m_importPasswords=m_wantPasswords && (!direct || directPasswordsSupported());
    m_importHistory=m_wantHistory && (!direct || directHistorySupported());
    emit changed();
}
void BrowserImporter::setImportPasswords(bool enabled) {
    if(busy()) return;
    invalidate(); m_wantPasswords=enabled; m_importPasswords=enabled && (!m_direct || directPasswordsSupported()); emit changed();
}
void BrowserImporter::setImportHistory(bool enabled) {
    if(busy()) return;
    invalidate(); m_wantHistory=enabled; m_importHistory=enabled && (!m_direct || directHistorySupported()); emit changed();
}
void BrowserImporter::preview() { startPreview(false); }
void BrowserImporter::importFromBrowser() { startPreview(true); }
void BrowserImporter::startPreview(bool applyAfter) {
    if(busy()) return;
    invalidate();
    if(m_direct && (!m_selected.contains("directory") || (!m_importPasswords && !m_importHistory))) { m_error=tr("Select a browser profile and the data to import."); emit changed(); emit operationFinished(false); return; }
    if(m_direct && m_importPasswords && !directPasswordsSupported()) { m_error=tr("This browser requires its password export. Use Import from files for passwords, or select history only."); emit changed(); emit operationFinished(false); return; }
    if(!m_direct && m_passwordFile.isEmpty() && m_historyFile.isEmpty()) { m_error=tr("Choose a password CSV, a history source, or both."); emit changed(); emit operationFinished(false); return; }
    m_busy=true; const auto generation=m_generation;
    auto *watcher=new QFutureWatcher<ImportPreview>(this);
    connect(watcher,&QFutureWatcher<ImportPreview>::finished,this,[this,watcher,generation,applyAfter] {
        auto result=watcher->result(); watcher->deleteLater();
        if(generation!=m_generation) {
            if(m_direct) { m_busy=false; m_message=tr("Import cancelled."); emit changed(); emit operationFinished(false); }
            return;
        }
        m_busy=false; m_error=result.error;
        if(m_error.isEmpty()) {
            // Preview excludes existing accounts; applying rechecks them too.
            QSet<QString> existing;
            for(const auto &value:m_passwords->entries()) { const auto row=value.toMap(); existing.insert(row.value("origin").toString()+QChar(0)+row.value("username").toString()); }
            for(auto it=result.passwords.begin();it!=result.passwords.end();) {
                if(existing.contains(it->url.toString()+QChar(0)+it->username)) { ++result.skippedPasswords; it=result.passwords.erase(it); }
                else ++it;
            }
            m_preview=std::move(result); m_passwordCount=m_preview.passwords.size(); m_historyCount=m_preview.visits.size();
            m_ready=m_passwordCount>0 || m_historyCount>0;
            if(!m_ready) m_message=tr("No new supported entries were found. Password rows skipped: %1. History rows skipped: %2.").arg(m_preview.skippedPasswords).arg(m_preview.skippedVisits);
        }
        emit changed();
        if(applyAfter && m_ready) QTimer::singleShot(0,this,[this,generation] { if(generation==m_generation) apply(); });
        else emit operationFinished(m_error.isEmpty());
    });
    watcher->setFuture(QtConcurrent::run([vault=m_vaultReader,direct=m_direct,source=m_selected,includePasswords=m_importPasswords,includeHistory=m_importHistory,password=m_passwordFile,history=m_historyFile] {
        if(!direct) return BrowserImport::read(password,history);
        ImportPreview result;
        if(includePasswords) result=vault(source);
        if(result.error.isEmpty() && includeHistory) {
            const auto file=source.value("file").toUrl().toLocalFile();
            if(file.isEmpty()) result.error=QStringLiteral("No history database was found for this profile. Select passwords only, or choose a different profile.");
            else {
                auto imported=BrowserImport::read({},file);
                result.visits=std::move(imported.visits); result.skippedVisits=imported.skippedVisits; result.error=imported.error;
            }
        }
        if(!result.error.isEmpty()) { result.passwords.clear(); result.visits.clear(); }
        return result;
    }));
    emit changed();
}
QString BrowserImporter::progress() const {
    if(m_requestingAccess) return tr("Waiting for browser access…");
    if(!m_busy) return {};
    if(m_importing) return tr("Saving passwords securely: %1 of %2…").arg(m_completedPasswords).arg(m_passwordCount);
    return tr("Reading browser data… Approve the system unlock prompt if one appears.");
}
void BrowserImporter::apply() {
    if(busy() || !m_ready) return;
    if(m_passwords->busy()) { m_error=tr("Password storage is busy. Try again when the current operation finishes."); emit changed(); emit operationFinished(false); return; }
    m_busy=m_importing=true; m_ready=false; m_cancelled=false;
    m_index=m_completedPasswords=m_addedPasswords=m_addedHistory=0; m_error.clear(); m_message.clear();
    emit changed();
    if(!m_preview.visits.isEmpty()) {
        m_addedHistory=m_history->importVisits(m_preview.visits);
        if(m_addedHistory<0) { m_addedHistory=0; m_error=m_history->error(); finish(); return; }
    }
    QTimer::singleShot(0,this,&BrowserImporter::nextPassword);
}
void BrowserImporter::nextPassword() {
    if(m_cancelled || m_index>=m_preview.passwords.size()) { finish(); return; }
    auto &row=m_preview.passwords[m_index++]; const QPointer<BrowserImporter> guard(this);
    m_passwords->importAccount(row.url,row.username,row.password,[guard](bool imported,const QString &error) {
        if(!guard) return;
        if(!error.isEmpty()) { guard->m_error=error; guard->finish(); return; }
        ++guard->m_completedPasswords;
        if(imported) ++guard->m_addedPasswords;
        emit guard->changed();
        QTimer::singleShot(0,guard,&BrowserImporter::nextPassword);
    });
    // A synchronous store can finish the entire operation in the callback.
    if(m_importing && m_index<=m_preview.passwords.size()) m_preview.passwords[m_index-1].password.clear();
}
void BrowserImporter::finish() {
    m_busy=m_importing=false;
    const auto passwordTotal=m_addedPasswords==1 ? tr("1 password") : tr("%1 passwords").arg(m_addedPasswords);
    const auto historyTotal=m_addedHistory==1 ? tr("1 history visit") : tr("%1 history visits").arg(m_addedHistory);
    m_message=tr("Imported %1 and %2. Passwords not imported: %3. History entries not imported: %4. Existing passwords were kept; history retains the latest 10,000 visits.")
        .arg(passwordTotal,historyTotal).arg(m_preview.skippedPasswords+m_passwordCount-m_addedPasswords)
        .arg(m_preview.skippedVisits+m_historyCount-m_addedHistory);
    if(m_cancelled) m_message=tr("Import stopped. ")+m_message;
    if(!m_error.isEmpty()) m_message=tr("Import stopped after a storage error. ")+m_message;
    m_preview={}; emit changed(); emit operationFinished(m_error.isEmpty() && !m_cancelled);
}
void BrowserImporter::cancel() {
    if(m_requestingAccess) return;
    if(m_importing) { m_cancelled=true; m_message=tr("Stopping after the current password is saved…"); emit changed(); return; }
    if(m_busy && m_direct) {
        ++m_generation; m_ready=false; m_message=tr("Cancel the system authentication prompt if it is open. Waiting for the browser read to finish…"); emit changed(); return;
    }
    const bool pending=m_ready || m_busy;
    invalidate(); m_busy=false; emit changed(); if(pending) emit operationFinished(false);
}
