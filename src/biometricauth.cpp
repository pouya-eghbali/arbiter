#include "biometricauth.h"
#include <utility>
#include <QGuiApplication>

BiometricAuth::BiometricAuth(QObject *parent) : QObject(parent) {
    m_timeout.setSingleShot(true); m_timeout.setInterval(45000);
    connect(&m_timeout,&QTimer::timeout,this,&BiometricAuth::cancel);
    m_focusWait.setInterval(25);
    connect(&m_focusWait,&QTimer::timeout,this,[this] {
        if(!current(m_focusToken)) { m_focusWait.stop(); return; }
        if(QGuiApplication::applicationState()==Qt::ApplicationActive) {
            m_focusWait.stop(); complete(m_focusToken,true);
        } else if(m_focusDeadline.hasExpired()) {
            m_focusWait.stop(); complete(m_focusToken,false,tr("Return to Arbiter and use your vault passphrase or try again."));
        }
    });
}
void BiometricAuth::availability(bool available, const QString &status) {
    m_available=available; m_availableStatus=status; m_status=status; emit changed();
}
void BiometricAuth::message(const QString &status) { m_status=status; emit changed(); }
void BiometricAuth::authenticate(const QString &reason, Callback callback) {
    if(foregroundRequired() && QGuiApplication::applicationState()!=Qt::ApplicationActive) {
        callback(false,tr("Focus the browser window and try again.")); return;
    }
    if (!available() || busy()) { callback(false,tr("Biometric verification is unavailable. Use your vault passphrase.")); return; }
    m_callback=std::move(callback); ++m_token; m_timeout.start(); emit changed();
    startNative(reason,m_token);
}
void BiometricAuth::complete(quint64 token, bool success, const QString &error) {
    if(!current(token)) return;
    // Hello/Touch ID may deliver their result just before focus returns from
    // the native dialog. Never release authorization to a background window.
    if(success && foregroundRequired() && QGuiApplication::applicationState()!=Qt::ApplicationActive) {
        if(!m_focusWait.isActive()) { m_focusToken=token; m_focusDeadline=QDeadlineTimer(1000); m_focusWait.start(); }
        return;
    }
    m_timeout.stop(); m_focusWait.stop();
    auto callback=std::exchange(m_callback,{});
    ++m_token; m_status=m_availableStatus; emit changed();
    callback(success,error);
}
void BiometricAuth::cancel() {
    if(!busy()) return;
    const auto token=m_token;
    cancelNative();
    complete(token,false,tr("Biometric verification was cancelled or timed out. Use your vault passphrase or try again."));
}
