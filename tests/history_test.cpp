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
    void suggestionsRankDeduplicateAndDoNotChangeHistorySearch() {
        QTemporaryDir directory; History history(directory.path());
        history.record(QUrl("https://user:secret@example.test/docs#first"),"Example documentation");
        history.record(QUrl("https://example.test/docs#second"),"Example documentation");
        history.record(QUrl("https://other.test/recent"),"Recent other page");
        history.setSearch("other"); QCOMPARE(history.count(),1);
        const auto suggestions=history.suggestions("EXAMPLE docs",8);
        QCOMPARE(suggestions.size(),1);
        QCOMPARE(suggestions.first().toMap().value("url").toString(),QString("https://example.test/docs"));
        QCOMPARE(history.search(),QString("other")); QCOMPARE(history.count(),1);
        const auto frequent=history.suggestions("",1); QCOMPARE(frequent.size(),1);
        QCOMPARE(frequent.first().toMap().value("title").toString(),QString("Example documentation"));
        QVERIFY(history.suggestions("not in history",8).isEmpty()); QVERIFY(history.suggestions("",0).isEmpty());
        history.clear(); QVERIFY(history.suggestions("",8).isEmpty());
        History privateHistory(QString{}); QVERIFY(privateHistory.suggestions("",8).isEmpty());
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
    void combinedWebsiteAndInclusiveLocalDateFilters() {
        QTemporaryDir directory; History history(directory.path());
        QAbstractItemModelTester tester(&history, QAbstractItemModelTester::FailureReportingMode::QtTest);
        const auto day = QDate::currentDate().addDays(-3);
        const auto start = day.startOfDay();
        const auto end = day.addDays(1).startOfDay().addMSecs(-1);
        QCOMPARE(history.importVisits({
            {QUrl("https://example.test/start"), "Docs start", start},
            {QUrl("https://docs.example.test/end"), "Docs end", end},
            {QUrl("https://example.test/earlier"), "Docs earlier", start.addMSecs(-1)},
            {QUrl("https://example.test/later"), "Docs later", end.addMSecs(1)},
            {QUrl("https://notexample.test/"), "Docs unrelated", start},
            {QUrl("https://example.test.evil.test/"), "Docs deceptive", start},
            {QUrl("https://example.test/other"), "Other title", start}
        }), 7);
        history.setWebsite(" HTTPS://EXAMPLE.TEST/path ");
        QCOMPARE(history.website(), QString("example.test"));
        QCOMPARE(history.count(), 5);
        QVERIFY(history.setDateRange(day.toString(Qt::ISODate), day.toString(Qt::ISODate)));
        QCOMPARE(history.count(), 3);
        history.setSearch("Docs"); QCOMPARE(history.count(), 2);
        QVERIFY(history.hasFilters());
        QCOMPARE(history.data(history.index(0), History::Website).toString(), QString("docs.example.test"));
        QVERIFY(!history.setDateRange("2026-02-30", ""));
        QVERIFY(!history.setDateRange(day.addDays(1).toString(Qt::ISODate), day.toString(Qt::ISODate)));
        QCOMPARE(history.count(), 2); QCOMPARE(history.fromDate(), day);
        history.setWebsite("missing.test"); QCOMPARE(history.count(), 0);
        history.resetFilters(); QVERIFY(!history.hasFilters()); QCOMPARE(history.count(), 7);
        history.setWebsite("."); QCOMPARE(history.count(), 0);
        history.resetFilters();
        QVERIFY(history.setDateRange("", day.addDays(-1).toString(Qt::ISODate))); QCOMPARE(history.count(), 1);
        QVERIFY(history.setDateRange(day.addDays(1).toString(Qt::ISODate), "")); QCOMPARE(history.count(), 1);
        history.resetFilters(); QCOMPARE(history.suggestions("Docs", 20).size(), 6);
    }
    void deletesOnlyConfirmedMatchingSnapshotAndPersists() {
        QTemporaryDir directory;
        {
            History history(directory.path());
            QAbstractItemModelTester tester(&history, QAbstractItemModelTester::FailureReportingMode::QtTest);
            history.record(QUrl("https://keep.test/"), "Keep me");
            history.record(QUrl("https://remove.test/a"), "Remove A");
            history.record(QUrl("https://remove.test/b"), "Remove B");
            history.setWebsite("remove.test");
            const auto confirmed = history.matchingIds(); QCOMPARE(confirmed.size(), 2);
            history.record(QUrl("https://remove.test/new"), "New after confirmation");
            history.resetFilters(); // A later filter change must not broaden a captured deletion.
            QVERIFY(history.removeVisits(confirmed)); QCOMPARE(history.totalCount(), 2);
            QVERIFY(history.removeVisits(confirmed)); QCOMPARE(history.totalCount(), 2);
            QVERIFY(history.removeVisits({})); QCOMPARE(history.totalCount(), 2);
            history.setWebsite("remove.test"); QCOMPARE(history.count(), 1);
            QCOMPARE(history.data(history.index(0), History::PageTitle).toString(), QString("New after confirmation"));
        }
        History restored(directory.path()); QCOMPARE(restored.totalCount(), 2);
        restored.setSearch("Remove A"); QCOMPARE(restored.count(), 0);
        QVERIFY(restored.clear()); QCOMPARE(restored.totalCount(), 0);
        History privateHistory(QString{}); QVERIFY(!privateHistory.removeVisits({"1"}));
    }
    void matchingDeletionRollsBackOnStorageFailure() {
        QTemporaryDir directory; History history(directory.path());
        history.record(QUrl("https://example.test/a"), "Allowed");
        history.record(QUrl("https://example.test/b"), "Blocked");
        {
            auto db = QSqlDatabase::addDatabase("QSQLITE", "history-delete-failure");
            db.setDatabaseName(directory.filePath("history.sqlite")); QVERIFY(db.open());
            {
                QSqlQuery query(db);
                QVERIFY(query.exec("CREATE TRIGGER fail_delete BEFORE DELETE ON visits WHEN OLD.title='Blocked' BEGIN SELECT RAISE(ABORT,'synthetic storage failure'); END"));
            }
            db.close();
        }
        QSqlDatabase::removeDatabase("history-delete-failure");
        QVERIFY(!history.removeVisits(history.matchingIds()));
        QVERIFY(!history.error().isEmpty()); QCOMPARE(history.totalCount(), 2);
        History restored(directory.path()); QCOMPARE(restored.totalCount(), 2);
    }
};
QTEST_GUILESS_MAIN(HistoryTest)
#include "history_test.moc"
