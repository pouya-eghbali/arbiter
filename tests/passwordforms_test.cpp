#include "../src/passwordmanager.h"
#include <QApplication>
#include <QEventLoop>
#include <QTemporaryDir>
#include <QWebChannel>
#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QWebEngineScript>
#include <QWebEngineScriptCollection>
#include <QWebEngineView>
#include <QtTest>

class PasswordFormsTest : public QObject
{
    Q_OBJECT
    QWebEngineView *view = nullptr;
    PasswordCapture *capture = nullptr;
    QWebChannel *channel = nullptr;
    QTemporaryDir directory;
    PasswordManager *manager = nullptr;
    QList<QStringList> captures;
    QList<QVariantMap> focuses;

    QVariant js(const QString &script, int world = 1) {
        QVariant result;
        QEventLoop loop;
        QTimer timer;
        timer.setSingleShot(true);
        connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
        view->page()->runJavaScript(script, world, [&](const QVariant &value) { result = value; loop.quit(); });
        timer.start(5000);
        loop.exec();
        return result;
    }
    void load(const QString &body) {
        captures.clear();
        QSignalSpy loaded(view, &QWebEngineView::loadFinished);
        view->setHtml("<!doctype html><meta charset=utf-8>" + body, QUrl("https://example.test/login"));
        QVERIFY(loaded.wait(15000));
        QVERIFY(loaded.last().first().toBool());
        QTRY_VERIFY_WITH_TIMEOUT(js("typeof globalThis.__arbiterPasswords").toString() == "object", 5000);
        // Wait for the isolated channel to finish initialization before capture.
        QTest::qWait(150);
    }
private slots:
    void initTestCase() {
        manager = new PasswordManager(directory.path(), "Test", nullptr, this);
        view = new QWebEngineView;
        view->resize(900, 600);
        view->show();
        channel = new QWebChannel(view);
        capture = new PasswordCapture(view);
        capture->receiver = [this](const QString &origin, const QString &username, const QString &password) {
            captures.append({origin, username, password});
        };
        capture->focusReceiver=[this](const QVariantMap &document) { focuses.append(document); };
        channel->registerObject("passwordCapture", capture);
        view->page()->setWebChannel(channel, 1);
        QWebEngineScript script;
        script.setName("Password forms test");
        script.setWorldId(QWebEngineScript::ApplicationWorld);
        script.setInjectionPoint(QWebEngineScript::DocumentReady);
        script.setRunsOnSubFrames(false);
        QVERIFY(!manager->captureScript().isEmpty());
        script.setSourceCode(manager->captureScript());
        view->page()->scripts().insert(script);
    }
    void cleanupTestCase() { delete view; }
    void suggestedPasswordFillsNewAndConfirmationThenIsCapturedOnSubmit() {
        load("<form onsubmit='event.preventDefault()'><input autocomplete=username value=alice>"
             "<input type=password autocomplete=current-password value=old>"
             "<input type=password autocomplete='section-signup new-password'>"
             "<input type=password autocomplete='section-signup new-password'><button>Register</button></form>");
        const auto target=js("(()=>{const r=document.querySelectorAll('[type=password]')[1].getBoundingClientRect();return __arbiterPasswords.inspectPasswordAt(r.x+2,r.y+2)})()").toMap();
        QVERIFY(!target.isEmpty());
        const auto script=PasswordManager::suggestionScript(target["origin"].toString(),target["document"].toString(),
            target["form"].toString(),target["field"].toString(),"Generated'p\\assword!123");
        QVERIFY(js(script).toBool());
        const auto values=js("Array.from(document.querySelectorAll('input')).map(i=>i.value)",0).toStringList();
        QCOMPARE(values,QStringList({"alice","old","Generated'p\\assword!123","Generated'p\\assword!123"}));
        QVERIFY(captures.isEmpty()); // Generation neither submits nor saves.
        js("document.querySelector('form').requestSubmit()",0);
        QTRY_COMPARE(captures.size(),1);
        QCOMPARE(captures.first().last(),QString("Generated'p\\assword!123"));
        QCOMPARE(js("typeof __arbiterPasswords",0).toString(),QString("undefined"));
    }
    void passwordSuggestionRejectsOtherFieldsChangedFormsAndOldDocuments() {
        load("<form><input autocomplete=username><input type=password></form>");
        QVERIFY(js("(()=>{const r=document.querySelector('input').getBoundingClientRect();return __arbiterPasswords.inspectPasswordAt(r.x+2,r.y+2)})()").isNull());
        const auto target=js("(()=>{const r=document.querySelector('[type=password]').getBoundingClientRect();return __arbiterPasswords.inspectPasswordAt(r.x+2,r.y+2)})()").toMap();
        QVERIFY(!target.isEmpty());
        const auto script=PasswordManager::suggestionScript(target["origin"].toString(),target["document"].toString(),
            target["form"].toString(),target["field"].toString(),"GeneratedPassword123!");
        js("document.querySelector('form').action='https://evil.test/'",0);
        QVERIFY(!js(script).toBool());
        QCOMPARE(js("document.querySelector('[type=password]').value",0).toString(),QString());
        load("<form><input type=password></form>");
        js("(()=>{const r=document.querySelector('input').getBoundingClientRect();return __arbiterPasswords.inspectPasswordAt(r.x+2,r.y+2)})()");
        QVERIFY(!js(script).toBool());
        QCOMPARE(js("document.querySelector('[type=password]').value",0).toString(),QString());
    }
    void syntheticPointerEventsDoNotOfferAutofill() {
        load("<form><input autocomplete=username><input type=password autocomplete=current-password></form>");
        focuses.clear();
        js("document.querySelector('input').dispatchEvent(new PointerEvent('pointerdown',{bubbles:true}))",0);
        js("document.querySelector('input').focus()",0);
        QTest::qWait(100);
        QVERIFY(focuses.isEmpty());
    }
    void fillAndCaptureWithoutSubmitting() {
        load("<form><input name=user autocomplete=username><input type=password autocomplete=current-password><button>Log in</button></form>");
        const auto document = js("__arbiterPasswords.inspect()").toMap();
        QCOMPARE(document.value("origin").toString(), QString("https://example.test"));
        QVERIFY(!document.value("form").toString().isEmpty());
        const auto script = PasswordManager::fillScript("https://example.test", document["document"].toString(),
            document["form"].toString(), "alice", "p'ass\\word\u2028");
        QVERIFY(js(script).toBool());
        QCOMPARE(js("document.querySelector('input').value", 0).toString(), QString("alice"));
        QCOMPARE(js("document.querySelector('input[type=password]').value", 0).toString(), QString("p'ass\\word\u2028"));
        QVERIFY(js("__arbiterPasswords.capture()").toBool());
        QTRY_COMPARE(captures.size(), 1);
        QCOMPARE(captures.first().at(0), QString("https://example.test"));
        QCOMPARE(captures.first().at(1), QString("alice"));
        QCOMPARE(captures.first().at(2), QString("p'ass\\word\u2028"));
        QCOMPARE(js("typeof globalThis.__arbiterPasswords", 0).toString(), QString("undefined"));
        QCOMPARE(js("typeof qt", 0).toString(), QString("undefined"));
    }
    void wrongOriginDocumentAndChangedActionDoNotFill() {
        load("<form><input autocomplete=username><input type=password></form>");
        const auto document = js("__arbiterPasswords.inspect()").toMap();
        QVERIFY(!js(PasswordManager::fillScript("https://evil.test", document["document"].toString(), document["form"].toString(), "alice", "secret")).toBool());
        QVERIFY(!js(PasswordManager::fillScript("https://example.test", "old-document", document["form"].toString(), "alice", "secret")).toBool());
        js("document.querySelector('form').action='https://evil.test/'", 0);
        QVERIFY(!js(PasswordManager::fillScript("https://example.test", document["document"].toString(), document["form"].toString(), "alice", "secret")).toBool());
        QCOMPARE(js("document.querySelector('[type=password]').value", 0).toString(), QString());
        QVERIFY(!js("__arbiterPasswords.capture()").toBool());
    }
    void submittedFormsOfferToSave() {
        load("<form onsubmit='event.preventDefault()'><input autocomplete=username value=alice><input type=password value=secret><button>Sign in</button></form>");
        js("document.querySelector('form').requestSubmit()", 0);
        QTRY_COMPARE(captures.size(), 1);
        QCOMPARE(captures.first(), QStringList({"https://example.test", "alice", "secret"}));
    }
    void dynamicFormsUsernameFirstAndNewPasswordCapture() {
        load("<div id=container></div>");
        js("document.querySelector('#container').innerHTML='<form><input autocomplete=username></form>'", 0);
        auto document = js("__arbiterPasswords.inspect()").toMap();
        QVERIFY(js(PasswordManager::fillScript("https://example.test", document["document"].toString(), document["form"].toString(), "alice", "secret")).toBool());
        QCOMPARE(js("document.querySelector('input').value", 0).toString(), QString("alice"));
        load("<form><input autocomplete=username value=alice><input type=password autocomplete=current-password value=old><input type=password autocomplete=new-password value=new><input type=password autocomplete=new-password value=new></form>");
        QVERIFY(js("__arbiterPasswords.capture()").toBool());
        QTRY_COMPARE(captures.size(), 1);
        QCOMPARE(captures.first().at(2), QString("new"));
    }
    void hiddenAmbiguousAndNewPasswordFormsAreNotFilled() {
        load("<form><input type=password style='display:none'></form>");
        QVERIFY(js("__arbiterPasswords.inspect()").isNull());
        load("<form><input type=password></form><form><input type=password></form>");
        QVERIFY(js("__arbiterPasswords.inspect()").isNull());
        load("<form><input type=password autocomplete=new-password></form>");
        QVERIFY(js("__arbiterPasswords.inspect()").isNull());
    }
    void oldDocumentCannotFillAfterNavigation() {
        load("<form><input autocomplete=username><input type=password></form>");
        const auto old = js("__arbiterPasswords.inspect()").toMap();
        load("<form><input autocomplete=username><input type=password></form>");
        js("__arbiterPasswords.inspect()");
        QVERIFY(!js(PasswordManager::fillScript("https://example.test", old["document"].toString(), old["form"].toString(), "alice", "secret")).toBool());
        QCOMPARE(js("document.querySelector('[type=password]').value", 0).toString(), QString());
    }
};
QTEST_MAIN(PasswordFormsTest)
#include "passwordforms_test.moc"
