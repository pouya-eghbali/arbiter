#pragma once
#include "mobiletabs.h"
#include "mobilevault.h"
#include <QElapsedTimer>
#include <QImage>
#include <QJsonArray>
#include <QJsonObject>
#include <QMutex>
#include <QObject>
#include <QPointer>
#include <QReadWriteLock>
#include <QTimer>
#include <QVariantList>
class MobileModel : public QObject {
    Q_OBJECT
    Q_PROPERTY(MobileTabs *screens READ screens CONSTANT)
    Q_PROPERTY(QVariantList tabs READ tabs NOTIFY changed)
    Q_PROPERTY(QVariantList workspaces READ workspaces NOTIFY changed)
    Q_PROPERTY(QVariantList history READ history NOTIFY changed)
    Q_PROPERTY(QVariantMap previews READ previews NOTIFY previewsChanged)
    Q_PROPERTY(QVariantList downloads READ downloads NOTIFY changed)
    Q_PROPERTY(QVariantMap settings READ settings NOTIFY settingsChanged)
    Q_PROPERTY(QString activeId READ activeId NOTIFY changed)
    Q_PROPERTY(QString workspaceId READ workspaceId NOTIFY changed)
    Q_PROPERTY(bool fieldFocused READ fieldFocused NOTIFY changed)
    Q_PROPERTY(bool privateMode READ privateMode NOTIFY changed)
    Q_PROPERTY(bool privateLocked READ privateLocked NOTIFY changed)
    Q_PROPERTY(bool onboardingDone READ onboardingDone NOTIFY settingsChanged)
    Q_PROPERTY(QString backend READ backend CONSTANT)
    Q_PROPERTY(QString version READ version CONSTANT)
    Q_PROPERTY(bool defaultBrowserAvailable READ defaultBrowserAvailable CONSTANT)
    Q_PROPERTY(QString privacyPolicy READ privacyPolicy CONSTANT)
    Q_PROPERTY(QString thirdPartyNotices READ thirdPartyNotices CONSTANT)
    Q_PROPERTY(MobileVault *vault READ vault CONSTANT)
  public:
    explicit MobileModel(QObject *parent = nullptr, const QString &directory = QString());
    ~MobileModel();
    friend class MobileCoreTest;
    static MobileModel *instance;
    static QReadWriteLock &lifetimeLock();
    static bool filterRequest(const QUrl &, const QUrl &, const QByteArray &, const QByteArray &);
    MobileTabs *screens() {
        return &m_screens;
    }
    QVariantMap previews() const {
        return m_previews;
    }
    Q_INVOKABLE void captureActive();
    Q_INVOKABLE void setPreview(const QString &id, const QImage &image, const QString &sourceUrl = QString());
    Q_INVOKABLE void cycleTab(int direction);
    Q_INVOKABLE void findText(const QString &text, bool backwards = false);
    QVariantList tabs() const;
    QVariantList workspaces() const;
    QVariantList history() const;
    QVariantList downloads() const;
    QVariantMap settings() const {
        return m_settings.toVariantMap();
    }
    QString activeId() const {
        return m_active;
    }
    QString workspaceId() const {
        return m_workspace;
    }
    bool fieldFocused() const {
        return !m_focus.isEmpty() && m_focus.value("tab") == m_active;
    }
    bool privateMode() const {
        return m_private;
    }
    bool privateLocked() const {
        return m_private && m_locked;
    }
    bool onboardingDone() const {
        return m_settings.value("onboardingDone").toBool();
    }
    QString backend() const;
    QString version() const;
    bool defaultBrowserAvailable() const {
#if defined(Q_OS_IOS) && defined(ARBITER_IOS_DEVICE_TESTING)
        return false;
#else
        return true;
#endif
    }
    QString privacyPolicy() const;
    QString thirdPartyNotices() const;
    MobileVault *vault() {
        return &m_vault;
    }
    Q_INVOKABLE QString addTab(const QString &address = QString());
    Q_INVOKABLE void selectTab(const QString &id);
    Q_INVOKABLE void closeTab(const QString &id);
    Q_INVOKABLE void openUrl(const QUrl &url);
    Q_INVOKABLE QString takePairingInvitation();
    Q_INVOKABLE void navigate(const QString &address);
    Q_INVOKABLE QUrl addressUrl(const QString &address) const;
    Q_INVOKABLE void updatePage(const QString &id, const QString &type, const QVariantMap &data);
    Q_INVOKABLE void selectWorkspace(const QString &id);
    Q_INVOKABLE void addWorkspace(const QString &name, bool isolated);
    Q_INVOKABLE void removeWorkspace(const QString &id);
    Q_INVOKABLE void renameWorkspace(const QString &id, const QString &name);
    Q_INVOKABLE void togglePrivate();
    Q_INVOKABLE void newPrivateTab();
    Q_INVOKABLE void unlockPrivate();
    Q_INVOKABLE void setSetting(const QString &name, const QVariant &value);
    Q_INVOKABLE void toggleSiteBlocking();
    Q_INVOKABLE bool blockingFor(const QUrl &) const;
    bool blockRequest(const QUrl &url, const QUrl &source, const QByteArray &type, const QByteArray &method);
    Q_INVOKABLE QVariantList suggestions(const QString &query) const;
    Q_INVOKABLE void clearHistory(const QString &query);
    Q_INVOKABLE void removeHistory(const QString &url);
    Q_INVOKABLE void downloadEvent(const QString &type, const QVariantMap &data,
                                   bool privateDownload = false);
    Q_INVOKABLE void share();
    Q_INVOKABLE void setSystemAppearance(const QColor &color);
    Q_INVOKABLE void openDownload(const QString &id);
    Q_INVOKABLE void saveLogin(const QString &username);
    Q_INVOKABLE void dismissLogin();
    Q_INVOKABLE void dismissSuggestions() {
        m_focus = {};
        emit changed();
    }
    Q_INVOKABLE void passwordAccounts();
    Q_INVOKABLE void fillPassword(const QString &id);
    Q_INVOKABLE void suggestPassword();
    Q_INVOKABLE void copy(const QString &text);
    Q_INVOKABLE void chooseBackground(const QUrl &, bool dark);
    Q_INVOKABLE void setDefaultBrowser();
    Q_INVOKABLE void openSystemSettings();
    Q_INVOKABLE void enableCredentialProvider();
    QJsonObject page(const QString &id) const;
    Q_INVOKABLE QString formScript(bool privateMode) const;
    void backgroundSync(std::function<void(bool)> done = {});
    void cancelBackgroundSync() {
        if (m_backgroundWorker)
            m_backgroundWorker->cancel();
    }
    QPointer<MobileVault> m_backgroundWorker;
    bool verificationActive = false;
  signals:
    void changed();
    void settingsChanged();
    void toolbarScrolled(qreal delta, qreal offset);
    void previewsChanged();
    void previewReady(QString tabId);
    void message(QString text);
    void loginOffered(QString origin, QString username);
    void accountsOffered(QVariantList accounts);
    void pairingReady(QString desktop, QString pin);
    void pairingInvitationReceived();
    void defaultBrowserRequested();
    void credentialProviderRequested();
    void pageAction(QString tabId, QString action, QString argument);

  private:
    void restore();
    void persist();
    void ensureTab();
    void sleepIdle();
    void lockPrivate();
    int tabIndex(const QString &id) const;
    QJsonArray m_tabs, m_workspaces, m_history, m_downloads;
    QJsonObject m_settings, m_pending, m_focus;
    QVariantMap m_previews;
    QString m_pairingInvitation;
    QString m_active, m_workspace, m_privateContext, m_storePath;
    bool m_private = false, m_locked = false, m_syncing = false;
    MobileTabs m_screens;
    MobileVault m_vault;
    QTimer m_saveTimer, m_sleepTimer, m_syncTimer, m_privateTimer, m_pendingTimer;
    QElapsedTimer m_activity;
    QMutex m_rulesMutex;
    void *m_rules = nullptr;
};
