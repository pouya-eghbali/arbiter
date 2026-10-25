#include "../src/localization.h"
#include "../src/settings.h"
#include <QTemporaryDir>
#include <QTranslator>
#include <QtTest>

class LocalizationTest : public QObject {
    Q_OBJECT
    QTemporaryDir settings;
private slots:
    void initTestCase() {
        QCoreApplication::setOrganizationName("ArbiterTests");
        QCoreApplication::setApplicationName("Localization");
        qputenv("XDG_CONFIG_HOME", settings.path().toUtf8());
        qputenv("XDG_CONFIG_DIRS", settings.path().toUtf8());
    }
    void init() { Settings().clear(); }
    void followsSystemBeforeOnboarding() {
        Localization localization;
        QCOMPARE(localization.language(), "system");
        QCOMPARE(localization.activeLanguage(), Localization::resolve("system", QLocale::system().uiLanguages()));
        QVERIFY(!localization.restartRequired());
        const auto welcome = QCoreApplication::translate("Onboarding", "Welcome to Arbiter");
        if (localization.activeLanguage() == "en") QCOMPARE(welcome, "Welcome to Arbiter");
        else QVERIFY(welcome != QString("Welcome to Arbiter"));
    }
    void languageMatchingHonorsSystemPriorityAndScript() {
        QCOMPARE(Localization::resolve("system", {"it-CH", "de-CH", "en-US"}), "it");
        QCOMPARE(Localization::resolve("system", {"fr-CA", "en-US"}), "fr");
        QCOMPARE(Localization::resolve("system", {"nl-NL", "de-CH", "en-US"}), "de");
        QCOMPARE(Localization::resolve("system", {"zh-Hans-CN", "en-US"}), "zh_CN");
        QCOMPARE(Localization::resolve("system", {"zh-Hant-TW", "en-US"}), "en");
        QCOMPARE(Localization::resolve("system", QLocale("zh_TW").uiLanguages()), "en");
        QCOMPARE(Localization::resolve("system", {"zh-Hant-TW", "zh", "de-DE"}), "de");
        QCOMPARE(Localization::resolve("system", {"zh-Hant-TW", "zh-Hans-CN"}), "zh_CN");
        QCOMPARE(Localization::resolve("system", QLocale("zh_CN").uiLanguages()), "zh_CN");
        QCOMPARE(Localization::resolve("system", {"ar-EG"}), "en");
        QCOMPARE(Localization::resolve("system", {"pt-PT", "en-US"}), "pt_BR");
        QCOMPARE(Localization::resolve("ja", {"de-DE"}), "ja");
        QCOMPARE(Localization::resolve("not-a-language", {"de-DE"}), "en");
    }
    void defaultsPersistenceAndRestartScope() {
        Settings().setValue("General/language", "en");
        {
            Localization localization;
            QCOMPARE(localization.activeLanguage(), "en"); QVERIFY(!localization.restartRequired());
            QCOMPARE(localization.languages().size(), 9);
            localization.setLanguage("de");
            QCOMPARE(localization.language(), "de"); QVERIFY(localization.restartRequired());
            QCOMPARE(localization.activeLanguage(), "en");
            QCOMPARE(Settings().value("General/language").toString(), "de");
            localization.setLanguage("invalid"); QCOMPARE(localization.language(), "de");
            localization.setLanguage("en"); QVERIFY(!localization.restartRequired());
            localization.setLanguage("fr");
        }
        Localization restored;
        QCOMPARE(restored.language(), "fr"); QCOMPARE(restored.activeLanguage(), "fr");
        QVERIFY(!restored.restartRequired());
        QVERIFY(QCoreApplication::translate("Main", "Settings") != QString("Settings"));
    }
    void eachBundledLanguageContainsQmlAndNativeStrings() {
        for (const auto &code : {"de", "fr", "it", "es", "pt_BR", "ja", "zh_CN"}) {
            QTranslator translator;
            QVERIFY2(translator.load(QString(":/i18n/arbiter_%1.qm").arg(code)), code);
            QVERIFY2(!translator.translate("Main", "Settings").isEmpty(), code);
            QVERIFY2(!translator.translate("QObject", "New Tab").isEmpty(), code);
            QVERIFY2(!translator.translate("KeyBindings", "Edit address").isEmpty(), code);
            QVERIFY2(!translator.translate("BiometricAuth", "Touch ID is available.").isEmpty(), code);
            // Qt's standard widget catalogs are merged into the same offline resource.
            QVERIFY2(!translator.translate("QPlatformTheme", "Cancel").isEmpty(), code);
        }
    }
};
QTEST_GUILESS_MAIN(LocalizationTest)
#include "localization_test.moc"
