#include "../src/biometricauth.h"
#include <QApplication>
#include <QtTest>
#include <memory>
#include <QUuid>
#include <QScopeGuard>
#ifdef Q_OS_MACOS
#include <Security/Security.h>
#endif

class BiometricAuthTest : public QObject {
    Q_OBJECT
private slots:
    void protectedKeyRequiresNativeAuthorization() {
#ifndef Q_OS_MACOS
        QSKIP("macOS Data Protection Keychain probe");
#else
        if (!qEnvironmentVariableIsSet("ARBITER_PROTECTED_KEY_PROBE")) QSKIP("Opt-in signed native probe; no user credentials are read");
        std::unique_ptr<BiometricAuth> auth(createBiometricAuth(nullptr)); auth->refresh();
        QVERIFY(auth->available()); QVERIFY(auth->persistentKeysSupported());
        const auto identifier=QStringLiteral("arbiter-test/")+QUuid::createUuid().toString();
        auto cleanup=qScopeGuard([&] { auth->removeProtectedKey(identifier,[](const QString &,const QByteArray &){}); });
        QString error; bool done=false;
        auth->storeProtectedKey(identifier,QByteArray(32,'t'),[&](const QString &value,const QByteArray &){error=value; done=true;});
        QVERIFY(done); QVERIFY2(error.isEmpty(),qPrintable(error));
        auto account=CFStringCreateWithCString(nullptr,identifier.toUtf8().constData(),kCFStringEncodingUTF8);
        const void *keys[]={kSecClass,kSecAttrService,kSecAttrAccount,kSecUseDataProtectionKeychain,kSecReturnData,kSecUseAuthenticationUI};
        const void *values[]={kSecClassGenericPassword,CFSTR("Arbiter Biometric Passkey Keys"),account,kCFBooleanTrue,kCFBooleanTrue,kSecUseAuthenticationUIFail};
        auto query=CFDictionaryCreate(nullptr,keys,values,6,&kCFTypeDictionaryKeyCallBacks,&kCFTypeDictionaryValueCallBacks);
        CFTypeRef result=nullptr; auto status=SecItemCopyMatching(query,&result);
        if(result) CFRelease(result); CFRelease(query); CFRelease(account);
        QCOMPARE(status,errSecInteractionNotAllowed);
        qInfo("OS Keychain refused to release the test key without native authentication");
#endif
    }
    void nativeAvailabilityDoesNotPrompt() {
        std::unique_ptr<BiometricAuth> auth(createBiometricAuth(nullptr));
        QVERIFY(!auth->name().isEmpty());
        QSignalSpy changed(auth.get(),&BiometricAuth::changed);
        auth->refresh();
        QTRY_VERIFY_WITH_TIMEOUT(!changed.isEmpty(),15000);
        QVERIFY(!auth->status().isEmpty()); QVERIFY(!auth->busy());
        qInfo().noquote()<<auth->name()<<":"<<auth->status();
        auth->cancel(); QVERIFY(!auth->busy());
    }
};
QTEST_MAIN(BiometricAuthTest)
#include "biometricauth_test.moc"
