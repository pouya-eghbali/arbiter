#include "../src/settings.h"
#include <QTemporaryDir>
#include <QtTest>

class SettingsTest : public QObject {
    Q_OBJECT
    QTemporaryDir directory;
    int run=0;
    QString user,system1,system2;
    static void write(const QString &path,const QByteArray &contents) {
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile file(path); QVERIFY(file.open(QIODevice::WriteOnly)); QCOMPARE(file.write(contents),contents.size());
    }
    static QByteArray read(const QString &path) { QFile file(path); if(!file.open(QIODevice::ReadOnly)) return {}; return file.readAll(); }
private slots:
    void initTestCase() {
        QVERIFY(directory.isValid());
        QCoreApplication::setOrganizationName("ArbiterSettingsTests");
        QCoreApplication::setApplicationName("Settings");
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,directory.path()+"/legacy");
    }
    void init() {
        const auto base=directory.path()+"/"+QString::number(++run);
        user=base+"/user"; system1=base+"/first"; system2=base+"/second";
        qputenv("XDG_CONFIG_HOME",user.toUtf8());
        qputenv("XDG_CONFIG_DIRS",(system1+QDir::listSeparator()+"relative"+QDir::listSeparator()+system2).toUtf8());
        QSettings().clear();
    }
    void stringLists() {
        Settings settings;
        const QStringList hosts{"example.com","quotes\".example","bücher.example"};
        settings.setValue("Audio/mutedSites",hosts);
        QCOMPARE(settings.status(),QSettings::NoError);
        QCOMPARE(Settings().value("Audio/mutedSites").toStringList(),hosts);
        settings.setValue("Audio/mutedSites",QStringList{});
        QCOMPARE(Settings().value("Audio/mutedSites").toStringList(),QStringList{});
    }
    void xdgPaths() {
        QCOMPARE(Settings::configFile(),user+"/arbiter/settings.toml");
        QCOMPARE(Settings::systemFiles(),QStringList({system1+"/arbiter/settings.toml",system2+"/arbiter/settings.toml"}));
        for(const auto &value:{QByteArray(),QByteArray("relative")}) {
            qputenv("XDG_CONFIG_HOME",value);
            QCOMPARE(Settings::configFile(),QDir::homePath()+"/.config/arbiter/settings.toml");
        }
        qputenv("XDG_CONFIG_DIRS",""); QCOMPARE(Settings::systemFiles(),QStringList({"/etc/xdg/arbiter/settings.toml"}));
    }
    void precedenceAndUserOnlyWrites() {
        const auto first=system1+"/arbiter/settings.toml",second=system2+"/arbiter/settings.toml";
        write(first,"[appearance]\ntheme=\"dark\"\nlightAccent=\"#123456\"\n");
        write(second,"[appearance]\ntheme=\"light\"\nsiteMenuMode=\"off\"\n");
        const auto firstBefore=read(first),secondBefore=read(second);
        Settings preferences;
        QCOMPARE(preferences.value("appearance/theme").toString(),QString("dark"));
        QCOMPARE(preferences.value("appearance/siteMenuMode").toString(),QString("off"));
        QCOMPARE(preferences.value("missing",42).toInt(),42);
        preferences.setValue("appearance/theme","system");
        Settings reloaded;
        QCOMPARE(reloaded.value("appearance/theme").toString(),QString("system"));
        QCOMPARE(reloaded.value("appearance/lightAccent").toString(),QString("#123456"));
        QCOMPARE(read(first),firstBefore); QCOMPARE(read(second),secondBefore);
        Settings raw;
        QVERIFY(!raw.contains("appearance/lightAccent"));
    }
    void migrationImportsOnlyPreferencesOnce() {
        QSettings legacy;
        legacy.setValue("appearance/theme","dark"); legacy.setValue("KeyboardBindings/address","Ctrl+K");
        legacy.setValue("History/enabled",false); legacy.setValue("Passwords/enabled",false);
        legacy.setValue("secret/password","not-a-preference"); legacy.sync();
        QVERIFY(Settings::initialize().isEmpty());
        Settings preferences;
        QCOMPARE(preferences.value("appearance/theme").toString(),QString("dark"));
        QCOMPARE(preferences.value("KeyboardBindings/address").toString(),QString("Ctrl+K"));
        QVERIFY(!preferences.value("History/enabled",true).toBool());
        QVERIFY(!preferences.value("Passwords/enabled",true).toBool());
        QVERIFY(!preferences.contains("secret/password"));
        QCOMPARE(legacy.value("appearance/theme").toString(),QString("dark"));
        preferences.setValue("appearance/theme","light");
        QVERIFY(Settings::initialize().isEmpty());
        Settings again; QCOMPARE(again.value("appearance/theme").toString(),QString("light"));
    }
    void suppliedDotfileWinsAndSurvivesRestart() {
        QSettings().setValue("appearance/theme","dark");
        write(Settings::configFile(),"[appearance]\ntheme=\"light\"\nlightAccent=\"#aabbcc\"\n[custom]\nkeep=\"yes\"\n");
        QVERIFY(Settings::initialize().isEmpty());
        { Settings preferences; QCOMPARE(preferences.value("appearance/theme").toString(),QString("light")); preferences.setValue("History/enabled",false); }
        Settings restored;
        QCOMPARE(restored.value("appearance/lightAccent").toString(),QString("#aabbcc"));
        QCOMPARE(restored.value("custom/keep").toString(),QString("yes"));
        QVERIFY(!restored.value("History/enabled",true).toBool());
    }
    void exampleIsValidToml() {
        const auto example=QFINDTESTDATA("../docs/settings.example.toml");
        QVERIFY(!example.isEmpty());
        write(Settings::configFile(),read(example));
        Settings preferences;
        QCOMPARE(preferences.status(),QSettings::NoError);
        QCOMPARE(preferences.value("appearance/theme").toString(),QString("system"));
        QCOMPARE(preferences.value("appearance/lightAccent").toString(),QString("#2868b8"));
        QCOMPARE(preferences.value("appearance/navigationPill").metaType().id(),int(QMetaType::Bool));
    }
    void iniFileIsIgnored() {
        const auto ini=user+"/arbiter/settings.ini";
        const QByteArray contents("[appearance]\ntheme=dark\n");
        write(ini,contents);
        QVERIFY(Settings::initialize().isEmpty());
        Settings preferences;
        QCOMPARE(preferences.value("appearance/theme","system").toString(),QString("system"));
        QCOMPARE(read(ini),contents);
    }
    void fullTomlAndUnknownValuesRoundTrip() {
        write(Settings::configFile(),R"TOML(# a dotfile with standard TOML syntax
appearance.theme = 'dark'
[custom]
message = """first line
second line"""
items = [1, 2, 3]
inline = { enabled = true, label = 'hello' }
date = 2026-10-24
[KeyboardBindings]
settings = 'Ctrl+,'
)TOML");
        Settings preferences;
        QCOMPARE(preferences.value("appearance/theme").toString(),QString("dark"));
        QCOMPARE(preferences.value("custom/message").toString(),QString("first line\nsecond line"));
        QCOMPARE(preferences.value("custom/items").toList().size(),3);
        QCOMPARE(preferences.value("custom/inline").toMap().value("label").toString(),QString("hello"));
        QCOMPARE(preferences.value("KeyboardBindings/settings").toString(),QString("Ctrl+,"));
        preferences.setValue("appearance/theme","light");
        Settings restored;
        QCOMPARE(restored.status(),QSettings::NoError);
        QCOMPARE(restored.value("custom/items"),preferences.value("custom/items"));
        QVERIFY(read(Settings::configFile()).contains("2026-10-24"));
    }
    void independentWritersMergeAndExternalDamageIsPreserved() {
        Settings first,second;
        first.setValue("appearance/theme","dark");
        second.setValue("History/enabled",false);
        Settings merged;
        QCOMPARE(merged.value("appearance/theme").toString(),QString("dark"));
        QVERIFY(!merged.value("History/enabled",true).toBool());
        const QByteArray damaged("[appearance\n");
        write(Settings::configFile(),damaged);
        first.setValue("appearance/theme","light");
        QCOMPARE(first.status(),QSettings::FormatError);
        QCOMPARE(read(Settings::configFile()),damaged);
    }
    void malformedFileIsPreserved() {
        const QByteArray broken("[appearance\ntheme=\"dark\"\n");
        write(Settings::configFile(),broken);
        QVERIFY(!Settings::initialize().isEmpty());
        { Settings preferences; QCOMPARE(preferences.value("appearance/theme","system").toString(),QString("system")); preferences.setValue("appearance/theme","light"); }
        QCOMPARE(read(Settings::configFile()),broken);
    }
    void symlinkedDotfileRemainsLinked() {
#ifdef Q_OS_WIN
        QSKIP("Creating Unix-style symlinks requires special Windows privileges.");
#else
        const auto target=directory.path()+"/dotfiles/settings.toml";
        write(target,"[appearance]\ntheme=\"dark\"\n");
        QDir().mkpath(QFileInfo(Settings::configFile()).absolutePath());
        QVERIFY(QFile::link(target,Settings::configFile()));
        QVERIFY(Settings::initialize().isEmpty());
        { Settings preferences; preferences.setValue("appearance/theme","light"); }
        QVERIFY(QFileInfo(Settings::configFile()).isSymbolicLink());
        QVERIFY(read(target).contains("light"));
#endif
    }
};
QTEST_GUILESS_MAIN(SettingsTest)
#include "settings_test.moc"
