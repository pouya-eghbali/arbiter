#include "../src/tabs.h"
#include <QAbstractItemModelTester>
#include <QtTest>

class TabsTest : public QObject
{
    Q_OBJECT
private slots:
    void insertionPreservesSelectedIdentity() {
        Tabs tabs;
        QAbstractItemModelTester tester(&tabs, QAbstractItemModelTester::FailureReportingMode::QtTest);
        tabs.add(QUrl("https://example.com/"));
        const int selectedId=tabs.idAt(tabs.currentIndex());
        QCOMPARE(tabs.addToWorkspace(QUrl("https://example.org/"),"main","Default",0),0);
        QCOMPARE(tabs.currentIndex(),2);
        QCOMPARE(tabs.idAt(tabs.currentIndex()),selectedId);
        QCOMPARE(tabs.data(tabs.index(0),Tabs::Url).toUrl(),QUrl("https://example.org/"));
        QCOMPARE(tabs.addToWorkspace(QUrl("about:blank"),"main","Default",999),3);
        QCOMPARE(tabs.idAt(tabs.currentIndex()),selectedId);
    }
    void closingPreservesSelectedPage() {
        Tabs tabs;
        QAbstractItemModelTester tester(&tabs, QAbstractItemModelTester::FailureReportingMode::QtTest);
        tabs.add(QUrl("https://example.com/"));
        tabs.add(QUrl("https://example.org/"));
        const int selectedId = tabs.data(tabs.index(2), Tabs::Id).toInt();
        tabs.close(0);
        QCOMPARE(tabs.currentIndex(), 1);
        QCOMPARE(tabs.data(tabs.index(1), Tabs::Id).toInt(), selectedId);
        tabs.close(1);
        QCOMPARE(tabs.currentIndex(), 0);
        tabs.close(0);
        QCOMPARE(tabs.currentIndex(), -1);
        QCOMPARE(tabs.count(), 0);
        tabs.cycle(1);
        tabs.setCurrentIndex(10);
        QCOMPARE(tabs.currentIndex(), -1);
    }
    void backgroundAndWraparound() {
        Tabs tabs;
        tabs.add(QUrl("https://example.com/"), false);
        QCOMPARE(tabs.currentIndex(), 0);
        tabs.cycle(-1);
        QCOMPARE(tabs.currentIndex(), 1);
        tabs.cycle(1);
        QCOMPARE(tabs.currentIndex(), 0);
        tabs.setCurrentIndex(-1);
        QCOMPARE(tabs.currentIndex(), 0);
    }
    void metadataUsesStableIdentityAndReopensLastUrl() {
        Tabs tabs;
        tabs.add(QUrl("https://example.com/"));
        const int id = tabs.data(tabs.index(1), Tabs::Id).toInt();
        tabs.close(0);
        tabs.update(id, "Example", QUrl("https://example.com/next"));
        QCOMPARE(tabs.data(tabs.index(0), Tabs::Url).toUrl(), QUrl("https://example.com/next"));
        tabs.close(0);
        tabs.update(id, "Late signal", QUrl("https://wrong.example/"));
        QVERIFY(tabs.reopen());
        QCOMPARE(tabs.data(tabs.index(0), Tabs::InitialUrl).toUrl(), QUrl("https://example.com/next"));
        QVERIFY(tabs.data(tabs.index(0), Tabs::Id).toInt() != id);
        QCOMPARE(tabs.data(tabs.index(0), Tabs::Title).toString(), QString("Example"));
    }
    void closedHistoryIsBounded() {
        Tabs tabs;
        QVERIFY(!tabs.reopen());
        for (int i = 0; i < 25; ++i) {
            tabs.add(QUrl(QString("https://example.com/%1").arg(i)));
            tabs.close(1);
        }
        int reopened = 0;
        while (tabs.reopen()) ++reopened;
        QCOMPARE(reopened, 20);
    }
};
QTEST_GUILESS_MAIN(TabsTest)
#include "tabs_test.moc"
