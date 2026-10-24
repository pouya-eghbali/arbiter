#include "privatebrowsing.h"
#include "settings.h"
#include "passkeycrypto.h"
#include <QGuiApplication>
#include <QEvent>
#include <QFutureWatcher>
#include <QtConcurrent/QtConcurrentRun>
#include <openssl/crypto.h>
#include <algorithm>

namespace {
const QString credentialKey=QStringLiteral("Arbiter/PrivateBrowsing/Unlock/v1");
bool matches(const QByteArray &a,const QByteArray &b) {
    return a.size()==32 && b.size()==32 && CRYPTO_memcmp(a.constData(),b.constData(),32)==0;
}
}
PrivateBrowsing::PrivateBrowsing(QObject *parent) : PrivateBrowsing(new KeychainSecretStore(nullptr,QStringLiteral("Arbiter Private Browsing")),parent) {
    m_store->setParent(this);
}
PrivateBrowsing::PrivateBrowsing(SecretStore *store,QObject *parent) : QObject(parent),m_store(store) {
    m_timeout=qBound(1,Settings().value("Privacy/lockTimeout",300).toInt(),86400);
}
PrivateBrowsing::~PrivateBrowsing() { PasskeyCrypto::wipe(m_salt); PasskeyCrypto::wipe(m_verifier); }
void PrivateBrowsing::load() {
    if(m_ready || m_busy) return;
    m_busy=true; m_status.clear(); emit changed();
    QPointer<PrivateBrowsing> self(this);
    m_store->readOptional(credentialKey,[self](const QString &error,const QByteArray &data) {
        if(!self) return;
        self->m_busy=false;
        if(!error.isEmpty()) { self->m_status=error; emit self->changed(); return; }
        if(!data.isEmpty()) {
            QCborParserError parse;
            const auto map=data.size()<=1024 ? QCborValue::fromCbor(data,&parse).toMap() : QCborMap();
            if(parse.error!=QCborError::NoError || map.value(0).toInteger()!=1
                    || map.value(1).toByteArray().size()!=32 || map.value(2).toByteArray().size()!=32) {
                self->m_status=tr("The saved private-browsing credential could not be read. Unlock or repair your system credential store and retry.");
                emit self->changed(); return;
            }
            self->m_salt=map.value(1).toByteArray(); self->m_verifier=map.value(2).toByteArray();
        }
        self->m_ready=true; emit self->changed();
    });
}
bool PrivateBrowsing::retryAllowed() {
    if(m_retry.isValid() && m_retry.elapsed()<2000) return false;
    m_retry.start(); return true;
}
void PrivateBrowsing::verify(const QString &password,Callback callback) {
    if(!m_ready || !configured() || m_busy) { callback(false,tr("Wait for the private-browsing credential store and try again.")); return; }
    if(!retryAllowed()) { callback(false,tr("Wait a moment before trying again.")); return; }
    m_busy=true; m_status.clear(); emit changed();
    auto *watcher=new QFutureWatcher<QByteArray>(this);
    connect(watcher,&QFutureWatcher<QByteArray>::finished,this,[this,watcher,callback] {
        auto verifier=watcher->result(); watcher->deleteLater();
        const bool ok=matches(verifier,m_verifier); PasskeyCrypto::wipe(verifier);
        m_busy=false; emit changed();
        callback(ok,ok ? QString() : tr("The private-browsing PIN or password did not match."));
    });
    watcher->setFuture(QtConcurrent::run([bytes=password.toUtf8(),salt=m_salt]() mutable {
        auto result=PasskeyCrypto::derive(bytes,salt); PasskeyCrypto::wipe(bytes); return result;
    }));
}
void PrivateBrowsing::setPassword(const QString &current,const QString &password,const QString &confirmation) {
    savePassword(current,password,confirmation,{});
}
void PrivateBrowsing::setup(const QString &password,const QString &confirmation,Callback callback) {
    if(configured()) { callback(false,tr("Private browsing is already configured. Unlock with your existing PIN or password.")); return; }
    savePassword({},password,confirmation,std::move(callback));
}
void PrivateBrowsing::savePassword(const QString &current,const QString &password,const QString &confirmation,Callback callback) {
    const auto fail=[this,callback](const QString &error) { m_status=error; emit changed(); if(callback) callback(false,error); };
    if(!m_ready || m_busy) { fail(tr("Wait for the private-browsing credential store and try again.")); return; }
    const bool pin=password.size()>=6 && std::all_of(password.begin(),password.end(),[](QChar c) { return c>='0' && c<='9'; });
    if(password!=confirmation || (!pin && password.size()<8) || password.toUtf8().size()>1024) {
        fail(tr("Choose a PIN of at least 6 digits or a password of at least 8 characters, and enter it twice.")); return;
    }
    const bool replacing=configured();
    if(replacing && !retryAllowed()) { fail(tr("Wait a moment before trying again.")); return; }
    const auto salt=PasskeyCrypto::random(32);
    if(salt.size()!=32) { fail(tr("Could not create a secure credential. Try again.")); return; }
    m_busy=true; m_status.clear(); emit changed();
    auto *watcher=new QFutureWatcher<QByteArray>(this);
    connect(watcher,&QFutureWatcher<QByteArray>::finished,this,[this,watcher,replacing,salt,callback] {
        auto verifier=watcher->result(); watcher->deleteLater();
        if(verifier.isEmpty()) {
            m_busy=false; m_status=tr("The current PIN or password did not match, or secure derivation failed.");
            emit changed(); if(callback) callback(false,m_status); return;
        }
        const auto data=QCborValue(QCborMap{{0,1},{1,salt},{2,verifier}}).toCbor();
        QPointer<PrivateBrowsing> self(this);
        m_store->write(credentialKey,data,[self,salt,verifier,replacing,callback](const QString &error,const QByteArray &) mutable {
            if(!self) { PasskeyCrypto::wipe(verifier); return; }
            self->m_busy=false;
            if(error.isEmpty()) {
                PasskeyCrypto::wipe(self->m_salt); PasskeyCrypto::wipe(self->m_verifier);
                self->m_salt=salt; self->m_verifier=verifier;
                self->m_status=tr("Private-browsing PIN/password saved for all private windows.");
                if(replacing) emit self->credentialChanged();
            } else self->m_status=error;
            PasskeyCrypto::wipe(verifier); emit self->changed();
            if(callback) callback(error.isEmpty(),error);
        });
        PasskeyCrypto::wipe(verifier);
    });
    watcher->setFuture(QtConcurrent::run([old=current.toUtf8(),bytes=password.toUtf8(),oldSalt=m_salt,expected=m_verifier,salt,replacing]() mutable {
        auto oldVerifier=replacing ? PasskeyCrypto::derive(old,oldSalt) : QByteArray();
        const bool ok=!replacing || matches(oldVerifier,expected);
        PasskeyCrypto::wipe(old); PasskeyCrypto::wipe(oldVerifier); PasskeyCrypto::wipe(expected);
        auto result=ok ? PasskeyCrypto::derive(bytes,salt) : QByteArray();
        PasskeyCrypto::wipe(bytes); return result;
    }));
}
void PrivateBrowsing::setTimeoutSeconds(int seconds) {
    seconds=qBound(1,seconds,86400);
    if(seconds==m_timeout) return;
    m_timeout=seconds; Settings().setValue("Privacy/lockTimeout",seconds); emit changed();
}
PrivateSession::PrivateSession(PrivateBrowsing *settings,BiometricAuth *auth,QObject *parent)
    : QObject(parent),m_settings(settings),m_auth(auth) {
    m_idle.start(); m_timer.setInterval(250);
    connect(&m_timer,&QTimer::timeout,this,&PrivateSession::checkIdle);
    connect(settings,&PrivateBrowsing::changed,this,&PrivateSession::checkIdle);
    connect(settings,&PrivateBrowsing::changed,this,&PrivateSession::changed);
    connect(settings,&PrivateBrowsing::credentialChanged,this,&PrivateSession::sessionLocked);
    connect(auth,&BiometricAuth::changed,this,&PrivateSession::changed);
    connect(auth,&BiometricAuth::sessionLocked,this,&PrivateSession::sessionLocked);
    connect(qApp,&QGuiApplication::applicationStateChanged,this,[this](Qt::ApplicationState state) {
        if(state!=Qt::ApplicationActive) lock();
    });
    qApp->installEventFilter(this); m_timer.start();
}
PrivateSession::~PrivateSession() {
    ++m_generation; m_auth->cancel(); qApp->removeEventFilter(this);
}
void PrivateSession::setWindow(QWindow *window) {
    if(m_window) disconnect(m_window,nullptr,this,nullptr);
    m_window=window;
    connect(window,&QWindow::activeChanged,this,[this] { if(!m_window->isActive()) lock(); });
    connect(window,&QWindow::visibilityChanged,this,[this] { if(!m_window->isVisible() || m_window->visibility()==QWindow::Minimized) lock(); });
}
void PrivateSession::checkIdle() {
    if(!m_locked && m_idle.elapsed()>=qint64(m_settings->timeoutSeconds())*1000) lock();
}
bool PrivateSession::eventFilter(QObject *,QEvent *event) {
    switch(event->type()) {
    case QEvent::KeyPress: case QEvent::MouseButtonPress: case QEvent::MouseMove:
    case QEvent::Wheel: case QEvent::TouchBegin: case QEvent::TouchUpdate:
        if(m_window && m_window->isActive() && !m_locked) {
            // Check the old deadline before accepting the next event.
            checkIdle(); if(m_locked) return true;
            m_idle.restart();
        }
        break;
    default: break;
    }
    return false;
}
void PrivateSession::lock() {
    if(m_locked) return;
    m_locked=true; ++m_generation; m_status.clear(); emit changed();
}
void PrivateSession::sessionLocked() {
    ++m_generation; m_auth->cancel(); m_busy=false;
    m_locked=true; m_status.clear(); emit changed();
}
void PrivateSession::finishUnlock(quint64 generation) {
    if(generation!=m_generation) return;
    m_busy=false;
    if(!m_window || !m_window->isActive() || !m_window->isVisible()
            || QGuiApplication::applicationState()!=Qt::ApplicationActive) {
        m_status=tr("Return to this private window and unlock it again."); emit changed(); return;
    }
    m_locked=false; m_status.clear(); m_idle.restart(); emit changed();
}
void PrivateSession::setup(const QString &password,const QString &confirmation) {
    if(configured() || busy() || !m_window || !m_window->isActive()) return;
    const auto generation=m_generation; m_busy=true; m_status.clear(); emit changed();
    QPointer<PrivateSession> self(this);
    m_settings->setup(password,confirmation,[self,generation](bool ok,const QString &error) {
        if(!self || generation!=self->m_generation) return;
        if(ok) self->finishUnlock(generation);
        else { self->m_busy=false; self->m_status=error; emit self->changed(); }
    });
}
void PrivateSession::unlockPassword(const QString &password) {
    if(!configured() || !m_locked || busy() || !m_window || !m_window->isActive()) return;
    const auto generation=m_generation; m_busy=true; m_status.clear(); emit changed();
    QPointer<PrivateSession> self(this);
    m_settings->verify(password,[self,generation](bool ok,const QString &error) {
        if(!self || generation!=self->m_generation) return;
        if(ok) self->finishUnlock(generation);
        else { self->m_busy=false; self->m_status=error; emit self->changed(); }
    });
}
void PrivateSession::unlockBiometric() {
    if(!configured() || !m_locked || busy() || !m_window || !m_window->isActive()) return;
    m_busy=true; m_status.clear(); const auto generation=m_generation; emit changed();
    QPointer<PrivateSession> self(this);
    m_auth->authenticate(tr("Unlock your private browsing window"),[self,generation](bool ok,const QString &) {
        if(!self || generation!=self->m_generation) return;
        if(ok) {
            // The native sheet can report success just before window focus returns.
            QTimer::singleShot(150,self,[self,generation] { if(self) self->finishUnlock(generation); });
        } else { self->m_busy=false; self->m_status=tr("Verification did not complete. Try again or use your private-browsing PIN or password."); emit self->changed(); }
    });
}
