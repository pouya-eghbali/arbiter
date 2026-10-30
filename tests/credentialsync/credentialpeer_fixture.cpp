// Test-only peer: temporary files, in-memory secrets, and dummy credentials.
// No real browser profile or OS credential store is ever read.
#include "../../src/credentialsync.h"
#include "../../src/passwordmanager.h"
#include "../../src/passkeyvault.h"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QTextStream>
class FixtureSecrets : public SecretStore {
public:
    QHash<QString,QByteArray> items;
    void read(const QString &key,Callback cb) override { cb(items.contains(key) ? QString() : QStringLiteral("Missing fixture item"),items.value(key)); }
    void readOptional(const QString &key,Callback cb) override { cb({},items.value(key)); }
    void write(const QString &key,const QByteArray &value,Callback cb) override { items[key]=value; cb({},{}); }
    void remove(const QString &key,Callback cb) override { items.remove(key); cb({},{}); }
};
int main(int argc,char **argv) {
    QCoreApplication app(argc,argv); QTemporaryDir directory; FixtureSecrets p,k,s;
    qputenv("XDG_CONFIG_HOME",directory.path().toUtf8());
    PasswordManager passwords(directory.path()+"/passwords","Fixture",&p);
    passwords.save(QUrl("https://fixture.example"),"fixture-user","fixture-password-not-a-secret");
    PasskeyVault passkeys(&k,directory.path()+"/passkeys");
    const QString phrase=QStringLiteral("test fixture passphrase only"); passkeys.setup(phrase,phrase);
    CredentialSync sync(&passwords,&passkeys,directory.path()+"/sync",&s,"127.0.0.1");
    bool printed=false;
    QObject::connect(&sync,&CredentialSync::changed,&app,[&] {
        if(printed || sync.busy() || !sync.unlocked()) return;
        printed=true; sync.pair(); QTextStream(stdout)<<sync.pairingCode()<<Qt::endl;
    });
    sync.unlock(phrase); QTimer::singleShot(10*60*1000,&app,&QCoreApplication::quit); return app.exec();
}
