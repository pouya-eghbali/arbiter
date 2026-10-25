#include <QtTest>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QQuickItem>
#include <cmath>

class GlassSurfaceTest : public QObject {
    Q_OBJECT
private slots:
    void backdropBlur();
};

void GlassSurfaceTest::backdropBlur()
{
    QQmlApplicationEngine engine;
    engine.addImportPath("qrc:/glass-test");
    engine.loadData(R"QML(
        import QtQuick
        import Arbiter
        Window {
            id: window
            width: 640; height: 400; visible: true
            property bool colorful: false
            readonly property Item glassBackdrop: backdrop
            Component.onCompleted: Theme.dark = true
            Item {
                id: backdrop
                anchors.fill: parent
                Repeater {
                    model: 160
                    Rectangle {
                        required property int index
                        x: index * 4; width: 4; height: backdrop.height
                        color: window.colorful ? (index % 2 ? "red" : "blue") : (index % 2 ? "white" : "black")
                    }
                }
            }
            Item {
                id: holder
                objectName: "holder"
                x: 60; y: 80
                GlassSurface { objectName: "pill"; width: 240; height: 56; radius: 28; shadowEnabled: false }
                GlassSurface { objectName: "collapsed"; y: 100; width: 240; height: 6; radius: 3; shadowEnabled: false }
            }
            UiToolTip { parent: window.contentItem; x: 70; y: 320; text: "Workspaces: Personal"; visible: true; delay: 0; timeout: -1 }
            GlassSurface { objectName: "panel"; x: 360; y: 80; width: 240; height: 220; shadowEnabled: false }
        }
    )QML");
    QVERIFY(!engine.rootObjects().isEmpty());
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    QVERIFY(window);
    QVERIFY(QTest::qWaitForWindowExposed(window));
    auto *pill = window->findChild<QQuickItem *>("pill");
    auto *collapsed = window->findChild<QQuickItem *>("collapsed");
    auto *panel = window->findChild<QQuickItem *>("panel");
    QVERIFY(pill && collapsed && panel);
    if (!pill->property("effectsAvailable").toBool()) {
        QVERIFY(!pill->property("samplesBackdrop").toBool());
        QSKIP("Backdrop pixel checks require a hardware Qt Quick backend");
    }
    QTRY_VERIFY(pill->property("samplesBackdrop").toBool());
    QTest::qWait(150);
    const auto scan = [window](const QImage &image, const QQuickItem *surface) {
        const QPointF origin = surface->mapToScene(QPointF(0, 0));
        const double scale = double(image.width()) / window->width();
        const int y = qRound((origin.y() + surface->height() / 2) * scale);
        double sum = 0, square = 0;
        int count = 0;
        for (int x = qRound((origin.x() + 40) * scale); x < qRound((origin.x() + surface->width() - 40) * scale); ++x) {
            const double value = qGray(image.pixel(x, y));
            sum += value; square += value * value; ++count;
        }
        const double mean = sum / count;
        return QPointF(mean, std::sqrt(std::max(0., square / count - mean * mean)));
    };
    const QImage stripes = window->grabWindow();
    QVERIFY(!stripes.isNull());
    if (const auto path=qEnvironmentVariable("ARBITER_TEST_GLASS_IMAGE");!path.isEmpty())
        QVERIFY(stripes.save(path));
    // Sharp stripes behind a translucent tint retain strong contrast. Actual
    // backdrop blur must suppress them, including on the six-pixel pill.
    for (auto *surface : {pill, collapsed, panel}) {
        const auto values = scan(stripes, surface);
        QVERIFY2(values.y() < 3, qPrintable(QString("Unblurred stripe deviation: %1").arg(values.y())));
    }
    window->setProperty("colorful", true);
    QTest::qWait(150);
    const QImage colored = window->grabWindow();
    for (auto *surface : {pill, collapsed, panel}) {
        // An opaque fallback would also hide stripes, but cannot respond to a
        // change in the page's colors. Verify that the backdrop remains live.
        QVERIFY(std::abs(scan(stripes, surface).x() - scan(colored, surface).x()) > 8);
    }
    auto *holder = window->findChild<QQuickItem *>("holder");
    QVERIFY(holder);
    holder->setX(100); holder->setY(120);
    QTRY_COMPARE(pill->property("samplePosition").toPointF(), QPointF(100, 120));
    // Window-local sources must never sample another browser/private window.
    QQuickWindow foreign;
    QQuickItem foreignSource(foreign.contentItem());
    pill->setProperty("sourceItem", QVariant::fromValue(&foreignSource));
    QTRY_VERIFY(!pill->property("samplesBackdrop").toBool());
}

QTEST_MAIN(GlassSurfaceTest)
#include "glasssurface_test.moc"
