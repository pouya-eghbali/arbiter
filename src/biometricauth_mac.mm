#include "biometricauth.h"
#include <QPointer>
#import <LocalAuthentication/LocalAuthentication.h>
#import <AppKit/AppKit.h>

class MacBiometricAuth final : public BiometricAuth {
public:
    explicit MacBiometricAuth(QObject *parent) : BiometricAuth(parent) {
        QPointer<MacBiometricAuth> self(this);
        auto lock=^(NSNotification *) { if(self) emit self->sessionLocked(); };
        auto *center=NSWorkspace.sharedWorkspace.notificationCenter;
        m_sleep=[center addObserverForName:NSWorkspaceWillSleepNotification object:nil queue:NSOperationQueue.mainQueue usingBlock:lock];
        m_session=[center addObserverForName:NSWorkspaceSessionDidResignActiveNotification object:nil queue:NSOperationQueue.mainQueue usingBlock:lock];
        m_screen=[NSDistributedNotificationCenter.defaultCenter addObserverForName:@"com.apple.screenIsLocked" object:nil queue:NSOperationQueue.mainQueue usingBlock:lock];
    }
    ~MacBiometricAuth() override {
        [m_context invalidate];
        [NSWorkspace.sharedWorkspace.notificationCenter removeObserver:m_sleep];
        [NSWorkspace.sharedWorkspace.notificationCenter removeObserver:m_session];
        [NSDistributedNotificationCenter.defaultCenter removeObserver:m_screen];
    }
    QString name() const override { return BiometricAuth::tr("Touch ID"); }
    bool nativePromptCanTakeFocus() const override { return true; }
    void refresh() override {
        if(busy()) return;
        LAContext *context=[LAContext new];
        NSError *error=nil;
        const bool supported=[context canEvaluatePolicy:LAPolicyDeviceOwnerAuthenticationWithBiometrics error:&error];
        availability(supported,supported ? BiometricAuth::tr("Touch ID is available.") : BiometricAuth::tr("Touch ID is unavailable. Enroll a fingerprint in macOS settings, or use your vault passphrase."));
    }
protected:
    void startNative(const QString &reason, quint64 token) override {
        m_context=[LAContext new];
        m_context.localizedFallbackTitle=@"";
        m_context.touchIDAuthenticationAllowableReuseDuration=0;
        NSError *error=nil;
        if(![m_context canEvaluatePolicy:LAPolicyDeviceOwnerAuthenticationWithBiometrics error:&error]) {
            m_context=nil;
            complete(token,false,BiometricAuth::tr("Touch ID is unavailable or locked out. Use your vault passphrase.")); return;
        }
        QPointer<MacBiometricAuth> self(this);
        [m_context evaluatePolicy:LAPolicyDeviceOwnerAuthenticationWithBiometrics
            localizedReason:reason.toNSString() reply:^(BOOL success,NSError *) {
                dispatch_async(dispatch_get_main_queue(), ^{
                    if(!self || !self->current(token)) return;
                    self->m_context=nil;
                    self->complete(token,success,success ? QString() : BiometricAuth::tr("Touch ID did not verify. Use your vault passphrase or try again."));
                });
            }];
    }
    void cancelNative() override { [m_context invalidate]; m_context=nil; }
private:
    LAContext *__strong m_context=nil;
    id __strong m_sleep=nil,m_session=nil,m_screen=nil;
};
BiometricAuth *createBiometricAuth(QObject *parent) { return new MacBiometricAuth(parent); }
