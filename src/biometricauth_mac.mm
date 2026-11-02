#include "biometricauth.h"
#include <QPointer>
#import <LocalAuthentication/LocalAuthentication.h>
#import <AppKit/AppKit.h>
#import <Security/Security.h>
namespace {
void wipeKey(QByteArray &bytes) {
    volatile char *p = bytes.data();
    for (qsizetype i = 0; i < bytes.size(); ++i) p[i] = 0;
    bytes.clear();
}
}
#include <utility>

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
        wipeKey(m_readResult);
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
    bool persistentKeysSupported() const override {
        SecTaskRef task = SecTaskCreateFromSelf(kCFAllocatorDefault);
        if (!task) return false;
        CFTypeRef value = SecTaskCopyValueForEntitlement(task, CFSTR("com.apple.application-identifier"), nullptr);
        CFRelease(task);
        const bool supported = value && CFGetTypeID(value) == CFStringGetTypeID() && CFStringGetLength((CFStringRef)value) > 0;
        if (value) CFRelease(value);
        return supported;
    }
    void storeProtectedKey(const QString &identifier, const QByteArray &key, KeyCallback callback) override {
        if (!persistentKeysSupported() || key.size() != 32) { callback(tr("Persistent biometric unlocking is unavailable."), {}); return; }
        CFErrorRef error = nullptr;
        SecAccessControlRef access = SecAccessControlCreateWithFlags(kCFAllocatorDefault,
            kSecAttrAccessibleWhenUnlockedThisDeviceOnly, kSecAccessControlBiometryCurrentSet, &error);
        if (!access) { if (error) CFRelease(error); callback(tr("Could not protect the biometric vault key in Keychain."), {}); return; }
        auto query = keyQuery(identifier);
        query[(__bridge id)kSecAttrAccessControl] = (__bridge id)access;
        query[(__bridge id)kSecValueData] = [NSData dataWithBytes:key.constData() length:key.size()];
        const auto status = SecItemAdd((__bridge CFDictionaryRef)query, nullptr);
        CFRelease(access);
        if (status != errSecSuccess) qWarning("Protected passkey Keychain creation failed (%d)", int(status));
        callback(status == errSecSuccess ? QString() : tr("Could not protect the biometric vault key in Keychain."), {});
    }
    void removeProtectedKey(const QString &identifier, KeyCallback callback) override {
        const auto status = SecItemDelete((__bridge CFDictionaryRef)keyQuery(identifier));
        callback(status == errSecSuccess || status == errSecItemNotFound ? QString() : tr("Could not remove the biometric vault key from Keychain."), {});
    }
    void readProtectedKey(const QString &identifier, const QString &reason, KeyCallback callback) override {
        if (!persistentKeysSupported() || busy()) { callback(tr("Persistent biometric unlocking is unavailable."), {}); return; }
        m_readId = identifier;
        QPointer<MacBiometricAuth> self(this);
        authenticate(reason, [self, callback](bool success, const QString &error) {
            if (!self) return;
            self->m_readId.clear();
            auto key = std::exchange(self->m_readResult, {});
            callback(success && key.size() == 32 ? QString() : error.isEmpty() ? tr("Could not unlock the biometric vault key. Use your passphrase or enable Touch ID again.") : error,
                     success ? key : QByteArray());
            wipeKey(key);
        });
    }
private:
    static NSMutableDictionary *keyQuery(const QString &identifier) {
        return [@{(__bridge id)kSecClass: (__bridge id)kSecClassGenericPassword,
                  (__bridge id)kSecAttrService: @"Arbiter Biometric Passkey Keys",
                  (__bridge id)kSecAttrAccount: identifier.toNSString(),
                  (__bridge id)kSecUseDataProtectionKeychain: @YES,
                  (__bridge id)kSecAttrSynchronizable: @NO} mutableCopy];
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
        if (!m_readId.isEmpty()) {
            auto query = keyQuery(m_readId);
            query[(__bridge id)kSecReturnData] = @YES;
            query[(__bridge id)kSecUseAuthenticationContext] = m_context;
            m_context.localizedReason = reason.toNSString();
            dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
                CFTypeRef result = nullptr;
                const auto status = SecItemCopyMatching((__bridge CFDictionaryRef)query, &result);
                QByteArray key;
                if (status == errSecSuccess && result && CFGetTypeID(result) == CFDataGetTypeID()) {
                    auto data = (CFDataRef)result;
                    if (CFDataGetLength(data) == 32) key = QByteArray((const char *)CFDataGetBytePtr(data), 32);
                }
                if (result) CFRelease(result);
                dispatch_async(dispatch_get_main_queue(), ^{
                    auto value = key;
                    if (!self || !self->current(token)) { wipeKey(value); return; }
                    self->m_context = nil;
                    self->m_readResult = value;
                    wipeKey(value);
                    self->complete(token, status == errSecSuccess && self->m_readResult.size() == 32,
                        status == errSecSuccess ? QString() : tr("Touch ID could not unlock the vault key. Use your passphrase or enable Touch ID again."));
                });
                wipeKey(key);
            });
            return;
        }
        [m_context evaluatePolicy:LAPolicyDeviceOwnerAuthenticationWithBiometrics
            localizedReason:reason.toNSString() reply:^(BOOL success,NSError *) {
                dispatch_async(dispatch_get_main_queue(), ^{
                    if(!self || !self->current(token)) return;
                    self->m_context=nil;
                    self->complete(token,success,success ? QString() : BiometricAuth::tr("Touch ID did not verify. Use your vault passphrase or try again."));
                });
            }];
    }
    void cancelNative() override { [m_context invalidate]; m_context=nil; m_readId.clear(); wipeKey(m_readResult); }
private:
    QString m_readId;
    QByteArray m_readResult;
    LAContext *__strong m_context=nil;
    id __strong m_sleep=nil,m_session=nil,m_screen=nil;
};
BiometricAuth *createBiometricAuth(QObject *parent) { return new MacBiometricAuth(parent); }
