#include "../src/workspaces.h"
#include <QAbstractItemModelTester>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

class WorkspacesTest : public QObject {
    Q_OBJECT
private slots:
    void switchingPreservesIdentityAndSelection() {
        QTemporaryDir directory; Tabs tabs;
        QAbstractItemModelTester tester(&tabs,QAbstractItemModelTester::FailureReportingMode::QtTest);
        Workspaces spaces(&tabs,directory.path());
        const auto personal=spaces.currentId();
        tabs.add(QUrl("https://example.test/a"));
        const int first=tabs.data(tabs.index(tabs.currentIndex()),Tabs::Id).toInt();
        const auto work=spaces.create("Work",QColor("#ff8899"),false);
        QCOMPARE(tabs.workspaceCount(),1); QCOMPARE(tabs.count(),3);
        tabs.add(QUrl("https://example.test/b"));
        const int second=tabs.data(tabs.index(tabs.currentIndex()),Tabs::Id).toInt();
        spaces.activate(personal);
        QCOMPARE(tabs.data(tabs.index(tabs.currentIndex()),Tabs::Id).toInt(),first);
        QCOMPARE(tabs.workspaceCount(),2);
        tabs.cycle(1); QCOMPARE(tabs.currentIndex(),0);
        tabs.cycle(-1); QCOMPARE(tabs.data(tabs.index(tabs.currentIndex()),Tabs::Id).toInt(),first);
        spaces.activate(work);
        QCOMPARE(tabs.data(tabs.index(tabs.currentIndex()),Tabs::Id).toInt(),second);
        spaces.cycle(-1); QCOMPARE(spaces.currentId(),personal);
        spaces.cycle(-1); QCOMPARE(spaces.currentId(),work);
    }
    void persistencePinningAndProfiles() {
        QTemporaryDir directory; QString work; int selected;
        {
            Tabs tabs; Workspaces spaces(&tabs,directory.path());
            tabs.add(QUrl("https://user:password@example.test/page"));
            tabs.update(tabs.data(tabs.index(tabs.currentIndex()),Tabs::Id).toInt(),"Example",QUrl("https://example.test/page"));
            tabs.togglePinned(tabs.currentIndex());
            work=spaces.create("Work",QColor("#aabbcc"),true);
            tabs.add(QUrl("https://example.test/work"));
            selected=tabs.data(tabs.index(tabs.currentIndex()),Tabs::Id).toInt();
            QVERIFY(tabs.profileAt(tabs.currentIndex()).startsWith("Workspace-"));
            spaces.edit(work,"Job",QColor("#123456")); spaces.reorder(work,-1); spaces.flush();
        }
        Tabs tabs; Workspaces restored(&tabs,directory.path());
        QCOMPARE(restored.currentId(),work); QCOMPARE(restored.currentName(),QString("Job"));
        QCOMPARE(restored.currentColor(),QColor("#123456"));
        QCOMPARE(tabs.data(tabs.index(tabs.currentIndex()),Tabs::Id).toInt(),selected);
        QCOMPARE(restored.choices().first().toMap()["id"].toString(),work);
        restored.activate("main"); QCOMPARE(tabs.workspaceCount(),2);
        QVERIFY(tabs.pinnedAt(tabs.currentIndex()));
        QCOMPARE(tabs.data(tabs.index(tabs.currentIndex()),Tabs::Url).toUrl(),QUrl("https://example.test/page"));
    }
    void movingCopyingAndDeleting() {
        QTemporaryDir directory; Tabs tabs; Workspaces spaces(&tabs,directory.path());
        const auto shared=spaces.create("Shared",QColor("#112233"),false);
        const auto isolated=spaces.create("Isolated",QColor("#334455"),true);
        spaces.activate("main"); tabs.add(QUrl("https://example.test/move"));
        const int background=spaces.addTab(isolated,QUrl("https://example.test/popup"),false);
        QCOMPARE(spaces.currentId(),QString("main"));
        QCOMPARE(tabs.workspaceAt(background),isolated);
        QVERIFY(tabs.profileAt(background)!="Default");
        int row=tabs.currentIndex(); const int id=tabs.data(tabs.index(row),Tabs::Id).toInt();
        QVERIFY(!spaces.moveTab(row,isolated)); QCOMPARE(tabs.workspaceAt(row),QString("main"));
        QVERIFY(spaces.moveTab(row,shared)); QCOMPARE(spaces.currentId(),shared);
        QCOMPARE(tabs.data(tabs.index(tabs.currentIndex()),Tabs::Id).toInt(),id);
        QVERIFY(spaces.copyTab(tabs.currentIndex(),isolated));
        QCOMPARE(spaces.currentId(),isolated); QVERIFY(tabs.profileAt(tabs.currentIndex())!="Default");
        QCOMPARE(tabs.data(tabs.index(tabs.currentIndex()),Tabs::Url).toUrl(),QUrl("https://example.test/move"));
        QVERIFY(spaces.remove(isolated)); QCOMPARE(spaces.count(),2);
        QVERIFY(spaces.remove(shared)); QCOMPARE(spaces.count(),1);
        QVERIFY(!spaces.remove("main")); QCOMPARE(tabs.workspaceId(),QString("main"));
    }
    void closedTabsAndCorruptStorage() {
        QTemporaryDir directory; Tabs tabs; Workspaces spaces(&tabs,directory.path());
        tabs.add(QUrl("https://example.test/closed")); tabs.close(tabs.currentIndex());
        const auto other=spaces.create("Other",QColor("#778899"),false);
        QVERIFY(!tabs.reopen()); spaces.activate("main"); QVERIFY(tabs.reopen());
        QCOMPARE(tabs.data(tabs.index(tabs.currentIndex()),Tabs::Url).toUrl(),QUrl("https://example.test/closed"));
        QTemporaryDir broken; const QByteArray bytes("{broken");
        { QFile file(broken.filePath("session.json")); QVERIFY(file.open(QIODevice::WriteOnly)); file.write(bytes); }
        { Tabs temp; Workspaces corrupt(&temp,broken.path()); QVERIFY(!corrupt.error().isEmpty()); corrupt.create("Temporary",QColor("#abcdef"),false); corrupt.flush(); }
        QFile file(broken.filePath("session.json")); QVERIFY(file.open(QIODevice::ReadOnly)); QCOMPARE(file.readAll(),bytes);
        Q_UNUSED(other);
    }
    void concurrentSessionIsReadOnly() {
        QTemporaryDir directory; Tabs tabs; Workspaces owner(&tabs,directory.path());
        owner.create("Saved",QColor("#778899"),false); owner.flush();
        QFile file(directory.filePath("session.json")); QVERIFY(file.open(QIODevice::ReadOnly));
        const auto original=file.readAll(); file.close();
        {
            Tabs otherTabs; Workspaces other(&otherTabs,directory.path());
            QVERIFY(!other.error().isEmpty());
            other.create("Unsaved",QColor("#abcdef"),true); other.flush();
        }
        QVERIFY(file.open(QIODevice::ReadOnly)); QCOMPARE(file.readAll(),original);
    }
};
QTEST_GUILESS_MAIN(WorkspacesTest)
#include "workspaces_test.moc"
