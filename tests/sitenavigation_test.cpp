#include "../src/settings.h"
#include "../src/sitenavigation.h"
#include "../src/appearance.h"
#include <QApplication>
#include <QMenuBar>
#include <QMenu>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QWebEngineView>
#include <QWebEnginePage>
#include <QtTest>

class SiteNavigationTest : public QObject {
    Q_OBJECT
    QWebEngineView view;
    QMenuBar bar;
    SiteNavigation navigation{&bar};
    QTemporaryDir settings;
    QVariant js(const QString &script) {
        QVariant result;
        QEventLoop loop;
        QTimer timer;
        timer.setSingleShot(true);
        connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
        bool done = false;
        view.page()->runJavaScript(script, 1, [&](const QVariant &value) { result = value; done = true; loop.quit(); });
        timer.start(5000);
        loop.exec();
        Q_ASSERT(done);
        return result;
    }
    void load(const QString &html) {
        QSignalSpy loaded(&view, &QWebEngineView::loadFinished);
        view.setHtml("<!doctype html>" + html, QUrl("https://example.test/start"));
        QVERIFY(loaded.wait(10000));
        QVERIFY(loaded.last().first().toBool());
    }
    QVariantMap snapshot() { return js(navigation.extractionScript()).toMap(); }
    QMenu *menu() { return bar.actions().first()->menu(); }
private slots:
    void initTestCase() {
        qputenv("XDG_CONFIG_HOME", settings.path().toUtf8());
        qputenv("XDG_CONFIG_DIRS", settings.path().toUtf8());
        QCoreApplication::setOrganizationName("ArbiterSiteMenuTest");
        QCoreApplication::setApplicationName("Tests");
        view.resize(900, 600); view.show();
        QVERIFY(!navigation.extractionScript().isEmpty());
    }
    void extractionAndNativeActions() {
        load(R"HTML(<h1 id="nav-title">Primary</h1><nav aria-labelledby="nav-title"><ul>
          <li><a href="/products">Products</a><ul><li><a href="/desktop">Desktop</a></li></ul></li>
          <li><a href="/docs" aria-label="API & Docs">icon</a></li>
          <li><a href="/start" aria-current="page">Home</a></li></ul>
          <a href="/docs">Duplicate</a><a href="javascript:alert(1)">Unsafe</a>
          <a href="data:text/html,no">Data</a><a href="/download" download>Download</a>
          <a style="display:none" href="/hidden">Hidden</a><a href="https://user:secret@example.test/private">Credentials</a>
          </nav><div role="navigation" aria-label="Footer"><a href="/about">About</a></div>
          <a href="/outside">Outside nav</a><iframe srcdoc="<nav><a href='/frame'>Frame</a></nav>"></iframe>)HTML");
        auto state = snapshot();
        const auto groups = state["nodes"].toList();
        QCOMPARE(groups.size(), 2);
        QCOMPARE(groups[0].toMap()["label"].toString(), QString("Primary"));
        auto links = groups[0].toMap()["children"].toList();
        QCOMPARE(links.size(), 3);
        QCOMPARE(links[0].toMap()["children"].toList().size(), 1);
        navigation.reset(view.url(), "Example tab", "title");
        navigation.update(view.url(), state);
        QCOMPARE(bar.actions().size(), 1);
        QCOMPARE(menu()->title(), QString("Example tab"));
        auto *primary = menu()->actions()[0]->menu(); QVERIFY(primary);
        auto *products = primary->actions()[0]->menu(); QVERIFY(products);
        QSignalSpy activated(&navigation, &SiteNavigation::linkActivated);
        products->actions().last()->trigger();
        QCOMPARE(activated.size(), 1);
        QCOMPARE(activated.last()[0].toUrl(), QUrl("https://example.test/desktop"));
        QCOMPARE(primary->actions()[1]->text(), QString("API && Docs"));
        QVERIFY(primary->actions()[2]->isChecked());
        navigation.reset(view.url(), "Example tab", "navigation"); navigation.update(view.url(), state);
        QCOMPARE(bar.actions().size(), 4);
        QCOMPARE(bar.actions()[0]->text(), QString("Products"));
        QCOMPARE(menu()->actions().last()->text(), QString("Desktop"));
        navigation.reset(view.url(), "Example tab", "off"); navigation.update(view.url(), state);
        QVERIFY(bar.actions().isEmpty());
    }
    void spaAndStaleSnapshots() {
        load("<nav><a href='/one'>One</a></nav>");
        const auto old = snapshot();
        const auto oldUrl = view.url();
        navigation.reset(oldUrl, "First", "title"); navigation.update(oldUrl, old);
        QCOMPARE(menu()->actions()[0]->text(), QString("One"));
        js("history.pushState({}, '', '/next'); document.querySelector('nav').innerHTML = '<a href=\"/two#section\">Two</a>'");
        const auto next = snapshot();
        const QUrl nextUrl(next["url"].toString());
        navigation.reset(nextUrl, "Next", "title");
        navigation.update(oldUrl, old);
        QCOMPARE(menu()->actions()[0]->text(), QString("No navigation links found"));
        navigation.update(nextUrl, next);
        QCOMPARE(menu()->actions()[0]->text(), QString("Two"));
        navigation.setTitle("Updated tab"); QCOMPARE(menu()->title(), QString("Updated tab"));
        navigation.reset(QUrl("about:blank"), "New tab", "title");
        QVERIFY(bar.actions().isEmpty());
    }
    void defensiveLimitsAndPreferences() {
        QVariantList links;
        for (int i = 0; i < 200; ++i) links.append(QVariantMap{{"label", QString("Link %1").arg(i)}, {"url", QString("https://example.test/%1").arg(i)}});
        const QUrl url("https://example.test/");
        QVariantMap state{{"url", url.toString()}, {"nodes", QVariantList{QVariantMap{{"label", "Primary"}, {"children", links}}}}};
        navigation.reset(url, "Example", "navigation"); navigation.update(url, state);
        QCOMPARE(bar.actions().size(), 7); // Six headings and bounded overflow.
        QVERIFY(bar.actions().last()->menu()->actions().size() <= 114);
        Appearance appearance;
        QCOMPARE(appearance.siteMenuMode(), QString("title"));
        appearance.setSiteMenuMode("navigation");
        Appearance restored; QCOMPARE(restored.siteMenuMode(), QString("navigation"));
        appearance.setSiteMenuMode("invalid"); QCOMPARE(appearance.siteMenuMode(), QString("navigation"));
        appearance.setSiteMenuMode("off"); QCOMPARE(appearance.siteMenuMode(), QString("off"));
    }
    void windowDecorationPreference() {
        Appearance appearance;
        QVERIFY(!appearance.windowDecorations());
        QSignalSpy changed(&appearance, &Appearance::windowDecorationsChanged);
        appearance.setWindowDecorations(true);
        QCOMPARE(changed.size(), 1);
        appearance.setWindowDecorations(true);
        QCOMPARE(changed.size(), 1);
        Appearance decorated;
        QVERIFY(decorated.windowDecorations());
        appearance.setWindowDecorations(false);
        Appearance restored;
        QVERIFY(!restored.windowDecorations());
    }
};
QTEST_MAIN(SiteNavigationTest)
#include "sitenavigation_test.moc"
