#include <QtTest>
#include <QTemporaryDir>
#include <QImage>
#include <QDir>
#include <QQmlEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlExpression>
#include <QQuickItem>
#include <QQuickWindow>
#include "../src/onboarding.h"
#include "../src/appearance.h"

class OnboardingTest : public QObject {
    Q_OBJECT
    QTemporaryDir config;
private slots:
    void initTestCase() {
        qmlRegisterSingletonType(QUrl::fromLocalFile(QStringLiteral(ARBITER_THEME_FILE)),"Arbiter",1,0,"Theme");
        QVERIFY(config.isValid());
        qputenv("XDG_CONFIG_HOME",config.path().toUtf8());
        qputenv("XDG_CONFIG_DIRS",config.path().toUtf8());
        QCoreApplication::setOrganizationName("ArbiterOnboardingTest");
        QCoreApplication::setApplicationName("Tests");
    }
    void init() { Settings settings; settings.clear(); }
    void backgroundImagesPersistIndependentlyAndKeepTheirOwnCopy() {
        QTemporaryDir originals;
        const auto light=originals.filePath("light image.png"), dark=originals.filePath("dark image.png");
        QImage picture(120,80,QImage::Format_RGB32); picture.fill(QColor("#bbddff")); QVERIFY(picture.save(light));
        picture.fill(QColor("#223355")); QVERIFY(picture.save(dark));
        Appearance appearance;
        QVERIFY(appearance.lightBackgroundImage().isEmpty()); QVERIFY(appearance.darkBackgroundImage().isEmpty());
        QVERIFY(appearance.setBackgroundImage(false,QUrl::fromLocalFile(light)).isEmpty());
        QVERIFY(appearance.setBackgroundImage(true,QUrl::fromLocalFile(dark)).isEmpty());
        const auto lightCopy=appearance.lightBackgroundImage(),darkCopy=appearance.darkBackgroundImage();
        QVERIFY(lightCopy!=QUrl::fromLocalFile(light)); QVERIFY(darkCopy!=lightCopy);
        QVERIFY(Settings().value("appearance/lightBackgroundImage").toString().startsWith("backgrounds/"));
        QVERIFY(QFile::remove(light)); QVERIFY(QFile::remove(dark));
        Appearance restored;
        QCOMPARE(restored.lightBackgroundImage(),lightCopy); QCOMPARE(restored.darkBackgroundImage(),darkCopy);
        QCOMPARE(QImage(lightCopy.toLocalFile()).pixelColor(0,0),QColor("#bbddff"));
        QCOMPARE(QImage(darkCopy.toLocalFile()).pixelColor(0,0),QColor("#223355"));
        QVERIFY(!appearance.setBackgroundImage(false,QUrl("https://example.test/photo.png")).isEmpty());
        QCOMPARE(appearance.lightBackgroundImage(),lightCopy);
        QFile invalid(originals.filePath("broken.png")); QVERIFY(invalid.open(QIODevice::WriteOnly)); invalid.write("invalid image"); invalid.close();
        QVERIFY(!appearance.setBackgroundImage(false,QUrl::fromLocalFile(invalid.fileName())).isEmpty()); QCOMPARE(appearance.lightBackgroundImage(),lightCopy);
        appearance.clearBackgroundImage(false); QVERIFY(appearance.lightBackgroundImage().isEmpty()); QCOMPARE(appearance.darkBackgroundImage(),darkCopy);
        Appearance cleared; QVERIFY(cleared.lightBackgroundImage().isEmpty()); QCOMPARE(cleared.darkBackgroundImage(),darkCopy);
    }
    void accentForegroundContrast() {
        qmlRegisterSingletonType(QUrl::fromLocalFile(QStringLiteral(ARBITER_THEME_FILE)),"ArbiterThemeTest",1,0,"Theme");
        QQmlEngine engine;
        QQmlComponent component(&engine);
        component.setData("import QtQuick\nimport ArbiterThemeTest\nQtObject {"
                          "property color foreground: Theme.accentText;"
                          "function light() { Theme.dark=false; Theme.lightAccent='#2868b8' }"
                          "function dark() { Theme.dark=true; Theme.darkAccent='#8dbbff' } }",QUrl());
        QScopedPointer<QObject> probe(component.create());
        QVERIFY2(probe,qPrintable(component.errorString()));
        QVERIFY(QMetaObject::invokeMethod(probe.data(),"light"));
        QCOMPARE(probe->property("foreground").value<QColor>(),QColor("white"));
        QVERIFY(QMetaObject::invokeMethod(probe.data(),"dark"));
        QCOMPARE(probe->property("foreground").value<QColor>(),QColor("black"));
        QVERIFY(QMetaObject::invokeMethod(probe.data(),"light"));
        QCOMPARE(probe->property("foreground").value<QColor>(),QColor("white"));
    }
    void actualPrimaryButtonUsesReadableForeground() {
        QQmlEngine engine;
        // These tests load source files with their locally registered Theme,
        // not another test executable's generated Arbiter module beside us.
        auto imports = engine.importPathList(); imports.removeAll(QCoreApplication::applicationDirPath()); engine.setImportPathList(imports);
        QQmlComponent component(&engine,QUrl::fromLocalFile(QStringLiteral(ARBITER_BUTTON_FILE)));
        QScopedPointer<QObject> button(component.create());
        QVERIFY2(button,qPrintable(component.errorString()));
        button->setProperty("primary",true);
        button->setProperty("text", "Continue");
        auto *content=button->property("contentItem").value<QQuickItem *>(); QVERIFY(content);
        QQuickItem *label=nullptr;
        for (auto *child : content->childItems()) if (child->property("text").toString()=="Continue") label=child;
        QVERIFY(label);
        const auto change=[&](const QString &script) { QQmlExpression expression(qmlContext(button.data()),button.data(),script); expression.evaluate(); return !expression.hasError(); };
        QVERIFY(change("Theme.dark=false; Theme.lightAccent='#2868b8'"));
        QCOMPARE(label->property("color").value<QColor>(),QColor("white"));
        QVERIFY(change("Theme.dark=true; Theme.darkAccent='#8dbbff'"));
        QCOMPARE(label->property("color").value<QColor>(),QColor("black"));
        QVERIFY(change("Theme.dark=false; Theme.lightAccent='#15294f'"));
        QCOMPARE(label->property("color").value<QColor>(),QColor("white"));
    }
    void browserAccessScansAfterConfirmationOnly() {
        QQmlEngine engine;
        auto imports = engine.importPathList(); imports.removeAll(QCoreApplication::applicationDirPath()); engine.setImportPathList(imports);
        QQmlComponent fake(&engine);
        fake.setData("import QtQml\nQtObject { property string selectedBrowser: 'Chrome'; property bool busy: false; property bool requestingAccess: false; property var requestedWindow; property int scans: 0; property int requests: 0; signal accessFinished(bool allowed);"
                     "function requestBrowserAccess(window) { requestedWindow=window; requests++; requestingAccess=true; busy=true }"
                     "function finish(allowed: bool) { busy=false; requestingAccess=false; if(allowed) scans++; accessFinished(allowed) } }",QUrl());
        QScopedPointer<QObject> importer(fake.create());
        QVERIFY2(importer,qPrintable(fake.errorString()));
        QQmlComponent component(&engine,QUrl::fromLocalFile(QStringLiteral(ARBITER_BROWSER_ACCESS_FILE)));
        QScopedPointer<QObject> button(component.createWithInitialProperties({{"importer",QVariant::fromValue(importer.data())}}));
        QVERIFY2(button,qPrintable(component.errorString()));
        QQuickWindow window;
        auto *item=qobject_cast<QQuickItem *>(button.data()); QVERIFY(item);
        item->setParentItem(window.contentItem());
        window.resize(500,400); window.show();
        QSignalSpy scanned(button.data(),SIGNAL(scanned()));
        QVERIFY(QMetaObject::invokeMethod(button.data(),"clicked"));
        QCOMPARE(importer->property("requestedWindow").value<QObject *>(),&window);
        QCOMPARE(importer->property("requests").toInt(),1);
        QVERIFY(!button->property("enabled").toBool());
        QCOMPARE(importer->property("scans").toInt(),0);
        QVERIFY(QMetaObject::invokeMethod(importer.data(),"finish",Q_ARG(bool,false)));
        QVERIFY(button->property("enabled").toBool());
        QCOMPARE(scanned.size(),0);
        QVERIFY(QMetaObject::invokeMethod(button.data(),"clicked"));
        QVERIFY(QMetaObject::invokeMethod(importer.data(),"finish",Q_ARG(bool,true)));
        QCOMPARE(importer->property("scans").toInt(),1);
        QCOMPARE(scanned.size(),1);
        QVERIFY(button->property("enabled").toBool());
        item->setParentItem(nullptr);
    }
    void edgeNavigationPreferencesPersistAndRejectInvalidValues() {
        Appearance appearance;
        QCOMPARE(appearance.navigationRevealMode(), QString("handle"));
        QCOMPARE(appearance.pageEdgeMode(), QString("hover"));
        appearance.setNavigationRevealMode("edge");
        appearance.setPageEdgeMode("drag");
        appearance.setNavigationPill(false);
        Appearance restored;
        QCOMPARE(restored.navigationRevealMode(), QString("edge"));
        QCOMPARE(restored.pageEdgeMode(), QString("drag"));
        QVERIFY(!restored.navigationPill());
        restored.setNavigationRevealMode("invalid");
        restored.setPageEdgeMode("invalid");
        QCOMPARE(restored.navigationRevealMode(), QString("edge"));
        QCOMPARE(restored.pageEdgeMode(), QString("drag"));
        restored.setPageEdgeMode("off");
        Appearance disabled;
        QCOMPARE(disabled.pageEdgeMode(), QString("off"));
    }
    void resumeAndPresets() {
        Appearance appearance;
        Onboarding setup(&appearance);
        QCOMPARE(setup.step(),0); QVERIFY(!setup.completed());
        QCOMPARE(setup.preset(),QString("glacier"));
        setup.back(); QCOMPARE(setup.step(),0);
        setup.next(); setup.next(); QCOMPARE(setup.step(),2);
        Onboarding resumed(&appearance); QCOMPARE(resumed.step(),2);
        setup.choosePreset("minimal");
        QVERIFY(!appearance.navigationPill()); QVERIFY(!appearance.windowDecorations());
        setup.choosePreset("invalid"); QCOMPARE(setup.preset(),QString("minimal"));
        setup.choosePreset("classic");
        Appearance restored; QVERIFY(restored.navigationPill()); QVERIFY(restored.windowDecorations());
        setup.choosePreset("glacier");
        QVERIFY(appearance.navigationPill()); QVERIFY(!appearance.windowDecorations());
        setup.back(); Onboarding backed(&appearance); QCOMPARE(backed.step(),1);
    }
    void completionAndTutorial() {
        Appearance appearance; Onboarding setup(&appearance);
        QSignalSpy finished(&setup,&Onboarding::finished);
        setup.finish(true); QVERIFY(!setup.completed()); QCOMPARE(finished.size(),0);
        for(int i=0;i<8;++i) setup.next(); QCOMPARE(setup.step(),5);
        setup.finish(true); QVERIFY(setup.completed()); QCOMPARE(finished.size(),1);
        setup.finish(false); QCOMPARE(finished.size(),1); QVERIFY(setup.tutorialPending());
        Onboarding relaunched(&appearance);
        QVERIFY(relaunched.completed()); QVERIFY(relaunched.tutorialPending());
        relaunched.dismissTutorial(); QVERIFY(!relaunched.tutorialPending());
        relaunched.replayTutorial(); QVERIFY(relaunched.tutorialPending());
        relaunched.dismissTutorial(); Onboarding dismissed(&appearance); QVERIFY(!dismissed.tutorialPending());
    }
    void skipTutorialAndAbort() {
        Appearance appearance; Onboarding setup(&appearance);
        QSignalSpy aborted(&setup,&Onboarding::aborted);
        setup.next(); setup.abort(); QCOMPARE(aborted.size(),1); QVERIFY(!setup.completed());
        for(int i=0;i<4;++i) setup.next(); setup.finish(false);
        QVERIFY(setup.completed()); QVERIFY(!setup.tutorialPending());
    }
    void unreadableSettingsDoNotCompleteSetup() {
        QFile file(Settings::configFile()); QVERIFY(file.open(QIODevice::WriteOnly)); file.write("[invalid"); file.close();
        Appearance appearance; Onboarding setup(&appearance);
        setup.next(); QCOMPARE(setup.step(),0); QVERIFY(!setup.error().isEmpty());
        setup.finish(true); QVERIFY(!setup.completed());
        QVERIFY(file.open(QIODevice::ReadOnly)); QCOMPARE(file.readAll(),QByteArray("[invalid")); file.close();
        QVERIFY(file.remove());
    }
};
QTEST_MAIN(OnboardingTest)
#include "onboarding_test.moc"
