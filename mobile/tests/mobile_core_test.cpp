#include "mobilemodel.h"
#include "mobileplatform.h"
#include "mobiletabs.h"
#include "mobilevault.h"
#include <QFile>
#include <QJsonDocument>
#include <QProcess>
#include <QTemporaryDir>
#include <QUrlQuery>
#include <QtTest>
namespace TestPlatform {
extern QString directory;
extern bool approved;
extern int verifications;
} // namespace TestPlatform
class MobileCoreTest : public QObject {
    Q_OBJECT
  private slots:
    void init() {
        TestPlatform::approved = false;
        TestPlatform::verifications = 0;
    }
    void stablePages() {
        MobileTabs model;
        QJsonArray a{QJsonObject{{"id", "one"}, {"title", "Old"}}, QJsonObject{{"id", "two"}}};
        model.synchronize(a);
        auto first = model.data(model.index(0), Qt::UserRole).value<MobileTab *>();
        QSignalSpy removed(&model, &QAbstractItemModel::rowsRemoved);
        a[0] = QJsonObject{{"id", "one"}, {"title", "New"}};
        model.synchronize(a);
        QCOMPARE(first, model.data(model.index(0), Qt::UserRole).value<MobileTab *>());
        QCOMPARE(first->record().value("title").toString(), "New");
        a.removeAt(1);
        model.synchronize(a);
        QCOMPARE(removed.count(), 1);
        QCOMPARE(first, model.data(model.index(0), Qt::UserRole).value<MobileTab *>());
    }
    void privateSessionNeverPersists() {
        QTemporaryDir dir;
        MobileModel model(nullptr, dir.path());
        model.navigate("https://normal.fixture.example");
        model.togglePrivate();
        model.navigate("https://private.fixture.example");
        model.persist();
        QFile file(dir.path() + "/browser-session-v1.json");
        QVERIFY(file.open(QIODevice::ReadOnly));
        auto bytes = file.readAll();
        QVERIFY(bytes.contains("normal.fixture.example"));
        QVERIFY(!bytes.contains("private.fixture.example"));
    }
    void addressSafety() {
        QTemporaryDir dir;
        MobileModel model(nullptr, dir.path());
        QCOMPARE(model.addressUrl("example.com").scheme(), "https");
        QVERIFY(model.addressUrl("javascript:alert(1)").host().contains("duckduckgo"));
        QVERIFY(model.addressUrl("https://user:password@example.com").host().contains("duckduckgo"));
        QCOMPARE(model.addressUrl("localhost:8080/path").scheme(), "http");
    }
    void sleepProtectsPages() {
        QTemporaryDir dir;
        MobileModel model(nullptr, dir.path());
        const auto eligible = model.addTab("https://eligible.example");
        const auto audible = model.addTab("https://audible.example");
        const auto edited = model.addTab("https://edited.example");
        const auto active = model.addTab("https://active.example");
        for (int i = 0; i < model.m_tabs.size(); ++i) {
            auto t = model.m_tabs[i].toObject();
            t["lastUsed"] = 0;
            if (t.value("id") == audible)
                t["audio"] = true;
            if (t.value("id") == edited)
                t["dirty"] = true;
            model.m_tabs[i] = t;
        }
        model.sleepIdle();
        QVERIFY(model.page(eligible).value("sleeping").toBool());
        QVERIFY(!model.page(audible).value("sleeping").toBool());
        QVERIFY(!model.page(edited).value("sleeping").toBool());
        QVERIFY(!model.page(active).value("sleeping").toBool());
    }
    void authenticationAndEncryption() {
        QTemporaryDir dir;
        TestPlatform::directory = dir.path();
        MobileVault vault;
        vault.savePassword("https://fixture.example", "fixture-user", "fixture-secret");
        QVERIFY(!QFile::exists(dir.path() + "/credentials-v1.enc"));
        QVERIFY(!vault.unlocked());
        TestPlatform::approved = true;
        vault.savePassword("https://fixture.example", "fixture-user", "fixture-secret");
        QVERIFY(vault.unlocked());
        QCOMPARE(vault.entries().size(), 1);
        QFile file(dir.path() + "/credentials-v1.enc");
        QVERIFY(file.open(QIODevice::ReadOnly));
        auto bytes = file.readAll();
        QVERIFY(!bytes.contains("fixture-secret"));
        QVERIFY(!vault.entries().first().toMap().contains("password"));
        const int verified = TestPlatform::verifications;
        vault.savePassword("https://fixture.example", "fixture-user", "updated-fixture");
        QCOMPARE(TestPlatform::verifications, verified);
        vault.setCooldown(0);
        vault.savePassword("https://fixture.example", "fixture-user", "updated-again");
        QCOMPARE(TestPlatform::verifications, verified + 1);
        vault.lock();
        QVERIFY(vault.entries().isEmpty());
    }
    void malformedSettings() {
        QTemporaryDir dir;
        MobileModel model(nullptr, dir.path());
        model.setSetting("privateLockSeconds", -1);
        QCOMPARE(model.settings().value("privateLockSeconds").toInt(), 60);
        model.setSetting("tabSleepSeconds", 2147483647);
        QCOMPARE(model.settings().value("tabSleepSeconds").toInt(), 600);
        model.setSetting("adblockEnabled", "false");
        QVERIFY(model.settings().value("adblockEnabled").toBool());
    }
    void conflictsRemainPrivateAndResolvable() {
        QTemporaryDir dir;
        TestPlatform::directory = dir.path();
        TestPlatform::approved = true;
        ArbiterSync::State first, second;
        QString error;
        const auto id = ArbiterSync::passwordId("https://fixture.example", "user");
        QJsonObject a{{"kind", "password"},
                      {"origin", "https://fixture.example"},
                      {"username", "user"},
                      {"password", "first-secret"}},
            b = a;
        b["password"] = "second-secret";
        QVERIFY(first.update(id, "device-first", a, false, &error));
        QVERIFY(second.update(id, "device-second", b, false, &error));
        QVERIFY(first.merge(second, &error));
        QFile file(dir.path() + "/credentials-v1.enc");
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(MobilePlatform::seal(
            QJsonDocument(QJsonObject{{"device", "phone"}, {"state", first.json()}}).toJson(),
            QByteArray(32, 'x'), "ArbiterMobileVault/v1"));
        file.close();
        MobileVault vault;
        vault.unlock();
        QCOMPARE(vault.conflicts(), 1);
        QCOMPARE(vault.conflictChoices().size(), 2);
        const auto choice = vault.conflictChoices().first().toMap();
        QVERIFY(!choice.contains("password"));
        QVERIFY(!choice.contains("privateKey"));
        vault.resolveConflict(id, choice.value("version").toString());
        QCOMPARE(vault.conflicts(), 0);
        QCOMPARE(vault.entries().size(), 1);
    }
    void pairingValidation() {
        QJsonObject payload{{"v", 1},
                            {"endpoint", "https://192.168.1.10:8443"},
                            {"pin", QString(64, 'a')},
                            {"key", QString::fromLatin1(QByteArray(32, 'k').toBase64(
                                        QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals))},
                            {"code", QString::fromLatin1(QByteArray(32, 'c').toBase64(
                                         QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals))},
                            {"vaultId", "fixture-vault-identifier"}};
        auto invitation = [&] {
            QUrl url("arbiter-pair://v1");
            QUrlQuery query;
            query.addQueryItem("data", QString::fromLatin1(QJsonDocument(payload)
                                                               .toJson(QJsonDocument::Compact)
                                                               .toBase64(QByteArray::Base64UrlEncoding |
                                                                         QByteArray::OmitTrailingEquals)));
            url.setQuery(query);
            return url.toString();
        };
        QVERIFY(!MobileVault::pairingInfo(invitation()).isEmpty());
        payload["endpoint"] = "https://example.com";
        QVERIFY(MobileVault::pairingInfo(invitation()).isEmpty());
        QVERIFY(!MobileVault::localEndpoint(QUrl("https://192.168.1.1@evil.example")));
        QVERIFY(!MobileVault::localEndpoint(QUrl("http://127.0.0.1")));
    }
    void nativePeer() {
        const auto fixture = qEnvironmentVariable("ARBITER_TEST_DESKTOP_FIXTURE");
        if (fixture.isEmpty())
            QSKIP("Set ARBITER_TEST_DESKTOP_FIXTURE to run the isolated native TLS peer");
        QProcess peer;
        peer.start(fixture);
        QVERIFY(peer.waitForStarted());
        QVERIFY(peer.waitForReadyRead());
        const auto invitation = QString::fromUtf8(peer.readLine()).trimmed();
        QVERIFY(invitation.startsWith("arbiter-pair://"));
        QTemporaryDir dir;
        TestPlatform::directory = dir.path();
        TestPlatform::approved = true;
        MobileVault vault;
        vault.savePassword("https://phone.fixture.example", "phone-user", "phone-fixture");
        QSignalSpy review(&vault, &MobileVault::pairingReview);
        QSignalSpy complete(&vault, &MobileVault::completed);
        auto wrongPin = MobileVault::pairingInfo(invitation);
        wrongPin["pin"] = QString(64, '0');
        const auto rejected = QStringLiteral("arbiter-pair://v1?data=") +
                              QString::fromLatin1(QJsonDocument(wrongPin)
                                                      .toJson(QJsonDocument::Compact)
                                                      .toBase64(QByteArray::Base64UrlEncoding |
                                                                QByteArray::OmitTrailingEquals));
        vault.pair(rejected);
        QCOMPARE(review.count(), 1);
        vault.confirmPair();
        QTRY_VERIFY_WITH_TIMEOUT(!complete.isEmpty(), 30000);
        QVERIFY(!complete.last().first().toBool());
        QCOMPARE(vault.entries().size(), 1);
        complete.clear();
        // The wrong TLS pin must not consume the one-use desktop invitation.
        vault.pair(invitation);
        QCOMPARE(review.count(), 2);
        vault.confirmPair();
        QTRY_VERIFY_WITH_TIMEOUT(!complete.isEmpty(), 30000);
        QVERIFY2(complete.last().first().toBool(), qPrintable(vault.error()));
        QCOMPARE(vault.entries().size(), 2);
        peer.terminate();
        peer.waitForFinished();
    }
};
QTEST_MAIN(MobileCoreTest)
#include "mobile_core_test.moc"
