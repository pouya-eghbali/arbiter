#include "../src/passwordmanager.h"
#include <QCborMap>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include "../src/settings.h"
#include <QTemporaryDir>
#include <QUuid>
#include <utility>
#include <QtTest>

class MemoryStore : public SecretStore
{
public:
    QMap<QString, QByteArray> values;
    QString failure;
    int reads = 0;
    bool delayed = false;
    std::function<void()> pending;
    void complete(std::function<void()> operation) {
        if (delayed) pending = operation;
        else operation();
    }
    void read(const QString &key, Callback cb) override {
        ++reads;
        complete([this, key, cb] { cb(!failure.isEmpty() ? failure : values.contains(key) ? QString() : QString("Missing"), values.value(key)); });
    }
    void write(const QString &key, const QByteArray &value, Callback cb) override {
        complete([this, key, value, cb] { if (failure.isEmpty()) values[key] = value; cb(failure, {}); });
    }
    void remove(const QString &key, Callback cb) override {
        complete([this, key, cb] { if (failure.isEmpty()) values.remove(key); cb(failure, {}); });
    }
};

class PasswordManagerTest : public QObject
{
    Q_OBJECT
    QTemporaryDir settings;
private slots:
    void biometricPreferenceDefaultsOnAndControlsSessionFill() {
        QTemporaryDir dir; MemoryStore store;
        PasswordManager manager(dir.path(),"Test",&store);
        QVERIFY(manager.biometricEnabled());
        manager.save(QUrl("https://example.test"),"alice","test-secret");
        int prompts=0;
        manager.setUserVerification("Test fingerprint",true,[&](const QString &,PasswordManager::VerifyCallback cb) { ++prompts; cb(true,{}); });
        PasswordSession session; session.setProperty("manager",QVariant::fromValue(&manager));
        session.prepare(QUrl("https://example.test"),{{"origin","https://example.test"},
            {"document",QUuid::createUuid().toString(QUuid::WithoutBraces)},{"form","1"}});
        QSignalSpy filled(&session,&PasswordSession::runFillScript);
        const auto id=manager.entries().first().toMap().value("id").toString();
        session.fill(id,QUrl("https://example.test")); QCOMPARE(prompts,1); QCOMPARE(filled.size(),1);
        manager.setBiometricEnabled(false);
        QTemporaryDir other;
        PasswordManager restored(other.path(),"Test",&store); QVERIFY(!restored.biometricEnabled());
        session.fill(id,QUrl("https://example.test")); QCOMPARE(prompts,1); QCOMPARE(filled.size(),2);
        manager.setBiometricEnabled(true);
        session.fill(id,QUrl("https://example.test")); QCOMPARE(prompts,2); QCOMPARE(filled.size(),3);
    }
    void passwordSuggestionRequiresMatchingSecureDocument() {
        QTemporaryDir dir; MemoryStore store; PasswordManager manager(dir.path(),"Test",&store);
        PasswordSession session; session.setProperty("manager",QVariant::fromValue(&manager));
        QSignalSpy generated(&session,&PasswordSession::runPasswordSuggestionScript);
        QVariantMap field{{"origin","https://example.test"},{"document",QUuid::createUuid().toString(QUuid::WithoutBraces)},
            {"form","1"},{"field","1"}};
        session.suggestPassword(QUrl("https://evil.test"),field); QCOMPARE(generated.size(),0);
        session.suggestPassword(QUrl("http://example.test"),field); QCOMPARE(generated.size(),0);
        session.suggestPassword(QUrl("https://example.test"),field); QCOMPARE(generated.size(),1);
        QVERIFY(generated.first().first().toString().contains(".suggestPassword("));
        manager.setEnabled(false);
        session.suggestPassword(QUrl("https://example.test"),field); QCOMPARE(generated.size(),1);
        QCOMPARE(store.reads,0);
    }
    void verificationCooldownIsFixedAndCanBeDisabled() {
        QTemporaryDir dir; MemoryStore store; qint64 now=0;
        PasswordManager manager(dir.path(),"Test",&store,nullptr,[&]{return now;});
        manager.save(QUrl("https://example.test"),"alice","test-secret");
        const auto id=manager.entries().first().toMap().value("id").toString();
        int verifications=0;
        manager.setUserVerification("Test fingerprint",true,[&](const QString &,PasswordManager::VerifyCallback cb) { ++verifications; cb(true,{}); });
        const auto fill=[&](const QString &origin="https://example.test",bool biometric=true) {
            manager.readForFill(id,origin,[](const QString &error,const QByteArray &data) { QVERIFY(error.isEmpty()); QVERIFY(!data.isEmpty()); },biometric);
        };
        QCOMPARE(manager.verificationCooldown(),300);
        fill(); QCOMPARE(verifications,1);
        now=299999; fill(); QCOMPARE(verifications,1);
        now=300000; fill(); QCOMPARE(verifications,2); // A recent fill did not extend it.
        manager.setVerificationCooldown(0); fill(); fill(); QCOMPARE(verifications,4);
        manager.setVerificationCooldown(60); fill(); QCOMPARE(verifications,5);
        manager.clearUserVerification(); fill(); QCOMPARE(verifications,6);
        manager.setEnabled(false); manager.setEnabled(true); fill(); QCOMPARE(verifications,7);
        manager.setUserVerification("Test fingerprint",false,{});
        manager.setUserVerification("Test fingerprint",true,[&](const QString &,PasswordManager::VerifyCallback cb) { ++verifications; cb(true,{}); });
        fill(); QCOMPARE(verifications,8);
        manager.clearUserVerification(); fill("https://example.test",false); fill(); QCOMPARE(verifications,9); // OS access is not biometric verification.
        manager.readForFill(id,"https://evil.test",[](const QString &error,const QByteArray &data) { QVERIFY(!error.isEmpty()); QVERIFY(data.isEmpty()); },true);
        QCOMPARE(verifications,9);
    }
    void lockInvalidatesPendingVerificationAndRead() {
        QTemporaryDir dir; MemoryStore store; qint64 now=0;
        PasswordManager manager(dir.path(),"Test",&store,nullptr,[&]{return now;});
        manager.save(QUrl("https://example.test"),"alice","test-secret");
        const auto id=manager.entries().first().toMap().value("id").toString();
        PasswordManager::VerifyCallback approve; int prompts=0;
        manager.setUserVerification("Test fingerprint",true,[&](const QString &,PasswordManager::VerifyCallback cb) { ++prompts; approve=std::move(cb); });
        const auto fillRejected=[&] { manager.readForFill(id,"https://example.test",[](const QString &error,const QByteArray &data) { QVERIFY(!error.isEmpty()); QVERIFY(data.isEmpty()); },true); };
        fillRejected(); manager.clearUserVerification(); approve(true,{}); QCOMPARE(store.reads,0);
        fillRejected(); QCOMPARE(prompts,2); approve(false,"Cancelled"); QCOMPARE(store.reads,0);
        fillRejected(); QCOMPARE(prompts,3); store.delayed=true; approve(true,{}); QCOMPARE(store.reads,1);
        manager.clearUserVerification(); auto finish=std::exchange(store.pending,{}); QVERIFY(finish); finish(); QVERIFY(!manager.busy());
        fillRejected(); QCOMPARE(prompts,4); approve(false,"Cancelled");
    }
    void cooldownPreferencePersistsButApprovalDoesNot() {
        QTemporaryDir dir; MemoryStore store; qint64 now=0; int prompts=0;
        {
            PasswordManager manager(dir.path(),"Test",&store,nullptr,[&]{return now;});
            manager.setVerificationCooldown(900);
            manager.save(QUrl("https://example.test"),"alice","test-secret");
            manager.setUserVerification("Test fingerprint",true,[&](const QString &,PasswordManager::VerifyCallback cb) { ++prompts; cb(true,{}); });
            manager.readForFill(manager.entries().first().toMap().value("id").toString(),"https://example.test",[](const QString &error,const QByteArray &) { QVERIFY(error.isEmpty()); },true);
        }
        PasswordManager restored(dir.path(),"Test",&store,nullptr,[&]{return now;});
        QCOMPARE(restored.verificationCooldown(),900);
        restored.setUserVerification("Test fingerprint",true,[&](const QString &,PasswordManager::VerifyCallback cb) { ++prompts; cb(true,{}); });
        restored.readForFill(restored.entries().first().toMap().value("id").toString(),"https://example.test",[](const QString &error,const QByteArray &) { QVERIFY(error.isEmpty()); },true);
        QCOMPARE(prompts,2);
        restored.setVerificationCooldown(-1); QCOMPARE(restored.verificationCooldown(),0);
        restored.setVerificationCooldown(100000); QCOMPARE(restored.verificationCooldown(),3600);
    }
    void focusMetadataMustMatchCommittedOrigin() {
        QTemporaryDir dir; MemoryStore store; PasswordManager manager(dir.path(),"Test",&store);
        PasswordSession session; session.setProperty("manager",QVariant::fromValue(&manager));
        auto *capture=qobject_cast<PasswordCapture *>(session.capture()); QVERIFY(capture);
        connect(&session,&PasswordSession::focusNeedsValidation,&session,[&]{session.validateFocus(QUrl("https://example.test/login"));});
        QSignalSpy focused(&session,&PasswordSession::focused);
        QVariantMap document{{"origin","https://evil.test"},{"document",QUuid::createUuid().toString(QUuid::WithoutBraces)},
            {"form","1"},{"x",20},{"y",30},{"width",200},{"height",30}};
        capture->focus(document); QCOMPARE(focused.size(),0);
        document["origin"]="https://example.test";
        capture->focus(document); QCOMPARE(focused.size(),1);
        session.validateFocus(QUrl("https://example.test/login")); QCOMPARE(focused.size(),1);
        document["width"]=-1; capture->focus(document); QCOMPARE(focused.size(),1);
        document["width"]=200; document["document"]="invalid"; capture->focus(document); QCOMPARE(focused.size(),1);
        QCOMPARE(store.reads,0);
    }
    void biometricFillRequiresApprovalAndCurrentRequest() {
        QTemporaryDir dir; MemoryStore store; PasswordManager manager(dir.path(),"Test",&store);
        manager.save(QUrl("https://example.test"),"alice","test-secret");
        const auto id=manager.entries().first().toMap().value("id").toString();
        PasswordManager::VerifyCallback verify;
        manager.setUserVerification("Test fingerprint",true,[&](const QString &,PasswordManager::VerifyCallback cb) { verify=std::move(cb); });
        bool replied=false;
        manager.readForFill(id,"https://example.test",[&](const QString &error,const QByteArray &data) { replied=true; QVERIFY(!error.isEmpty()); QVERIFY(data.isEmpty()); },true);
        QCOMPARE(store.reads,0); QVERIFY(manager.busy());
        verify(false,"Cancelled"); QVERIFY(replied); QCOMPARE(store.reads,0); QVERIFY(!manager.busy());
        bool current=true; replied=false;
        manager.readForFill(id,"https://example.test",[&](const QString &error,const QByteArray &data) { replied=true; QVERIFY(!error.isEmpty()); QVERIFY(data.isEmpty()); },true,[&]{return current;});
        current=false; verify(true,{}); QVERIFY(replied); QCOMPARE(store.reads,0);
        replied=false;
        manager.readForFill(id,"https://example.test",[&](const QString &error,const QByteArray &data) { replied=true; QVERIFY(error.isEmpty()); QVERIFY(!data.isEmpty()); },true);
        verify(true,{}); QVERIFY(replied); QCOMPARE(store.reads,1);
        manager.readForFill(id,"https://evil.test",[](const QString &error,const QByteArray &data) { QVERIFY(!error.isEmpty()); QVERIFY(data.isEmpty()); },true);
        QCOMPARE(store.reads,1);
    }

    void initTestCase() {
        QCoreApplication::setOrganizationName("ArbiterTests");
        QCoreApplication::setApplicationName("Passwords");
        qputenv("XDG_CONFIG_HOME", settings.path().toUtf8());
        qputenv("XDG_CONFIG_DIRS", settings.path().toUtf8());
    }
    void init() { Settings().clear(); }
    void exactHttpsOrigins() {
        QCOMPARE(PasswordManager::originFor(QUrl("https://EXAMPLE.com:443/login?q=1#x")), QString("https://example.com"));
        QCOMPARE(PasswordManager::originFor(QUrl("https://example.com:8443/")), QString("https://example.com:8443"));
        QCOMPARE(PasswordManager::originFor(QUrl("https://[::1]:443/")), QString("https://[::1]"));
        for (const QString &url : {"http://example.com", "file:///tmp/login", "data:text/html,test", "https://user:pass@example.com"})
            QVERIFY(PasswordManager::originFor(QUrl(url)).isEmpty());
    }
    void saveUpdateRestoreAndDelete() {
        QTemporaryDir dir;
        MemoryStore store;
        QString id;
        {
            PasswordManager manager(dir.path(), "Default", &store);
            manager.save(QUrl("https://example.com/login"), "alice", "first-secret");
            QVERIFY(manager.error().isEmpty());
            QCOMPARE(manager.entries().size(), 1);
            id = manager.entries().first().toMap()["id"].toString();
            manager.save(QUrl("https://example.com/other"), "alice", "second-secret");
            QCOMPARE(manager.entries().size(), 1);
            QCOMPARE(store.values.size(), 1);
            QVERIFY(store.values.firstKey().startsWith("Arbiter/Passwords/v1/"));
            QFile file(dir.filePath("index.json"));
            QVERIFY(file.open(QIODevice::ReadOnly));
            const auto index = file.readAll();
            QVERIFY(!index.contains("first-secret"));
            QVERIFY(!index.contains("second-secret"));
            QCOMPARE(manager.accountsFor(QUrl("https://example.com:443/path")).size(), 1);
            QVERIFY(manager.accountsFor(QUrl("https://sub.example.com")).isEmpty());
            QVERIFY(manager.accountsFor(QUrl("https://example.com:8443")).isEmpty());
        }
        PasswordManager restored(dir.path(), "Default", &store);
        QCOMPARE(restored.entries().size(), 1);
        bool read = false;
        restored.readForFill(id, "https://example.com", [&](const QString &error, const QByteArray &data) {
            QVERIFY(error.isEmpty());
            QCOMPARE(QCborValue::fromCbor(data).toMap().value(QStringLiteral("password")).toString(), QString("second-secret"));
            read = true;
        });
        QVERIFY(read);
        restored.remove(id);
        QVERIFY(restored.entries().isEmpty());
        QVERIFY(store.values.isEmpty());
    }
    void crossOriginAndIndexTamperingDoNotReleasePasswords() {
        QTemporaryDir dir;
        MemoryStore store;
        QString id;
        {
            PasswordManager manager(dir.path(), "Default", &store);
            manager.save(QUrl("https://example.com"), "alice", "secret");
            id = manager.entries().first().toMap()["id"].toString();
            manager.readForFill(id, "https://evil.test", [](const QString &error, const QByteArray &data) {
                QVERIFY(!error.isEmpty()); QVERIFY(data.isEmpty());
            });
            QCOMPARE(store.reads, 0);
        }
        QFile file(dir.filePath("index.json"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        auto bytes = file.readAll();
        file.close();
        bytes.replace("https://example.com", "https://evil.test");
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        file.write(bytes);
        file.close();
        PasswordManager manager(dir.path(), "Default", &store);
        manager.readForFill(id, "https://evil.test", [](const QString &error, const QByteArray &data) {
            QVERIFY(!error.isEmpty()); QVERIFY(data.isEmpty());
        });
    }
    void failedWritesAndDeletesPreserveExistingCredentials() {
        QTemporaryDir dir;
        MemoryStore store;
        PasswordManager manager(dir.path(), "Default", &store);
        store.failure = "Keyring locked";
        manager.save(QUrl("https://example.com"), "alice", "secret");
        QVERIFY(!manager.error().isEmpty());
        QVERIFY(manager.entries().isEmpty());
        store.failure.clear();
        manager.save(QUrl("https://example.com"), "alice", "secret");
        const auto original = store.values;
        store.failure = "Keyring locked";
        manager.save(QUrl("https://example.com"), "alice", "replacement");
        QCOMPARE(store.values, original);
        manager.remove(manager.entries().first().toMap()["id"].toString());
        QCOMPARE(manager.entries().size(), 1);
        QCOMPARE(store.values, original);
    }
    void rejectsInsecureOversizedAndConcurrentWrites() {
        QTemporaryDir dir;
        MemoryStore store;
        PasswordManager manager(dir.path(), "Default", &store);
        manager.save(QUrl("http://example.com"), "alice", "secret");
        QVERIFY(store.values.isEmpty());
        manager.save(QUrl("https://example.com"), "alice", QString(2600, 'x'));
        QVERIFY(store.values.isEmpty());
        store.delayed = true;
        manager.save(QUrl("https://example.com"), "alice", "secret");
        QVERIFY(manager.busy());
        manager.save(QUrl("https://other.test"), "bob", "secret");
        auto complete = std::exchange(store.pending, {});
        complete();
        QCOMPARE(manager.entries().size(), 1);
        QVERIFY(!manager.busy());
    }
    void privateDisabledAndMismatchedCapturesAreIgnored() {
        QTemporaryDir dir;
        MemoryStore store;
        PasswordManager manager(dir.path(), "Default", &store);
        manager.offer(QUrl("https://example.com"), "https://evil.test", "alice", "secret", false);
        QVERIFY(!manager.pending());
        manager.offer(QUrl("https://example.com"), "https://example.com", "alice", "secret", true);
        QVERIFY(!manager.pending());
        manager.offer(QUrl("https://example.com"), "https://example.com", "alice", "secret", false);
        QVERIFY(manager.pending());
        manager.acceptPending("alice");
        QVERIFY(!manager.pending());
        QCOMPARE(manager.entries().size(), 1);
        manager.offer(QUrl("https://example.com"), "https://example.com", "alice", "secret", false);
        QVERIFY(!manager.pending()); // An unchanged login does not nag to update.
        manager.offer(QUrl("https://example.com"), "https://example.com", "alice", "new-secret", false);
        QVERIFY(manager.pending());
        manager.setEnabled(false);
        manager.offer(QUrl("https://example.com"), "https://example.com", "alice", "secret", false);
        QVERIFY(!manager.pending());
        QVERIFY(manager.accountsFor(QUrl("https://example.com")).isEmpty());
    }
    void corruptIndexAndAnotherProcessFailClosed() {
        QTemporaryDir dir;
        MemoryStore store;
        {
            PasswordManager first(dir.path(), "Default", &store);
            PasswordManager second(dir.path(), "Default", &store);
            QVERIFY(!second.error().isEmpty());
            second.save(QUrl("https://example.com"), "alice", "secret");
            QVERIFY(store.values.isEmpty());
        }
        QFile file(dir.filePath("index.json"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("not-json");
        file.close();
        PasswordManager manager(dir.path(), "Default", &store);
        QVERIFY(!manager.error().isEmpty());
        manager.save(QUrl("https://example.com"), "alice", "secret");
        QVERIFY(store.values.isEmpty());
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), QByteArray("not-json"));
    }
    void navigationCancelsAsynchronousFill() {
        QTemporaryDir dir;
        MemoryStore store;
        PasswordManager manager(dir.path(), "Default", &store);
        manager.save(QUrl("https://example.com"), "alice", "secret");
        PasswordSession session;
        QVERIFY(session.setProperty("manager", QVariant::fromValue(&manager)));
        QSignalSpy fills(&session, &PasswordSession::runFillScript);
        session.prepare(QUrl("https://example.com"), {{"origin", "https://example.com"},
            {"document", "00000000-0000-0000-0000-000000000001"}, {"form", "1"}});
        store.delayed = true;
        session.fill(manager.entries().first().toMap()["id"].toString(), QUrl("https://example.com"));
        session.invalidate();
        auto complete = std::exchange(store.pending, {});
        QVERIFY(bool(complete));
        complete();
        QCOMPARE(fills.size(), 0);
    }
    void securePasswordGenerator() {
        QTemporaryDir dir;
        MemoryStore store;
        PasswordManager manager(dir.path(), "Default", &store);
        QSet<QString> generated;
        for (int i = 0; i < 100; ++i) {
            const auto password = manager.generatePassword();
            QCOMPARE(password.size(), 24);
            QVERIFY(password.contains(QRegularExpression("[A-Z]")));
            QVERIFY(password.contains(QRegularExpression("[a-z]")));
            QVERIFY(password.contains(QRegularExpression("[0-9]")));
            generated.insert(password);
        }
        QCOMPARE(generated.size(), 100);
    }
    void systemKeychainRoundTrip() {
        if (!qEnvironmentVariableIsSet("ARBITER_TEST_KEYCHAIN"))
            QSKIP("Set ARBITER_TEST_KEYCHAIN=1 to exercise the OS store with a disposable synthetic secret.");
        KeychainSecretStore store;
        const auto key = "Arbiter/Passwords/test/" + QUuid::createUuid().toString();
        bool done = false;
        QString error;
        QEventLoop loop;
        QTimer timer;
        timer.setSingleShot(true);
        connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
        const auto wait = [&] { timer.start(15000); if (!done) loop.exec(); timer.stop(); };
        store.write(key, QByteArray("synthetic-test-secret"), [&](const QString &e, const QByteArray &) { error = e; done = true; loop.quit(); });
        wait();
        QVERIFY(done);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        done = false;
        QByteArray result;
        store.read(key, [&](const QString &e, const QByteArray &data) { error = e; result = data; done = true; loop.quit(); });
        wait();
        QVERIFY(done);
        // Always attempt cleanup, including after a read failure.
        done = false;
        QString removeError;
        store.remove(key, [&](const QString &e, const QByteArray &) { removeError = e; done = true; loop.quit(); });
        wait();
        QVERIFY(done);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(result, QByteArray("synthetic-test-secret"));
        QVERIFY2(removeError.isEmpty(), qPrintable(removeError));
    }
};
#ifdef Q_OS_MACOS
#include <QApplication>
int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    PasswordManagerTest test;
    // QtKeychain delivers Apple completions on the main dispatch queue. Run
    // tests from Cocoa's application loop, rather than before app.exec().
    QTimer::singleShot(0, &app, [&] { app.exit(QTest::qExec(&test, argc, argv)); });
    return app.exec();
}
#else
QTEST_MAIN(PasswordManagerTest)
#endif
#include "passwordmanager_test.moc"
