#include "../src/history.h"
#include <QAbstractItemModelTester>
#include <QFile>
#include "../src/settings.h"
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QtTest>

class HistoryTest : public QObject {
    Q_OBJECT
    QTemporaryDir settings;
private slots:
    void initTestCase() {
        QCoreApplication::setOrganizationName("ArbiterTests");
        QCoreApplication::setApplicationName("History");
        qputenv("XDG_CONFIG_HOME", settings.path().toUtf8());
        qputenv("XDG_CONFIG_DIRS", settings.path().toUtf8());
    }
    void init() { Settings().clear(); }
    void recordsPersistsAndSearches() {
        QTemporaryDir directory;
        {
            History history(directory.path());
            QAbstractItemModelTester tester(&history, QAbstractItemModelTester::FailureReportingMode::QtTest);
            QVERIFY(history.error().isEmpty());
            QVERIFY(!history.record(QUrl("https://example.test/a"), "First visit").isEmpty());
            QVERIFY(!history.record(QUrl("https://example.test/a"), "Second visit").isEmpty());
            QVERIFY(!history.record(QUrl("https://other.test/path?q=hello#section"), "École").isEmpty());
            QCOMPARE(history.totalCount(), 3);
            QCOMPARE(history.data(history.index(0), History::PageTitle).toString(), QString("École"));
            QCOMPARE(history.data(history.index(0), History::Day).toString(), QString("Today"));
            history.setSearch("éCOLE"); QCOMPARE(history.count(), 1);
            history.setSearch("EXAMPLE.TEST"); QCOMPARE(history.count(), 2);
            history.setSearch("missing"); QCOMPARE(history.count(), 0); QCOMPARE(history.totalCount(), 3);
            history.setSearch(""); QCOMPARE(history.count(), 3);
        }
        History restored(directory.path());
        QCOMPARE(restored.totalCount(), 3);
        QCOMPARE(restored.data(restored.index(0), History::PageUrl).toString(), QString("https://other.test/path?q=hello#section"));
        QVERIFY(restored.data(restored.index(0), History::VisitedAt).toDateTime().isValid());
    }
    void exclusionsAndPause() {
        QTemporaryDir directory;
        History history(directory.path());
        for (const auto &url : {"about:blank", "file:///tmp/private.txt", "data:text/plain,secret", "javascript:alert(1)", "arbiter://settings", "https://"})
            QVERIFY(history.record(QUrl(url), "Excluded").isEmpty());
        QVERIFY(history.record(QUrl("https://example.test/private"), "Private", true).isEmpty());
        history.setEnabled(false);
        QVERIFY(history.record(QUrl("https://example.test/paused"), "Paused").isEmpty());
        History paused(directory.path()); QVERIFY(!paused.enabled());
        history.setEnabled(true);
        QVERIFY(!history.record(QUrl("https://user:password@example.test/ok"), "Allowed").isEmpty());
        QCOMPARE(history.totalCount(), 1);
        QCOMPARE(history.data(history.index(0), History::PageUrl).toString(), QString("https://example.test/ok"));
    }
    void updatesDeletionAndClear() {
        QTemporaryDir directory;
        History history(directory.path());
        QAbstractItemModelTester tester(&history, QAbstractItemModelTester::FailureReportingMode::QtTest);
        const QUrl url("https://example.test/page");
        const auto id = history.record(url, "Initial");
        history.updateTitle(id, QUrl("https://other.test/"), "Wrong page");
        QCOMPARE(history.data(history.index(0), History::PageTitle).toString(), QString("Initial"));
        history.updateTitle(id, url, "Updated");
        QCOMPARE(history.data(history.index(0), History::PageTitle).toString(), QString("Updated"));
        QVERIFY(history.remove(id)); QCOMPARE(history.totalCount(), 0);
        history.updateTitle(id, url, "Late title"); QCOMPARE(history.totalCount(), 0);
        const auto next = history.record(url, "Another"); QVERIFY(next != id);
        history.setSearch("Another"); QCOMPARE(history.count(), 1);
        QVERIFY(history.clear()); QCOMPARE(history.count(), 0); QCOMPARE(history.totalCount(), 0);
        History restored(directory.path()); QCOMPARE(restored.totalCount(), 0);
        const auto after = history.record(url, "After clear"); QVERIFY(after != next);
    }
    void importsWithoutTitlesAndRestoresUrls() {
        QTemporaryDir directory;
        const auto time=QDateTime::currentDateTime().addSecs(-100);
        {
            History history(directory.path());
            QCOMPARE(history.importVisits({{QUrl("https://untitled.test/"),QString(),time}}),1);
            QCOMPARE(history.data(history.index(0),History::PageUrl).toString(),QString("https://untitled.test/"));
            QCOMPARE(history.data(history.index(0),History::PageTitle).toString(),QString("untitled.test"));
            QVERIFY(!history.record(QUrl("https://another.test/"),QString()).isEmpty());
        }
        History restored(directory.path());
        QCOMPARE(restored.totalCount(),2);
        QCOMPARE(restored.data(restored.index(1),History::PageUrl).toString(),QString("https://untitled.test/"));
        QCOMPARE(restored.data(restored.index(1),History::VisitedAt).toDateTime(),time);
    }
    void importedHistoryUsesVisitDatesAndKeepsNewestAcrossSources() {
        QTemporaryDir directory; History history(directory.path());
        history.record(QUrl("https://current.test/"),"Current");
        const auto now=history.data(history.index(0),History::VisitedAt).toDateTime();
        QList<ImportedVisit> visits;
        for(int i=1;i<=10002;++i) visits.append({QUrl("https://old.test/"),QString::number(i),now.addSecs(-i)});
        QCOMPARE(history.importVisits(visits),10002); QCOMPARE(history.totalCount(),10000);
        QCOMPARE(history.data(history.index(0),History::PageTitle).toString(),QString("Current"));
        QCOMPARE(history.data(history.index(9999),History::VisitedAt).toDateTime(),now.addSecs(-9999));
        QCOMPARE(history.importVisits({{QUrl("https://current.test/"),"Duplicate must not overwrite",now}}),0);
        QCOMPARE(history.data(history.index(0),History::PageTitle).toString(),QString("Current"));
        history.setEnabled(false);
        QCOMPARE(history.importVisits({{QUrl("https://manual.test/"),"Explicit import",now.addMSecs(-500)}}),1);
        QVERIFY(!history.enabled()); QVERIFY(history.record(QUrl("https://paused.test/"),"No automatic recording").isEmpty());
        QCOMPARE(history.data(history.index(1),History::PageTitle).toString(),QString("Explicit import"));
    }
    void retentionAndCorruptStorage() {
        QTemporaryDir directory;
        { History initialize(directory.path()); }
        {
            auto db = QSqlDatabase::addDatabase("QSQLITE", "history-seed");
            db.setDatabaseName(directory.filePath("history.sqlite")); QVERIFY(db.open());
            QVERIFY(db.transaction());
            {
                QSqlQuery seed(db);
                seed.prepare("INSERT INTO visits(url,title,visited) VALUES ('https://example.test/',?,?)");
                for (int i = 0; i < 10002; ++i) {
                    seed.bindValue(0, QString::number(i)); seed.bindValue(1, QDateTime::currentMSecsSinceEpoch()); QVERIFY(seed.exec());
                }
            }
            QVERIFY(db.commit()); db.close();
        }
        QSqlDatabase::removeDatabase("history-seed");
        History history(directory.path()); QCOMPARE(history.totalCount(), 10000);
        QVERIFY(!history.record(QUrl("https://example.test/new"), "Newest").isEmpty());
        QCOMPARE(history.totalCount(), 10000);
        QCOMPARE(history.data(history.index(0), History::PageTitle).toString(), QString("Newest"));
        QTemporaryDir broken;
        const QByteArray original("not a sqlite database");
        { QFile file(broken.filePath("history.sqlite")); QVERIFY(file.open(QIODevice::WriteOnly)); file.write(original); }
        History corrupted(broken.path()); QVERIFY(!corrupted.error().isEmpty());
        QVERIFY(corrupted.record(QUrl("https://example.test/"), "No overwrite").isEmpty());
        QFile file(broken.filePath("history.sqlite")); QVERIFY(file.open(QIODevice::ReadOnly)); QCOMPARE(file.readAll(), original);
    }
};
QTEST_GUILESS_MAIN(HistoryTest)
#include "history_test.moc"
