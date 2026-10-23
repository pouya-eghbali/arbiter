#include "../src/biometricauth.h"
#include <QApplication>
#include <QtTest>
#include <memory>

class BiometricAuthTest : public QObject {
    Q_OBJECT
private slots:
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
