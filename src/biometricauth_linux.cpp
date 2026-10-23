#include "biometricauth.h"
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusServiceWatcher>
#include <QCoreApplication>

namespace {
const QString service=QStringLiteral("net.reactivated.Fprint");
const QString deviceInterface=QStringLiteral("net.reactivated.Fprint.Device");
QDBusPendingCall call(const QString &path,const QString &interface,const QString &method,const QVariantList &args={}) {
    auto message=QDBusMessage::createMethodCall(service,path,interface,method);
    message.setArguments(args);
    return QDBusConnection::systemBus().asyncCall(message,10000);
}
}
// Each verification gets a separate receiver with its own immutable token.
// Queued signals from an earlier attempt cannot authorize a later request.
class FingerprintSignals final : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    std::function<void(const QString &,bool)> callback;
public slots:
    void status(const QString &result,bool done) { callback(result,done); }
};
class DesktopLockSignals final : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    std::function<void()> callback;
public slots:
    void lock() { callback(); }
    void active(bool value) { if(value) callback(); }
    void properties(const QString &interface,const QVariantMap &values,const QStringList &) {
        if(interface=="org.freedesktop.login1.Session" && values.value("LockedHint").toBool()) callback();
    }
};
class LinuxBiometricAuth final : public BiometricAuth {
public:
    explicit LinuxBiometricAuth(QObject *parent) : BiometricAuth(parent) {
        auto *lock=new DesktopLockSignals(this);
        lock->callback=[this] { emit sessionLocked(); };
        auto system=QDBusConnection::systemBus();
        system.connect("org.freedesktop.login1","/org/freedesktop/login1","org.freedesktop.login1.Manager","PrepareForSleep",lock,SLOT(active(bool)));
        auto request=QDBusMessage::createMethodCall("org.freedesktop.login1","/org/freedesktop/login1","org.freedesktop.login1.Manager","GetSessionByPID");
        request.setArguments({quint32(QCoreApplication::applicationPid())});
        if(system.isConnected()) {
            auto *session=new QDBusPendingCallWatcher(system.asyncCall(request,10000),this);
            connect(session,&QDBusPendingCallWatcher::finished,this,[session,lock] {
                QDBusPendingReply<QDBusObjectPath> reply=*session; session->deleteLater();
                if(reply.isError()) return;
                auto bus=QDBusConnection::systemBus();
                bus.connect("org.freedesktop.login1",reply.value().path(),"org.freedesktop.login1.Session","Lock",lock,SLOT(lock()));
                bus.connect("org.freedesktop.login1",reply.value().path(),"org.freedesktop.DBus.Properties","PropertiesChanged",lock,SLOT(properties(QString,QVariantMap,QStringList)));
            });
        }
        auto session=QDBusConnection::sessionBus();
        session.connect("org.freedesktop.ScreenSaver","/org/freedesktop/ScreenSaver","org.freedesktop.ScreenSaver","ActiveChanged",lock,SLOT(active(bool)));
        session.connect("org.gnome.ScreenSaver","/org/gnome/ScreenSaver","org.gnome.ScreenSaver","ActiveChanged",lock,SLOT(active(bool)));
        auto *watcher=new QDBusServiceWatcher(service,QDBusConnection::systemBus(),QDBusServiceWatcher::WatchForOwnerChange,this);
        connect(watcher,&QDBusServiceWatcher::serviceOwnerChanged,this,[this](const QString &,const QString &oldOwner,const QString &owner) {
            if(!oldOwner.isEmpty() && owner!=oldOwner) { cancel(); availability(false,tr("Fingerprint service changed. Refresh biometric support or use your vault passphrase.")); }
        });
    }
    ~LinuxBiometricAuth() override { cancelNative(); }
    QString name() const override { return tr("Fingerprint"); }
    void refresh() override {
        if(busy() || m_checking || m_claimPending || m_releasing) return;
        if(!QDBusConnection::systemBus().isConnected()) { unavailable(); return; }
        m_checking=true;
        auto *watcher=new QDBusPendingCallWatcher(call("/net/reactivated/Fprint/Manager","net.reactivated.Fprint.Manager","GetDefaultDevice"),this);
        connect(watcher,&QDBusPendingCallWatcher::finished,this,[this,watcher] {
            QDBusPendingReply<QDBusObjectPath> reply=*watcher; watcher->deleteLater();
            if(reply.isError()) { unavailable(); return; }
            m_device=reply.value().path();
            auto *enrolled=new QDBusPendingCallWatcher(call(m_device,deviceInterface,"ListEnrolledFingers",{QString()}),this);
            connect(enrolled,&QDBusPendingCallWatcher::finished,this,[this,enrolled] {
                QDBusPendingReply<QStringList> result=*enrolled; enrolled->deleteLater(); m_checking=false;
                const bool supported=!result.isError() && !result.value().isEmpty();
                availability(supported,supported ? tr("Fingerprint verification is available through fprintd.")
                    : tr("No enrolled fingerprint is available. Configure fprintd and enroll a finger, or use your vault passphrase."));
            });
        });
    }
protected:
    void startNative(const QString &,quint64 token) override {
        if(m_device.isEmpty() || m_claimPending || m_releasing || m_checking) {
            complete(token,false,tr("The fingerprint reader is busy. Use your vault passphrase or try again shortly.")); return;
        }
        m_claimPending=true;
        auto *watcher=new QDBusPendingCallWatcher(call(m_device,deviceInterface,"Claim",{QString()}),this);
        connect(watcher,&QDBusPendingCallWatcher::finished,this,[this,watcher,token] {
            QDBusPendingReply<> reply=*watcher; watcher->deleteLater(); m_claimPending=false;
            if(!current(token)) { if(!reply.isError()) releaseDevice(); return; }
            if(reply.isError()) { complete(token,false,tr("The fingerprint reader is busy or permission was denied. Use your vault passphrase.")); return; }
            m_claimed=true;
            m_signals=new FingerprintSignals(this);
            m_signals->callback=[this,token](const QString &result,bool done) { verifyStatus(token,result,done); };
            if(!QDBusConnection::systemBus().connect(service,m_device,deviceInterface,"VerifyStatus",m_signals,SLOT(status(QString,bool)))) {
                cancelNative(); complete(token,false,tr("Could not listen to the fingerprint reader.")); return;
            }
            auto *started=new QDBusPendingCallWatcher(call(m_device,deviceInterface,"VerifyStart",{QStringLiteral("any")}),this);
            connect(started,&QDBusPendingCallWatcher::finished,this,[this,started,token] {
                QDBusPendingReply<> result=*started; started->deleteLater();
                if(!current(token)) return;
                if(result.isError()) { cancelNative(); complete(token,false,tr("Fingerprint verification could not start. Use your vault passphrase.")); }
                else message(tr("Touch your fingerprint reader to verify."));
            });
        });
    }
    void cancelNative() override {
        if(m_signals) {
            QDBusConnection::systemBus().disconnect(service,m_device,deviceInterface,"VerifyStatus",m_signals,SLOT(status(QString,bool)));
            m_signals->deleteLater(); m_signals=nullptr;
        }
        if(m_claimed) { m_claimed=false; releaseDevice(); }
    }
private:
    void releaseDevice() {
        m_releasing=true;
        call(m_device,deviceInterface,"VerifyStop");
        auto *watcher=new QDBusPendingCallWatcher(call(m_device,deviceInterface,"Release"),this);
        connect(watcher,&QDBusPendingCallWatcher::finished,this,[this,watcher] { watcher->deleteLater(); m_releasing=false; });
    }
    void verifyStatus(quint64 token,const QString &result,bool done) {
        if(!current(token) || !m_claimed) return;
        if(result=="verify-match") { cancelNative(); complete(token,true); }
        else if(done || result=="verify-no-match") {
            cancelNative(); complete(token,false,tr("Fingerprint did not verify. Use your vault passphrase or try again."));
        } else message(tr("Adjust your finger and scan again."));
    }
    void unavailable() { m_checking=false; availability(false,tr("Fingerprint support is unavailable. Install/configure fprintd and enroll a finger, or use your vault passphrase.")); }
    QString m_device;
    FingerprintSignals *m_signals=nullptr;
    bool m_claimed=false,m_checking=false,m_claimPending=false,m_releasing=false;
};
BiometricAuth *createBiometricAuth(QObject *parent) { return new LinuxBiometricAuth(parent); }
#include "biometricauth_linux.moc"
