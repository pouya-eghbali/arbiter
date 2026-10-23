#include "biometricauth.h"
#include <QFutureWatcher>
#include <QtConcurrent/QtConcurrentRun>
#include <QGuiApplication>
#include <QWindow>
#include <QScopeGuard>
#include <QAbstractNativeEventFilter>
#include <windows.h>
#include <wtsapi32.h>
#include <userconsentverifierinterop.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Security.Credentials.UI.h>

using namespace winrt::Windows::Security::Credentials::UI;
using Verification = winrt::Windows::Foundation::IAsyncOperation<UserConsentVerificationResult>;
class WindowsBiometricAuth final : public BiometricAuth, public QAbstractNativeEventFilter {
public:
    explicit WindowsBiometricAuth(QObject *parent) : BiometricAuth(parent) {
        m_lockWindow.setFlags(Qt::Tool);
        m_notifications=WTSRegisterSessionNotification(reinterpret_cast<HWND>(m_lockWindow.winId()),NOTIFY_FOR_THIS_SESSION);
        QCoreApplication::instance()->installNativeEventFilter(this);
    }
    ~WindowsBiometricAuth() override {
        QCoreApplication::instance()->removeNativeEventFilter(this);
        WTSUnRegisterSessionNotification(reinterpret_cast<HWND>(m_lockWindow.winId()));
        cancelNative();
    }
    bool nativeEventFilter(const QByteArray &,void *message,qintptr *) override {
        const auto *event=static_cast<MSG *>(message);
        if(event->hwnd==reinterpret_cast<HWND>(m_lockWindow.winId()) &&
           ((event->message==WM_WTSSESSION_CHANGE && (event->wParam==WTS_SESSION_LOCK || event->wParam==WTS_SESSION_LOGOFF))
            || (event->message==WM_POWERBROADCAST && event->wParam==PBT_APMSUSPEND))) emit sessionLocked();
        return false;
    }
    QString name() const override { return tr("Windows Hello"); }
    bool nativePromptCanTakeFocus() const override { return true; }
    void refresh() override {
        if(busy() || m_checking) return;
        if(!m_notifications) { availability(false,tr("Windows session-lock notifications are unavailable. Use your vault passphrase.")); return; }
        m_checking=true;
        auto *watcher=new QFutureWatcher<bool>(this);
        connect(watcher,&QFutureWatcher<bool>::finished,this,[this,watcher] {
            const bool ok=watcher->result(); watcher->deleteLater(); m_checking=false;
            if(busy()) return;
            availability(ok,ok ? tr("Windows Hello is available (biometrics or your Windows Hello PIN).")
                : tr("Windows Hello is unavailable. Configure Windows Hello on Windows 11, or use your vault passphrase."));
        });
        watcher->setFuture(QtConcurrent::run([] {
            try {
                winrt::init_apartment(winrt::apartment_type::multi_threaded);
                const auto cleanup=qScopeGuard([] { winrt::uninit_apartment(); });
                auto interop=winrt::get_activation_factory<UserConsentVerifier,IUserConsentVerifierInterop>();
                return bool(interop) && UserConsentVerifier::CheckAvailabilityAsync().get()==UserConsentVerifierAvailability::Available;
            } catch(...) { return false; }
        }));
    }
protected:
    void startNative(const QString &reason, quint64 token) override {
        auto *window=QGuiApplication::focusWindow();
        if(!window) { complete(token,false,tr("Focus the browser window and try again.")); return; }
        try {
            winrt::init_apartment(winrt::apartment_type::single_threaded);
            const auto cleanup=qScopeGuard([] { winrt::uninit_apartment(); });
            const auto interop=winrt::get_activation_factory<UserConsentVerifier,IUserConsentVerifierInterop>();
            const winrt::hstring message(reason.toStdWString());
            winrt::check_hresult(interop->RequestVerificationForWindowAsync(reinterpret_cast<HWND>(window->winId()),
                reinterpret_cast<HSTRING>(winrt::get_abi(message)),winrt::guid_of<Verification>(),winrt::put_abi(m_operation)));
            const auto operation=m_operation;
            auto *watcher=new QFutureWatcher<bool>(this);
            connect(watcher,&QFutureWatcher<bool>::finished,this,[this,watcher,token] {
                const bool ok=watcher->result(); watcher->deleteLater();
                if(!current(token)) return;
                m_operation=nullptr;
                complete(token,ok,ok ? QString() : tr("Windows Hello did not verify. Use your vault passphrase or try again."));
            });
            watcher->setFuture(QtConcurrent::run([operation] {
                try {
                    winrt::init_apartment(winrt::apartment_type::multi_threaded);
                    const auto cleanup=qScopeGuard([] { winrt::uninit_apartment(); });
                    return operation.get()==UserConsentVerificationResult::Verified;
                } catch(...) { return false; }
            }));
        } catch(...) { complete(token,false,tr("Windows Hello could not start. Use your vault passphrase.")); }
    }
    void cancelNative() override { if(m_operation) { try { m_operation.Cancel(); } catch(...) {} m_operation=nullptr; } }
private:
    QWindow m_lockWindow;
    bool m_notifications=false;
    Verification m_operation{nullptr};
    bool m_checking=false;
};
BiometricAuth *createBiometricAuth(QObject *parent) { return new WindowsBiometricAuth(parent); }
