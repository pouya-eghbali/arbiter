#include "../src/siteaudio.h"
#include "../src/settings.h"
#include <QtTest>
#include <QGuiApplication>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <cstdio>
#ifdef ARBITER_TEST_AUDIO_VIEWS
#include <QtWebEngineQuick>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#endif

#include "siteaudio_test.h"
void SiteAudioTest::init() { Settings().clear(); }
void SiteAudioTest::rememberedHostRules() {
        SiteAudio audio;
        QSignalSpy changed(&audio,&SiteAudio::changed);
        const QUrl site("https://user:secret@EXAMPLE.com:8443/a?q=private#part");
        QString host;
        QVERIFY(QMetaObject::invokeMethod(&audio,"siteHost",Q_RETURN_ARG(QString,host),Q_ARG(QUrl,site)));
        QCOMPARE(host,QString("example.com"));
        QVERIFY(audio.setMuted(site,true));
        QCOMPARE(changed.count(),1);
        QVERIFY(audio.isMuted(QUrl("http://example.com/another")));
        QVERIFY(audio.isMuted(QUrl("https://example.com./")));
        QVERIFY(!audio.isMuted(QUrl("https://sub.example.com/")));
        QVERIFY(!audio.isMuted(QUrl("https://notexample.com/")));
        SiteAudio restored; QVERIFY(restored.isMuted(site));
        QCOMPARE(Settings().value("Audio/mutedSites").toStringList(),QStringList{"example.com"});
        QVERIFY(audio.setMuted(site,true)); QCOMPARE(changed.count(),1);
        QVERIFY(audio.setMuted(site,false));
        SiteAudio unmuted; QVERIFY(!unmuted.isMuted(site));
        for(const auto *url:{"about:blank","file:///tmp/test","chrome-extension://abc/page.html","data:text/html,test"}) {
            QVERIFY(SiteAudio::siteHost(QUrl(url)).isEmpty());
            QVERIFY(!audio.setMuted(QUrl(url),true));
        }
    }
void SiteAudioTest::privateChangesStayTemporary() {
        const QUrl normal("https://example.com"),privateSite("https://private.example");
        SiteAudio normalAudio; QVERIFY(normalAudio.setMuted(normal,true));
        SiteAudio privateAudio(false);
        QVERIFY(privateAudio.isMuted(normal));
        QVERIFY(privateAudio.setMuted(normal,false));
        QVERIFY(privateAudio.setMuted(privateSite,true));
        QVERIFY(normalAudio.isMuted(normal)); QVERIFY(!normalAudio.isMuted(privateSite));
        SiteAudio restored;
        QVERIFY(restored.isMuted(normal)); QVERIFY(!restored.isMuted(privateSite));
    }
#ifdef ARBITER_TEST_AUDIO_VIEWS
void SiteAudioTest::openViewsFollowHostAndPrivacyChanges() {
        SiteAudio audio;
        QQmlApplicationEngine engine;
#ifdef ARBITER_WEBENGINE_QML_PATH
        engine.addImportPath(QString::fromUtf8(ARBITER_WEBENGINE_QML_PATH));
#endif
        engine.rootContext()->setContextProperty("siteAudio",&audio);
        engine.loadData(R"QML(
import QtQuick
import QtWebEngine
Window {
    id:owner;visible:true;width:600;height:300
    property bool covered:false
    WebEngineView {
        objectName:"first";width:300;height:300;url:"http://127.0.0.1:9/one"
        readonly property bool siteMuted:{siteAudio.revision;return siteAudio.isMuted(url)}
        audioMuted:owner.covered||siteMuted
    }
    WebEngineView {
        objectName:"second";x:300;width:300;height:300;url:"http://127.0.0.1:9/two"
        readonly property bool siteMuted:{siteAudio.revision;return siteAudio.isMuted(url)}
        audioMuted:owner.covered||siteMuted
    }
})QML");
        QVERIFY(!engine.rootObjects().isEmpty());auto *window=engine.rootObjects().first();
        auto *first=window->findChild<QObject *>("first"),*second=window->findChild<QObject *>("second");QVERIFY(first&&second);
        QVERIFY(audio.setMuted(QUrl("https://127.0.0.1/"),true));
        QTRY_VERIFY(first->property("audioMuted").toBool());QTRY_VERIFY(second->property("audioMuted").toBool());
        second->setProperty("url",QUrl("http://localhost:9/"));
        QTRY_VERIFY(!second->property("audioMuted").toBool());
        window->setProperty("covered",true);QTRY_VERIFY(second->property("audioMuted").toBool());
        window->setProperty("covered",false);
        QTRY_VERIFY(first->property("audioMuted").toBool());QTRY_VERIFY(!second->property("audioMuted").toBool());
        QVERIFY(audio.setMuted(QUrl("https://127.0.0.1/"),false));QTRY_VERIFY(!first->property("audioMuted").toBool());
    }
#endif
int main(int argc,char **argv) {
    std::fputs("Starting site audio fixture.\n",stderr);std::fflush(stderr);
#ifdef ARBITER_TEST_AUDIO_VIEWS
    QtWebEngineQuick::initialize();
#endif
    std::fputs("WebEngine initialized; creating GUI application.\n",stderr);std::fflush(stderr);
    QGuiApplication app(argc,argv);
    std::fputs("GUI application ready.\n",stderr);std::fflush(stderr);
    QStandardPaths::setTestModeEnabled(true);
    QTemporaryDir config;qputenv("XDG_CONFIG_HOME",config.path().toUtf8());Settings::initialize();
    SiteAudioTest test;return QTest::qExec(&test,argc,argv);
}
