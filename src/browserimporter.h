#pragma once
#include "importdata.h"
#include <QObject>
#include <QVariantList>
#include <QSet>
#include <functional>
class QWindow;
class PasswordManager;
class History;

// Only counts, file labels and status reach QML. Password rows stay in C++.
class BrowserImporter final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString passkeyImportNotice READ passkeyImportNotice CONSTANT)
    Q_PROPERTY(QUrl browserAccessDirectory READ browserAccessDirectory NOTIFY changed)
    Q_PROPERTY(QString selectedBrowser READ selectedBrowser NOTIFY changed)
    Q_PROPERTY(int selectedProfileIndex READ selectedProfileIndex NOTIFY changed)
    Q_PROPERTY(bool selectedNeedsAccess READ selectedNeedsAccess NOTIFY changed)
    Q_PROPERTY(bool requestingAccess READ requestingAccess NOTIFY changed)
    Q_PROPERTY(QString discoveryWarning READ discoveryWarning NOTIFY changed)
    Q_PROPERTY(QVariantList profiles READ profiles NOTIFY changed)
    Q_PROPERTY(bool direct READ direct WRITE setDirect NOTIFY changed)
    Q_PROPERTY(bool importPasswords READ importPasswords WRITE setImportPasswords NOTIFY changed)
    Q_PROPERTY(bool importHistory READ importHistory WRITE setImportHistory NOTIFY changed)
    Q_PROPERTY(bool directPasswordsSupported READ directPasswordsSupported NOTIFY changed)
    Q_PROPERTY(bool directHistorySupported READ directHistorySupported NOTIFY changed)
    Q_PROPERTY(QString passwordFileLabel READ passwordFileLabel NOTIFY changed)
    Q_PROPERTY(QString historyFileLabel READ historyFileLabel NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool importing READ importing NOTIFY changed)
    Q_PROPERTY(bool ready READ ready NOTIFY changed)
    Q_PROPERTY(int passwordCount READ passwordCount NOTIFY changed)
    Q_PROPERTY(int historyCount READ historyCount NOTIFY changed)
    Q_PROPERTY(int skippedPasswords READ skippedPasswords NOTIFY changed)
    Q_PROPERTY(int skippedHistory READ skippedHistory NOTIFY changed)
    Q_PROPERTY(int processed READ processed NOTIFY changed)
    Q_PROPERTY(QString progress READ progress NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(QString message READ message NOTIFY changed)
public:
    using AccessCompletion=std::function<void(bool,QString)>;
    using AccessRequester=std::function<void(QWindow *,const QUrl &,AccessCompletion)>;
    using ProfileReader=std::function<QVariantList()>;
    using VaultReader=std::function<ImportPreview(const QVariantMap &)>;
    BrowserImporter(PasswordManager *passwords,History *history,QObject *parent=nullptr,
                    ProfileReader profiles={},VaultReader vault={},AccessRequester access={});
    QString passkeyImportNotice() const { return tr("Passkeys cannot be imported from these browser profiles. Keep using your current passkey provider, or create new Arbiter passkeys in each account’s security settings."); }
    QUrl browserAccessDirectory() const;
    QString selectedBrowser() const { return m_selected.value("browser").toString(); }
    int selectedProfileIndex() const { return m_profiles.indexOf(m_selected); }
    bool selectedNeedsAccess() const { return m_selected.value("accessRequired").toBool(); }
    QString discoveryWarning() const { return m_discoveryWarning; }
    QVariantList profiles() const { return m_profiles; }
    bool direct() const { return m_direct; }
    void setDirect(bool direct);
    bool importPasswords() const { return m_importPasswords; }
    bool importHistory() const { return m_importHistory; }
    bool directPasswordsSupported() const { return !selectedNeedsAccess() && m_selected.value("passwordSupport").toBool(); }
    bool directHistorySupported() const { return m_selected.value("file").toUrl().isLocalFile(); }
    void setImportPasswords(bool enabled);
    void setImportHistory(bool enabled);
    QString passwordFileLabel() const;
    QString historyFileLabel() const;
    bool busy() const { return m_busy || m_requestingAccess; }
    bool requestingAccess() const { return m_requestingAccess; }
    bool importing() const { return m_importing; }
    bool ready() const { return m_ready; }
    int passwordCount() const { return m_passwordCount; }
    int historyCount() const { return m_historyCount; }
    int skippedPasswords() const { return m_preview.skippedPasswords; }
    int skippedHistory() const { return m_preview.skippedVisits; }
    int processed() const { return m_completedPasswords; }
    QString progress() const;
    QString error() const { return m_error; }
    QString message() const { return m_message; }
    Q_INVOKABLE void refreshProfiles();
    Q_INVOKABLE void requestBrowserAccess(QWindow *window,const QString &browser={});
    Q_INVOKABLE bool addProfileDirectory(const QString &browser, const QUrl &directory);
    Q_INVOKABLE void selectProfile(int index);
    Q_INVOKABLE void setPasswordFile(const QUrl &url);
    Q_INVOKABLE void setHistoryFile(const QUrl &url);
    Q_INVOKABLE void preview();
    Q_INVOKABLE void apply();
    Q_INVOKABLE void importFromBrowser();
    Q_INVOKABLE void cancel();
signals:
    void changed();
    void accessFinished(bool allowed);
    void operationFinished(bool success);
private:
    void invalidate();
    void startPreview(bool applyAfter);
    void nextPassword();
    void finish();
    AccessRequester m_accessRequester;
    bool m_requestingAccess=false;
    ProfileReader m_profileReader;
    VaultReader m_vaultReader;
    PasswordManager *m_passwords;
    History *m_history;
    QVariantList m_profiles;
    QList<QPair<QString,QString>> m_extraRoots;
    QString m_discoveryWarning;
    QSet<QString> m_authorizedRoots;
    QVariantMap m_selected;
    bool m_direct=true,m_importPasswords=true,m_importHistory=true,m_wantPasswords=true,m_wantHistory=true;
    QString m_passwordFile,m_historyFile,m_error,m_message;
    ImportPreview m_preview;
    quint64 m_generation=0;
    int m_passwordCount=0,m_historyCount=0,m_index=0,m_completedPasswords=0,m_addedPasswords=0,m_addedHistory=0;
    bool m_busy=false,m_ready=false,m_importing=false,m_cancelled=false;
};
