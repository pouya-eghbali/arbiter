#include "../src/splitswapgesture.h"
#include <QQuickWindow>
#include <QMouseEvent>
#include <QtTest>

namespace {
constexpr auto modifiers=Qt::ControlModifier | Qt::ShiftModifier;
QVariantList panes() { return {QVariantMap{{"paneId",1},{"x",0},{"y",0},{"width",100},{"height",200}},
    QVariantMap{{"paneId",2},{"x",108},{"y",0},{"width",100},{"height",200}}}; }
class Window : public QQuickWindow {
public:
    int presses=0;
    bool event(QEvent *event) override { if(event->type()==QEvent::MouseButtonPress) ++presses; return QQuickWindow::event(event); }
};
void mouse(QWindow &window,QEvent::Type type,QPointF point,Qt::KeyboardModifiers mods=modifiers) {
    const auto button=type==QEvent::MouseMove ? Qt::NoButton : Qt::LeftButton;
    QMouseEvent event(type,point,point,window.mapToGlobal(point.toPoint()),button,
        type==QEvent::MouseButtonRelease ? Qt::NoButton : Qt::LeftButton,mods);
    QCoreApplication::sendEvent(&window,&event);
}
}
class SplitSwapGestureTest : public QObject {
    Q_OBJECT
private slots:
    void modifierGestureConsumesTheWholeSequence() {
        Window window; window.resize(300,240); window.show();
        SplitSwapGesture gesture(window.contentItem()); gesture.setSize({208,200}); gesture.setPosition({10,20}); gesture.setPanes(panes());
        QSignalSpy started(&gesture,&SplitSwapGesture::started),moved(&gesture,&SplitSwapGesture::moved),finished(&gesture,&SplitSwapGesture::finished);
        mouse(window,QEvent::MouseButtonPress,{40,50}); QCOMPARE(window.presses,0); QCOMPARE(started.count(),1);
        QCOMPARE(started.first()[0].toInt(),1); QCOMPARE(started.first()[1].toPointF(),QPointF(30,30));
        mouse(window,QEvent::MouseMove,{160,100}); QCOMPARE(moved.count(),1);
        mouse(window,QEvent::MouseButtonRelease,{160,100}); QCOMPARE(finished.count(),1); QVERIFY(finished.first()[0].toBool());
    }
    void ordinaryAndWrongModifierDragsReachThePage() {
        Window window; window.resize(300,240); window.show();
        SplitSwapGesture gesture(window.contentItem()); gesture.setSize({208,200}); gesture.setPanes(panes());
        QSignalSpy started(&gesture,&SplitSwapGesture::started);
        for(auto mods:{Qt::KeyboardModifiers(),Qt::KeyboardModifiers(Qt::ControlModifier),Qt::KeyboardModifiers(Qt::ShiftModifier),Qt::KeyboardModifiers(Qt::AltModifier),Qt::KeyboardModifiers(modifiers|Qt::AltModifier)}) {
            const int before=window.presses;
            mouse(window,QEvent::MouseButtonPress,{30,30},mods); mouse(window,QEvent::MouseMove,{160,100},mods); mouse(window,QEvent::MouseButtonRelease,{160,100},mods);
            QCOMPARE(window.presses,before+1);
        }
        QCOMPARE(started.count(),0);
        mouse(window,QEvent::MouseButtonPress,{104,50}); QCOMPARE(started.count(),0); // Divider gap.
        gesture.setEnabled(false); mouse(window,QEvent::MouseButtonPress,{30,30}); QCOMPARE(started.count(),0);
    }
    void clickDoesNotSwapAndCancellationCannotLaterCommit() {
        Window window; window.resize(300,240); window.show();
        SplitSwapGesture gesture(window.contentItem()); gesture.setSize({208,200}); gesture.setPanes(panes());
        QSignalSpy finished(&gesture,&SplitSwapGesture::finished);
        mouse(window,QEvent::MouseButtonPress,{30,30}); mouse(window,QEvent::MouseButtonRelease,{30,30}); QVERIFY(!finished.last()[0].toBool());
        mouse(window,QEvent::MouseButtonDblClick,{30,30}); mouse(window,QEvent::MouseButtonRelease,{30,30}); QVERIFY(!finished.last()[0].toBool());
        const auto attempt=[&] { finished.clear(); mouse(window,QEvent::MouseButtonPress,{30,30}); mouse(window,QEvent::MouseMove,{160,100}); };
        const auto end=[&] { mouse(window,QEvent::MouseButtonRelease,{160,100}); for(const auto &args:finished) QVERIFY(!args[0].toBool()); };
        attempt(); mouse(window,QEvent::MouseMove,{160,100},Qt::ShiftModifier); end();
        attempt(); QKeyEvent escape(QEvent::KeyPress,Qt::Key_Escape,modifiers); QCoreApplication::sendEvent(&window,&escape); end();
        attempt(); gesture.setEnabled(false); end(); gesture.setEnabled(true);
        attempt(); gesture.setPanes({panes().first()}); end(); gesture.setPanes(panes());
        attempt(); QEvent deactivate(QEvent::WindowDeactivate); QCoreApplication::sendEvent(&window,&deactivate); end();
    }
};
QTEST_MAIN(SplitSwapGestureTest)
#include "splitswapgesture_test.moc"
