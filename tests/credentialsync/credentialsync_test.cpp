#include "../../src/credentialsyncstate.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QtTest>
using namespace ArbiterSync;
static QJsonObject password(const QString &value) { return {{"kind","password"},{"origin","https://example.com"},{"username","me"},{"password",value}}; }
class CredentialSyncTest : public QObject {
    Q_OBJECT
private slots:
    void nistAndTampering() {
        const QByteArray key(32,'\0'),nonce(12,'\0'),plain(16,'\0');
        const auto encrypted=seal(plain,key,{},nonce);
        QCOMPARE(encrypted.toHex(),QByteArray("000000000000000000000000cea7403d4d606b6e074ec5d3baf39d18d0d1c8a799996bf0265b98b5d48ab919"));
        bool ok=false; QCOMPARE(open(encrypted,key,{},&ok),plain); QVERIFY(ok);
        open(encrypted,key,"wrong-vault",&ok); QVERIFY(!ok);
        auto bad=encrypted; bad[12]^=1; open(bad,key,{},&ok); QVERIFY(!ok);
        open(encrypted,QByteArray(32,'1'),{},&ok); QVERIFY(!ok);
    }
    void concurrentEdits() {
        State a; QString error;
        QVERIFY(a.update("login","desktop",password("initial"),false,&error)); auto b=a;
        QVERIFY(a.update("login","desktop",password("desktop edit"),false,&error));
        QVERIFY(b.update("login","phone",password("phone edit"),false,&error));
        auto left=a,right=b; QVERIFY(left.merge(b,&error)); QVERIFY(right.merge(a,&error));
        QCOMPARE(left.json(),right.json()); QCOMPARE(left.conflicts(),1);
        QVERIFY(left.update("login","phone",password("resolved"),false,&error));
        QVERIFY(right.merge(left,&error)); QVERIFY(left.merge(right,&error)); QCOMPARE(left.json(),right.json()); QCOMPARE(left.conflicts(),0);
        QCOMPARE(left.records().value("login").toArray().first().toObject().value("data").toObject().value("password").toString(),"resolved");
    }
    void concurrentDelete() {
        State a; QString error; QVERIFY(a.update("login","desktop",password("initial"),false,&error));
        auto stale=a,b=a;
        QVERIFY(a.update("login","desktop",{},true,&error));
        QVERIFY(b.update("login","phone",password("edit"),false,&error));
        QVERIFY(a.merge(b,&error)); QCOMPARE(a.conflicts(),1);
        QVERIFY(a.update("login","phone",{},true,&error)); QVERIFY(a.merge(stale,&error));
        QCOMPARE(a.conflicts(),0); QVERIFY(a.records().value("login").toArray().first().toObject().value("deleted").toBool());
    }
    void equalVectorsAreNotAnOverwrite() {
        State a,b; QString error;
        QVERIFY(a.update("login","desktop",password("a"),false,&error)); QVERIFY(b.update("login","desktop",password("b"),false,&error));
        QVERIFY(a.merge(b,&error)); QCOMPARE(a.conflicts(),1);
    }
    void threeDevicesAndStaleConflictResolution() {
        State a; QString error;
        QVERIFY(a.update("login","a",password("initial"),false,&error));
        auto b=a,c=a,offline=a;
        QVERIFY(a.update("login","a",password("edited"),false,&error));
        QVERIFY(b.update("login","b",{},true,&error));
        QVERIFY(c.merge(a,&error)); QVERIFY(c.merge(b,&error)); QCOMPARE(c.conflicts(),1);
        const auto choice=c.conflictChoices().first().toObject();
        QVERIFY(!choice.contains("password"));
        QVERIFY(!choice.value("origin").toString().isEmpty());
        auto newer=a; QVERIFY(newer.update("login","a",password("edited again"),false,&error));
        QVERIFY(c.merge(newer,&error));
        QVERIFY(!c.resolve("login",choice.value("version").toString(),choice.value("generation").toString(),"c",&error));
        const auto reviewed=c.conflictChoices();
        for(const auto &v:reviewed) if(v.toObject().value("deleted").toBool()) {
            const auto deletion=v.toObject();
            QVERIFY(c.resolve("login",deletion.value("version").toString(),deletion.value("generation").toString(),"c",&error));
        }
        QVERIFY(a.merge(c,&error)); QVERIFY(b.merge(a,&error)); QVERIFY(b.merge(offline,&error));
        QCOMPARE(b.conflicts(),0); QVERIFY(b.records().value("login").toArray().first().toObject().value("deleted").toBool());
    }
    void identicalImportsAreNotConflicts() {
        State a,b,c; QString error;
        QVERIFY(a.update("login","a",password("same"),false,&error));
        QVERIFY(b.update("login","b",password("same"),false,&error));
        QVERIFY(c.update("login","c",password("same"),false,&error));
        a.rememberDevice("a","Computer A"); b.rememberDevice("b","Computer B"); c.rememberDevice("c","Phone");
        auto left=a,right=c;
        QVERIFY(left.merge(b,&error)); QVERIFY(left.merge(c,&error));
        QVERIFY(right.merge(b,&error)); QVERIFY(right.merge(a,&error));
        QCOMPARE(left.json(),right.json()); QCOMPARE(left.conflicts(),0);
        QCOMPARE(left.deviceNames().size(),3); QCOMPARE(left.deviceNames().value("c").toString(),"Phone");
        c.rememberDevice("c","Renamed phone"); QVERIFY(left.merge(c,&error));
        QCOMPARE(left.deviceNames().value("c").toString(),"Renamed phone");
        QCOMPARE(left.records().value("login").toArray().first().toObject().value("clock").toObject().size(),3);
    }
    void rejectsMalformedClocks() {
        State state; QString error;
        for(const auto count: {0.0,-1.0,1.5,9007199254740992.0}) {
            QJsonObject version{{"clock",QJsonObject{{"desktop",count}}},{"data",password("x")},{"deleted",false}};
            QVERIFY(!State::parse({{"v",1},{"records",QJsonObject{{"login",QJsonArray{version}}}}},&state,&error));
        }
        QVERIFY(state.records().isEmpty());
    }
};
QTEST_GUILESS_MAIN(CredentialSyncTest)
#include "credentialsync_test.moc"
