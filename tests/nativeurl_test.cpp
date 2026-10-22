#include "../src/browserapplication.h"
#include <QtTest>

class NativeUrlTest : public QObject
{
    Q_OBJECT
private slots:
    void queuesAndDeliversOnlyWebUrls()
    {
        auto *app = static_cast<BrowserApplication *>(QCoreApplication::instance());
        const QUrl first("https://example.com/path?q=one#section");
        QFileOpenEvent pending(first);
        QVERIFY(QCoreApplication::sendEvent(app, &pending));
        QList<QUrl> received;
        app->setUrlHandler([&received](const QUrl &url) { received.append(url); });
        QCOMPARE(received, QList<QUrl>{first});
        QFileOpenEvent next(QUrl("http://localhost:8080/test"));
        QVERIFY(QCoreApplication::sendEvent(app, &next));
        QCOMPARE(received.last(), next.url());
        for (const QString &value : {QString("file:///etc/hosts"), QString("javascript:alert(1)"), QString("https:///missing-host")}) {
            QFileOpenEvent rejected{QUrl(value)};
            QVERIFY(!QCoreApplication::sendEvent(app, &rejected));
        }
        QCOMPARE(received.size(), 2);
        app->setUrlHandler([](const QUrl &) {});
    }
};
int main(int argc, char **argv)
{
    BrowserApplication app(argc, argv);
    NativeUrlTest test;
    return QTest::qExec(&test, argc, argv);
}
#include "nativeurl_test.moc"
