#include "secretstore_mac.h"
#include <QPointer>
#import <Foundation/Foundation.h>
#import <Security/Security.h>

namespace {
dispatch_queue_t queue() {
    static dispatch_queue_t value=dispatch_queue_create("com.arbiter.credential-store", DISPATCH_QUEUE_SERIAL);
    return value;
}
NSDictionary *query(const QString &service,const QString &key) {
    return @{(__bridge id)kSecClass:(__bridge id)kSecClassGenericPassword,
             (__bridge id)kSecAttrService:service.toNSString(),
             (__bridge id)kSecAttrAccount:key.toNSString()};
}
void completed(QPointer<SecretStore> owner,SecretStore::Callback callback,const QString &error,const QByteArray &data={}) {
    const QString message=error;
    const QByteArray result=data;
    dispatch_async(dispatch_get_main_queue(), ^{ if(owner) callback(message,result); });
}
// Legacy Keychain UI permission is process-wide, not an individual query flag.
// Serialize our credential-store operations and restore the previous value
// before allowing another interactive job. Never change an item's access ACL.
class NoInteraction {
public:
    explicit NoInteraction(bool silent) : m_silent(silent) {
        if(!silent) { valid=true; return; }
        valid=SecKeychainGetUserInteractionAllowed(&m_previous)==errSecSuccess &&
              SecKeychainSetUserInteractionAllowed(false)==errSecSuccess;
    }
    ~NoInteraction() { if(m_silent && valid) SecKeychainSetUserInteractionAllowed(m_previous); }
    bool valid=false;
private:
    bool m_silent; Boolean m_previous=false;
};
}
void readMacSecret(SecretStore *store,const QString &service,const QString &key,bool interactive,bool optional,SecretStore::Callback callback) {
    QPointer<SecretStore> owner(store);
    auto match=[query(service,key) mutableCopy]; match[(__bridge id)kSecReturnData]=@YES;
    dispatch_async(queue(), ^{
        if(!owner) return;
        OSStatus status=errSecInteractionNotAllowed;
        CFTypeRef result=nullptr;
        {
            NoInteraction policy(!interactive);
            if(policy.valid) {
                status=SecItemCopyMatching((__bridge CFDictionaryRef)match,&result);
            }
        }
        QByteArray value;
        if(status==errSecSuccess && result && CFGetTypeID(result)==CFDataGetTypeID()) {
            auto data=(CFDataRef)result;
            if(CFDataGetLength(data)<=1024*1024) value=QByteArray((const char *)CFDataGetBytePtr(data),CFDataGetLength(data));
            else status=errSecDecode;
        }
        if(result) CFRelease(result);
        const bool ok=status==errSecSuccess || (optional && status==errSecItemNotFound);
        const auto error=ok ? QString() : !interactive ? QObject::tr("Sync paused: macOS requires permission to read a saved credential. Use that credential in Arbiter to approve access, then try syncing again.")
            : QObject::tr("Could not read the credential from the system credential store. Unlock your keyring and try again.");
        completed(owner,callback,error,ok ? value : QByteArray());
    });
}
void writeMacSecret(SecretStore *store,const QString &service,const QString &key,const QByteArray &data,SecretStore::Callback callback) {
    QPointer<SecretStore> owner(store);
    auto match=query(service,key);
    auto value=@{(__bridge id)kSecValueData:[NSData dataWithBytes:data.constData() length:data.size()]};
    dispatch_async(queue(), ^{
        if(!owner) return;
        auto status=SecItemUpdate((__bridge CFDictionaryRef)match,(__bridge CFDictionaryRef)value);
        if(status==errSecItemNotFound) {
            auto entry=[match mutableCopy];[entry addEntriesFromDictionary:value];
            status=SecItemAdd((__bridge CFDictionaryRef)entry,nullptr);
        }
        completed(owner,callback,status==errSecSuccess ? QString() : QObject::tr("Could not save the credential securely. Unlock your system keyring, or enable a supported credential store, and try again."));
    });
}
void removeMacSecret(SecretStore *store,const QString &service,const QString &key,SecretStore::Callback callback) {
    QPointer<SecretStore> owner(store);
    auto match=query(service,key);
    dispatch_async(queue(), ^{
        if(!owner) return;
        const auto status=SecItemDelete((__bridge CFDictionaryRef)match);
        completed(owner,callback,status==errSecSuccess || status==errSecItemNotFound ? QString() : QObject::tr("Could not delete the credential from the system credential store. Try again after unlocking your keyring."));
    });
}
