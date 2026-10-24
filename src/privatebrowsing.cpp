#include "privatebrowsing.h"
#include "settings.h"
#include "passkeycrypto.h"
#include <QGuiApplication>
#include <QEvent>
#include <QFutureWatcher>
#include <QtConcurrent/QtConcurrentRun>
#include <openssl/crypto.h>

PrivateBrowsing::PrivateBrowsing(QObject *parent) : QObject(parent) {
    m_timeout=qBound(1,Settings().value("Privacy/lockTimeout",300).toInt(),86400);
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
    connect(auth,&BiometricAuth::changed,this,&PrivateSession::changed);
    connect(auth,&BiometricAuth::sessionLocked,this,&PrivateSession::sessionLocked);
    connect(qApp,&QGuiApplication::applicationStateChanged,this,[this](Qt::ApplicationState state) {
        if(state!=Qt::ApplicationActive) lock();
    });
    qApp->installEventFilter(this); m_timer.start();
}
PrivateSession::~PrivateSession() {
    ++m_generation; m_auth->cancel(); qApp->removeEventFilter(this);
    PasskeyCrypto::wipe(m_salt); PasskeyCrypto::wipe(m_verifier);
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
    if(configured() || m_busy || !m_window || !m_window->isActive()) return;
    if(password!=confirmation || password.size()<8 || password.toUtf8().size()>1024) {
        m_status=tr("Use at least 8 characters and enter the same password twice."); emit changed(); return;
    }
    m_salt=PasskeyCrypto::random(32);
    if(m_salt.size()!=32) { m_status=tr("Could not create a secure session password. Try again."); emit changed(); return; }
    verifyPassword(password,true);
}
void PrivateSession::unlockPassword(const QString &password) {
    if(!configured() || !m_locked || m_busy || !m_window || !m_window->isActive()) return;
    if(m_retry.isValid() && m_retry.elapsed()<2000) { m_status=tr("Wait a moment before trying again."); emit changed(); return; }
    m_retry.start(); verifyPassword(password,false);
}
void PrivateSession::verifyPassword(const QString &password,bool setup) {
    const auto generation=m_generation; m_busy=true; m_status.clear(); emit changed();
    auto *watcher=new QFutureWatcher<QByteArray>(this);
    connect(watcher,&QFutureWatcher<QByteArray>::finished,this,[this,watcher,generation,setup] {
        auto verifier=watcher->result(); watcher->deleteLater();
        if(generation!=m_generation) { PasskeyCrypto::wipe(verifier); return; }
        if(setup && !verifier.isEmpty()) { m_verifier=verifier; PasskeyCrypto::wipe(verifier); finishUnlock(generation); return; }
        const bool ok=verifier.size()==32 && m_verifier.size()==32 && CRYPTO_memcmp(verifier.constData(),m_verifier.constData(),32)==0;
        PasskeyCrypto::wipe(verifier);
        if(ok) finishUnlock(generation);
        else { m_busy=false; m_status=tr("The private-session password did not match."); emit changed(); }
    });
    watcher->setFuture(QtConcurrent::run([bytes=password.toUtf8(),salt=m_salt]() mutable {
        auto result=PasskeyCrypto::derive(bytes,salt); PasskeyCrypto::wipe(bytes); return result;
    }));
}
void PrivateSession::unlockBiometric() {
    if(!configured() || !m_locked || m_busy || !m_window || !m_window->isActive()) return;
    m_busy=true; m_status.clear(); const auto generation=m_generation; emit changed();
    QPointer<PrivateSession> self(this);
    m_auth->authenticate(tr("Unlock your private browsing window"),[self,generation](bool ok,const QString &) {
        if(!self || generation!=self->m_generation) return;
        if(ok) {
            // The native sheet can report success just before window focus returns.
            QTimer::singleShot(150,self,[self,generation] { if(self) self->finishUnlock(generation); });
        } else { self->m_busy=false; self->m_status=tr("Verification did not complete. Try again or use your private-session password."); emit self->changed(); }
    });
}
