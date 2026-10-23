#include "../src/splitlayout.h"
#include "../src/tabs.h"
#include "../src/workspaces.h"
#include <QTemporaryDir>
#include <QtTest>
#include <cmath>

class SplitLayoutTest : public QObject {
    Q_OBJECT
    static int paneTab(const QVariant &pane) { return pane.toMap().value("tabId").toInt(); }
private slots:
    void nextHiddenTabSkipsVisibleTabsAndOtherWorkspaces() {
        Tabs tabs; SplitLayout layout(&tabs);
        layout.split(true); layout.split(false);
        const auto before=layout.panes();
        const auto handles=layout.handles();
        const int active=layout.activePane();
        const int hidden=tabs.add(QUrl("https://example.test/hidden"),false);
        tabs.addToWorkspace(QUrl("https://example.test/other"),"other","Default");
        const int hidden2=tabs.add(QUrl("https://example.test/hidden2"),false);
        // Start before two visible tabs; skip both to replace only this pane.
        tabs.setCurrentIndex(0);
        const int firstPane=layout.activePane();
        QVERIFY(layout.nextHiddenTab()); QCOMPARE(tabs.currentIndex(),hidden);
        QCOMPARE(layout.activePane(),firstPane); QCOMPARE(layout.handles(),handles);
        for(int i=1;i<before.size();++i) QCOMPARE(layout.panes()[i],before[i]);
        QVERIFY(layout.nextHiddenTab()); QCOMPARE(tabs.currentIndex(),hidden2);
        QVERIFY(layout.nextHiddenTab()); QCOMPARE(tabs.currentIndex(),0); // wrap
        QCOMPARE(layout.activePane(),firstPane);
        layout.activate(active);
        QVERIFY(layout.nextHiddenTab()); QCOMPARE(tabs.currentIndex(),hidden);
    }
    void nextHiddenTabHandlesSinglePaneAndAllTabsVisible() {
        Tabs tabs; SplitLayout layout(&tabs);
        const auto initial=layout.snapshot();
        QVERIFY(!layout.nextHiddenTab()); QCOMPARE(layout.snapshot(),initial);
        const int added=tabs.add(QUrl("https://example.test/next"),false);
        const int active=layout.activePane();
        QVERIFY(layout.nextHiddenTab()); QCOMPARE(tabs.currentIndex(),added);
        QCOMPARE(layout.activePane(),active); QCOMPARE(layout.panes().size(),1);
        QVERIFY(layout.nextHiddenTab()); QCOMPARE(tabs.currentIndex(),0);
        tabs.close(added);
        layout.split(true); layout.split(false);
        const auto allVisible=layout.snapshot();
        QVERIFY(!layout.nextHiddenTab()); QCOMPARE(layout.snapshot(),allVisible);
        while(tabs.count()) tabs.close(0);
        QVERIFY(!layout.nextHiddenTab());
    }
    void nestedResizeSwapAndRemove() {
        Tabs tabs; SplitLayout layout(&tabs); layout.setViewport(1200,800);
        const int original=tabs.idAt(0);
        layout.split(true); QCOMPARE(layout.panes().size(),2); QCOMPARE(tabs.count(),2);
        layout.split(false); QCOMPARE(layout.panes().size(),3); QCOMPARE(layout.handles().size(),2);
        const auto before=layout.panes();
        const int first=before[0].toMap().value("paneId").toInt();
        const int last=before[2].toMap().value("paneId").toInt();
        const int lastTab=paneTab(before[2]);
        QCOMPARE(paneTab(before[0]),original);
        const auto handle=layout.handles()[0].toMap();
        layout.resize(handle.value("handleId").toInt(),750,0);
        QVERIFY(layout.panes()[0].toMap().value("width").toReal()>700);
        layout.swap(last,first);
        QCOMPARE(paneTab(layout.panes()[0]),lastTab);
        QCOMPARE(paneTab(layout.panes()[2]),original);
        QCOMPARE(layout.activePane(),first); QCOMPARE(tabs.idAt(tabs.currentIndex()),lastTab);
        layout.remove(first); QCOMPARE(layout.panes().size(),2); QCOMPARE(tabs.count(),3);
        layout.single(); QCOMPARE(layout.panes().size(),1); QCOMPARE(tabs.count(),3);
        QVERIFY(!layout.multiple());
    }
    void choosingTabsAndClosingCollapsesOnlyAffectedPane() {
        Tabs tabs; SplitLayout layout(&tabs);
        layout.split(true); layout.split(false);
        const int last=tabs.idAt(tabs.currentIndex());
        tabs.setCurrentIndex(0); QCOMPARE(layout.geometry(tabs.idAt(0)).value("paneId").toInt(),layout.activePane());
        tabs.add(QUrl("https://example.test/new"));
        QCOMPARE(layout.panes().size(),3); QVERIFY(layout.geometry(last).contains("paneId"));
        const int active=tabs.idAt(tabs.currentIndex());
        tabs.close(0); QCOMPARE(layout.panes().size(),3); QCOMPARE(tabs.idAt(tabs.currentIndex()),active);
        tabs.close(tabs.currentIndex()); QCOMPARE(layout.panes().size(),2);
        QVERIFY(layout.geometry(tabs.idAt(tabs.currentIndex())).contains("paneId"));
        while(tabs.count()) tabs.close(0);
        QCOMPARE(layout.panes().size(),0);
        tabs.add(); QCOMPARE(layout.panes().size(),1);
    }
    void minimumSizesAndRepeatedSplits() {
        Tabs tabs; SplitLayout layout(&tabs); layout.setViewport(640,480);
        for(int i=0;i<40;++i) layout.split(i%2==0);
        QCOMPARE(layout.panes().size(),41); QCOMPARE(layout.handles().size(),40);
        QVERIFY(layout.minimumWidth()>640); QVERIFY(layout.minimumHeight()>480);
        for(const auto &p:layout.panes()) {
            const auto map=p.toMap(); QVERIFY(map.value("width").toReal()>=199.99); QVERIFY(map.value("height").toReal()>=159.99);
        }
        for(const auto &h:layout.handles()) layout.resize(h.toMap().value("handleId").toInt(),-100000,100000);
        for(const auto &p:layout.panes()) { QVERIFY(p.toMap().value("width").toReal()>=199.99); QVERIFY(p.toMap().value("height").toReal()>=159.99); }
        const auto previous=layout.snapshot(); layout.resize(1,NAN,0); QCOMPARE(layout.snapshot(),previous);
    }
    void workspacePersistenceAndMoves() {
        QTemporaryDir directory; QVariantMap snapshot; QString work;
        {
            Tabs tabs; Workspaces spaces(&tabs,directory.path()); auto *layout=spaces.splitLayout();
            layout->split(true); layout->split(false); snapshot=layout->snapshot();
            const int mainTab=tabs.idAt(tabs.currentIndex());
            work=spaces.create("Work",QColor("#112233"),false);
            QCOMPARE(layout->panes().size(),1); layout->split(false);
            spaces.activate("main"); QCOMPARE(layout->panes().size(),3); QCOMPARE(tabs.idAt(tabs.currentIndex()),mainTab);
            spaces.flush();
        }
        Tabs tabs; Workspaces spaces(&tabs,directory.path()); auto *layout=spaces.splitLayout();
        QCOMPARE(layout->panes().size(),3); QCOMPARE(layout->snapshot().value("main"),snapshot.value("main"));
        spaces.activate(work); QCOMPARE(layout->panes().size(),2);
        QVERIFY(spaces.moveTab(tabs.currentIndex(),"main"));
        QCOMPARE(layout->panes().size(),3);
        spaces.activate(work); QCOMPARE(layout->panes().size(),1);
    }
    void malformedRestoreCannotCreateCyclesOrDuplicatePages() {
        Tabs tabs; SplitLayout layout(&tabs); layout.split(true);
        auto saved=layout.snapshot(); auto tree=saved.value("main").toMap();
        auto nodes=tree.value("nodes").toList();
        for(auto &value:nodes) { auto n=value.toMap(); if(n.value("first").toInt()) { n["first"]=n["id"]; value=n; break; } }
        tree["nodes"]=nodes; saved["main"]=tree; layout.restore(saved);
        QCOMPARE(layout.panes().size(),1);
        layout.split(false); saved=layout.snapshot(); tree=saved.value("main").toMap(); nodes=tree.value("nodes").toList();
        int tab=0;
        for(auto &value:nodes) { auto n=value.toMap(); if(!n.value("first").toInt()) { if(!tab) tab=n.value("tab").toInt(); else n["tab"]=tab; value=n; } }
        tree["nodes"]=nodes; saved["main"]=tree; layout.restore(saved);
        QCOMPARE(layout.panes().size(),1);
    }
};
QTEST_GUILESS_MAIN(SplitLayoutTest)
#include "splitlayout_test.moc"
