#include "../src/browserimporter.h"
#include "../src/chromevault.h"
#include <openssl/evp.h>
#include <atomic>
#include <memory>
#include "../src/passwordmanager.h"
#include "../src/history.h"
#include "../src/settings.h"
#include <QCborValue>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QtTest>

class ImportStore : public SecretStore {
public:
    QMap<QString,QByteArray> values;
    int writes=0,failAt=0;
    bool delay=false;
    std::function<void()> pending;
    void read(const QString &key,Callback cb) override { cb({},values.value(key)); }
    void remove(const QString &key,Callback cb) override { values.remove(key); cb({},{}); }
    void write(const QString &key,const QByteArray &data,Callback cb) override {
        ++writes;
        auto complete=[this,key,data,cb] { if(failAt==writes) cb("Test credential-store failure",{}); else { values[key]=data; cb({},{}); } };
        if(delay) pending=complete; else complete();
    }
};
class BrowserImporterTest : public QObject {
    Q_OBJECT
    QTemporaryDir settings;
    static QString write(QTemporaryDir &dir,const QString &name,const QByteArray &bytes) {
        const auto path=dir.filePath(name); QFile file(path);
        if(!file.open(QIODevice::WriteOnly) || file.write(bytes)!=bytes.size()) return {};
        return path;
    }
    static QByteArray read(const QString &path) { QFile f(path); if(!f.open(QIODevice::ReadOnly)) return {}; return f.readAll(); }
    static QString historyJson(QTemporaryDir &dir) {
        const auto time=QDateTime::currentMSecsSinceEpoch()-86400000;
        return write(dir,"history.json",QJsonDocument(QJsonObject{{"history",QJsonArray{QJsonObject{{"url","https://example.test/old"},{"title","Old visit"},{"time_usec",time*1000}}}}}).toJson());
    }
    static QByteArray encrypted(const QByteArray &plain,const QByteArray &key,ChromeVault::Cipher kind) {
        const bool gcm=kind==ChromeVault::Cipher::WindowsGcm;
        const QByteArray iv=gcm ? QByteArray(12,'n') : QByteArray(16,' ');
        std::unique_ptr<EVP_CIPHER_CTX,decltype(&EVP_CIPHER_CTX_free)> ctx(EVP_CIPHER_CTX_new(),EVP_CIPHER_CTX_free);
        QByteArray out(plain.size()+16,Qt::Uninitialized); int count=0,tail=0;
        if(!ctx || EVP_EncryptInit_ex(ctx.get(),gcm ? EVP_aes_256_gcm() : EVP_aes_128_cbc(),nullptr,reinterpret_cast<const unsigned char *>(key.constData()),reinterpret_cast<const unsigned char *>(iv.constData()))!=1
            || EVP_EncryptUpdate(ctx.get(),reinterpret_cast<unsigned char *>(out.data()),&count,reinterpret_cast<const unsigned char *>(plain.constData()),plain.size())!=1
            || EVP_EncryptFinal_ex(ctx.get(),reinterpret_cast<unsigned char *>(out.data()+count),&tail)!=1) return {};
        out.resize(count+tail);
        if(gcm) { QByteArray tag(16,Qt::Uninitialized); if(EVP_CIPHER_CTX_ctrl(ctx.get(),EVP_CTRL_GCM_GET_TAG,16,tag.data())!=1) return {}; return "v10"+iv+out+tag; }
        return (kind==ChromeVault::Cipher::LinuxCbc ? "v11" : "v10")+out;
    }
    static void seedVault(const QString &path,const QByteArray &secret,const QString &user) {
        {
            auto db=QSqlDatabase::addDatabase("QSQLITE","vault-source"); db.setDatabaseName(path); QVERIFY(db.open()); QSqlQuery q(db);
            QVERIFY(q.exec("CREATE TABLE logins(origin_url TEXT,username_value TEXT,password_value BLOB,blacklisted_by_user INTEGER)"));
            q.prepare("INSERT INTO logins VALUES('https://example.test/path',?,?,0)"); q.addBindValue(user); q.addBindValue(secret); QVERIFY(q.exec());
            QVERIFY(q.exec("INSERT INTO logins VALUES('https://blocked.test','blocked',X'00',1)"));
        }
        QSqlDatabase::removeDatabase("vault-source");
    }
private slots:
    void initTestCase() {
        QVERIFY(settings.isValid()); qputenv("XDG_CONFIG_HOME",settings.path().toUtf8()); qputenv("XDG_CONFIG_DIRS",settings.path().toUtf8());
    }
    void init() { Settings().clear(); }
    void csvFormatsAndQuotedSecrets() {
        QTemporaryDir dir;
        const auto file=write(dir,"passwords.csv",QByteArray("\xef\xbb\xbf")+"name,url,username,password,note\r\nSite,https://EXAMPLE.test/path,alice,\"p,a\"\"ss\nword\",note\r\nDuplicate,https://example.test/else,alice,replacement,ignored\r\nBad,http://insecure.test/,user,secret,ignored\r\n");
        auto result=BrowserImport::read(file,{}); QVERIFY(result.error.isEmpty()); QCOMPARE(result.passwords.size(),1); QCOMPARE(result.skippedPasswords,2);
        QCOMPARE(result.passwords[0].password,QString("p,a\"ss\nword")); QCOMPARE(result.passwords[0].url,QUrl("https://example.test"));
        for(const QByteArray &header:{QByteArray("URL,Username,Password,Title,Notes,OTPAuth\n"),QByteArray("url,username,password,httpRealm,formActionOrigin,guid,timeCreated,timeLastUsed,timePasswordChanged\n")}) {
            const bool firefox=header.startsWith("url");
            const auto rows=header+(firefox ? "https://example.test/,user,secret,,,,,,\n" : "https://example.test/,user,secret,,,\n");
            const auto path=write(dir,"other.csv",rows); result=BrowserImport::read(path,{}); QVERIFY2(result.error.isEmpty(),qPrintable(result.error)); QCOMPARE(result.passwords.size(),1);
        }
    }
    void csvMalformedAndUnsupportedRows() {
        QTemporaryDir dir;
        for(const auto &bytes:{QByteArray("url,username,password\nhttps://example.test,user,\"unterminated"),QByteArray("url,username,wrong\nhttps://example.test,user,secret\n"),QByteArray("url,username,password\nhttps://example.test,user,\xff\n")}) {
            const auto result=BrowserImport::read(write(dir,"bad.csv",bytes),{}); QVERIFY(!result.error.isEmpty()); QVERIFY(result.passwords.isEmpty()); QVERIFY(!result.error.contains("unterminated"));
        }
        auto result=BrowserImport::read(write(dir,"large-login.csv","url,username,password\nhttps://example.test,u,"+QByteArray(3000,'x')+"\n"),{});
        QVERIFY(result.error.isEmpty()); QCOMPARE(result.skippedPasswords,1); QVERIFY(result.passwords.isEmpty());
    }
    void jsonDatesDedupAndBadUrls() {
        QTemporaryDir dir;
        const auto path=historyJson(dir); auto result=BrowserImport::read({},path);
        QVERIFY(result.error.isEmpty()); QCOMPARE(result.visits.size(),1);
        const auto stamp=result.visits[0].time;
        QVERIFY(stamp.secsTo(QDateTime::currentDateTime())>=86399);
        auto doc=QJsonDocument::fromJson(read(path)).object(); auto entries=doc["history"].toArray();
        entries.append(entries[0]); entries.append(QJsonObject{{"url","javascript:alert(1)"},{"time_usec",1000}});
        entries.append(QJsonObject{{"url","https://future.test/"},{"time_usec",(QDateTime::currentMSecsSinceEpoch()+86400000)*1000}});
        const auto takeout=write(dir,"BrowserHistory.json",QJsonDocument(QJsonObject{{"Browser History",entries}}).toJson());
        result=BrowserImport::read({},takeout); QVERIFY(result.error.isEmpty()); QCOMPARE(result.visits.size(),1); QCOMPARE(result.skippedVisits,3); QCOMPARE(result.visits[0].time,stamp);
    }
    void sqliteBrowsersReadOnly_data() { QTest::addColumn<int>("kind"); QTest::newRow("Chromium")<<0; QTest::newRow("Firefox")<<1; QTest::newRow("Safari")<<2; }
    void sqliteBrowsersReadOnly() {
        QFETCH(int,kind); QTemporaryDir dir; const auto path=dir.filePath("source.sqlite");
        const auto ms=QDateTime::currentMSecsSinceEpoch()-20000;
        {
            auto db=QSqlDatabase::addDatabase("QSQLITE","source"); db.setDatabaseName(path); QVERIFY(db.open()); QSqlQuery q(db);
            if(kind==0) { QVERIFY(q.exec("CREATE TABLE urls(id INTEGER,url TEXT,title TEXT)")); QVERIFY(q.exec("CREATE TABLE visits(url INTEGER,visit_time INTEGER)")); QVERIFY(q.exec("INSERT INTO urls VALUES(1,'https://example.test/','Chromium')")); q.prepare("INSERT INTO visits VALUES(1,?)"); q.addBindValue((ms+11644473600000LL)*1000); }
            else if(kind==1) { QVERIFY(q.exec("CREATE TABLE moz_places(id INTEGER,url TEXT,title TEXT)")); QVERIFY(q.exec("CREATE TABLE moz_historyvisits(place_id INTEGER,visit_date INTEGER)")); QVERIFY(q.exec("INSERT INTO moz_places VALUES(1,'https://example.test/','Firefox')")); q.prepare("INSERT INTO moz_historyvisits VALUES(1,?)"); q.addBindValue(ms*1000); }
            else { QVERIFY(q.exec("CREATE TABLE history_items(id INTEGER,url TEXT)")); QVERIFY(q.exec("CREATE TABLE history_visits(history_item INTEGER,title TEXT,visit_time REAL)")); QVERIFY(q.exec("INSERT INTO history_items VALUES(1,'https://example.test/')")); q.prepare("INSERT INTO history_visits VALUES(1,'Safari',?)"); q.addBindValue((ms-978307200000LL)/1000.0); }
            QVERIFY(q.exec());
        }
        QSqlDatabase::removeDatabase("source"); const auto original=read(path);
        const auto result=BrowserImport::read({},path); QVERIFY2(result.error.isEmpty(),qPrintable(result.error)); QCOMPARE(result.visits.size(),1);
        QVERIFY(qAbs(result.visits[0].time.toMSecsSinceEpoch()-ms)<=1); QCOMPARE(read(path),original);
    }
    void previewThenImportKeepsExistingPasswordsAndTimestamps() {
        QTemporaryDir dir; ImportStore store; PasswordManager passwords(dir.filePath("vault"),"test",&store); History history(dir.filePath("history"));
        passwords.save(QUrl("https://example.test"),"alice","original"); QCOMPARE(store.writes,1);
        BrowserImporter importer(&passwords,&history); importer.setDirect(false);
        const auto csv=write(dir,"passwords.csv","url,username,password\nhttps://example.test,alice,REPLACE\nhttps://new.test,bob,newpass\n");
        const auto sourceHistory=historyJson(dir);
        importer.setPasswordFile(QUrl::fromLocalFile(csv)); importer.setHistoryFile(QUrl::fromLocalFile(sourceHistory)); importer.preview();
        QTRY_VERIFY(!importer.busy()); QVERIFY(importer.ready()); QCOMPARE(importer.passwordCount(),1); QCOMPARE(importer.skippedPasswords(),1); QCOMPARE(history.totalCount(),0); QCOMPARE(store.writes,1);
        importer.apply(); QTRY_VERIFY(!importer.busy()); QVERIFY(importer.error().isEmpty()); QCOMPARE(store.writes,2); QCOMPARE(passwords.entries().size(),2); QCOMPARE(history.totalCount(),1);
        bool original=false; for(const auto &bytes:store.values) if(QCborValue::fromCbor(bytes).toMap().value("password").toString()=="original") original=true; QVERIFY(original);
        const auto stamp=history.data(history.index(0),History::VisitedAt).toDateTime(); QVERIFY(stamp.daysTo(QDateTime::currentDateTime())>=0); QVERIFY(stamp.secsTo(QDateTime::currentDateTime())>=86399);
        importer.preview(); QTRY_VERIFY(!importer.busy()); QCOMPARE(importer.passwordCount(),0); importer.apply(); QTRY_VERIFY(!importer.busy()); QCOMPARE(store.writes,2); QCOMPARE(history.totalCount(),1);
    }
    void chromiumKnownAnswerAndAuthenticatedCipher() {
        // Independent known-answer vector from Chromium's os_crypt_mac_unittest.
        const auto key=ChromeVault::deriveCbcKey("mock_password",1003);
        const auto data=QByteArray::fromHex("763130bf086d2056861a80de825fc935868630644f2ca187450213ae6681b4d643d19b2581c85c8878c1bc97e726a10e51ea77");
        const auto decoded=ChromeVault::decrypt(data,key,ChromeVault::Cipher::MacCbc); QVERIFY(decoded.has_value());
        QByteArray plain; for(int i=0;i<32;++i) plain.append(char(i)); QCOMPARE(*decoded,plain);
        const auto gcmKey=QByteArray(32,'k'); auto gcm=encrypted("a protected password",gcmKey,ChromeVault::Cipher::WindowsGcm);
        auto result=ChromeVault::decrypt(gcm,gcmKey,ChromeVault::Cipher::WindowsGcm); QVERIFY(result); QCOMPARE(*result,QByteArray("a protected password"));
        gcm[gcm.size()-1]=gcm.back()^1; QVERIFY(!ChromeVault::decrypt(gcm,gcmKey,ChromeVault::Cipher::WindowsGcm));
        const auto linuxKey=ChromeVault::deriveCbcKey("system-keyring-password",1);
        result=ChromeVault::decrypt(encrypted("Linux protected",linuxKey,ChromeVault::Cipher::LinuxCbc),linuxKey,ChromeVault::Cipher::LinuxCbc);
        QVERIFY(result); QCOMPARE(*result,QByteArray("Linux protected"));
        QVERIFY(!ChromeVault::decrypt("v20blocked",key,ChromeVault::Cipher::MacCbc));
    }
    void protectedBrowserVaultAutoImportAndDenial() {
        QTemporaryDir source,target; ImportStore store;
        const auto key=ChromeVault::deriveCbcKey("synthetic-keychain-password",1003);
        const auto encryptedPassword=encrypted("synthetic-password",key,ChromeVault::Cipher::MacCbc);
        seedVault(source.filePath("Login Data"),encryptedPassword,"alice");
        seedVault(source.filePath("Login Data For Account"),encryptedPassword,"bob");
        const auto original=read(source.filePath("Login Data"));
        PasswordManager passwords(target.filePath("vault"),"test",&store); History history(target.filePath("history"));
        const QVariantMap profile{{"label","Chrome — fixture"},{"directory",source.path()},{"passwordSupport",true},{"file",QUrl::fromLocalFile(historyJson(source))}};
        const auto profiles=[profile] { return QVariantList{QVariantMap{},profile}; };
        BrowserImporter importer(&passwords,&history,nullptr,profiles,[key](const QVariantMap &source) { return ChromeVault::readWithKey(source,key,ChromeVault::Cipher::MacCbc); });
        importer.refreshProfiles(); importer.selectProfile(1); importer.importFromBrowser();
        QTRY_VERIFY_WITH_TIMEOUT(!importer.message().isEmpty() || !importer.error().isEmpty(),5000);
        QVERIFY2(importer.error().isEmpty(),qPrintable(importer.error())); QVERIFY(!importer.busy()); QCOMPARE(passwords.entries().size(),2); QCOMPARE(history.totalCount(),1); QCOMPARE(store.writes,2);
        QCOMPARE(read(source.filePath("Login Data")),original);
        BrowserImporter denied(&passwords,&history,nullptr,profiles,[](const QVariantMap &) { ImportPreview result; result.error="System authorization denied"; return result; });
        denied.refreshProfiles(); denied.selectProfile(1); denied.importFromBrowser(); QTRY_VERIFY(!denied.busy()); QVERIFY(!denied.error().isEmpty()); QCOMPARE(store.writes,2);
        const auto blocked=source.filePath("blocked"); QDir().mkpath(blocked); seedVault(blocked+"/Login Data","v20protected","user");
        auto unsupported=ChromeVault::readWithKey(QVariantMap{{"directory",blocked}},key,ChromeVault::Cipher::MacCbc); QVERIFY(!unsupported.error.isEmpty()); QVERIFY(unsupported.passwords.isEmpty());
    }
    void profilesWithOnlyOneDataType() {
        QTemporaryDir dir; ImportStore store; PasswordManager passwords(dir.filePath("vault"),"test",&store); History history(dir.filePath("history"));
        const auto file=historyJson(dir);
        const auto profiles=[file] { return QVariantList{QVariantMap{},QVariantMap{{"directory","synthetic-passwords"},{"passwordSupport",true}},QVariantMap{{"directory","synthetic-history"},{"passwordSupport",false},{"file",QUrl::fromLocalFile(file)}}}; };
        BrowserImporter importer(&passwords,&history,nullptr,profiles,[](const QVariantMap &) { ImportPreview out; out.passwords.append({QUrl("https://only-password.test/"),"u","secret"}); return out; });
        importer.refreshProfiles(); importer.selectProfile(1);
        QVERIFY(importer.importPasswords()); QVERIFY(!importer.importHistory()); importer.importFromBrowser();
        QTRY_VERIFY(importer.message().startsWith("Imported")); QCOMPARE(passwords.entries().size(),1); QCOMPARE(history.totalCount(),0);
        importer.selectProfile(2); QVERIFY(!importer.importPasswords()); QVERIFY(importer.importHistory()); importer.importFromBrowser();
        QTRY_VERIFY(importer.message().startsWith("Imported")); QCOMPARE(store.writes,1); QCOMPARE(history.totalCount(),1);
        importer.setImportHistory(false); importer.selectProfile(1); importer.selectProfile(2); QVERIFY(!importer.importHistory());
    }
    void cancellationAndPartialFailure() {
        QTemporaryDir dir; ImportStore store; PasswordManager passwords(dir.filePath("vault"),"test",&store); History history(dir.filePath("history")); BrowserImporter importer(&passwords,&history); importer.setDirect(false);
        const auto csv=write(dir,"passwords.csv","url,username,password\nhttps://one.test,u,one\nhttps://two.test,u,two\n");
        importer.setPasswordFile(QUrl::fromLocalFile(csv)); importer.preview(); importer.cancel(); QTest::qWait(100); QVERIFY(!importer.ready()); QCOMPARE(store.writes,0);
        importer.preview(); QTRY_VERIFY(!importer.busy()); store.delay=true; importer.apply(); QTRY_VERIFY(bool(store.pending));
        importer.cancel(); auto complete=std::move(store.pending); complete(); QTRY_VERIFY(!importer.busy()); QCOMPARE(passwords.entries().size(),1); QCOMPARE(store.writes,1);
        importer.preview(); QTRY_VERIFY(!importer.busy()); store.delay=false; store.failAt=2; importer.apply(); QTRY_VERIFY(!importer.busy()); QVERIFY(!importer.error().isEmpty()); QCOMPARE(passwords.entries().size(),1); QCOMPARE(store.values.size(),1);
    }
};
QTEST_GUILESS_MAIN(BrowserImporterTest)
#include "browserimporter_test.moc"
