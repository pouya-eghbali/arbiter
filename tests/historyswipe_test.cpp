#include <QtTest>
#include <QQuickItem>
#include "../src/historyswipe.h"

class HistorySwipeTest : public QObject {
    Q_OBJECT
private slots:
    void commitsOnlyAfterCompositorOverscrollAndRelease() {
        QQuickItem view; view.setWidth(1000);
        HistorySwipe swipe; swipe.setView(&view); swipe.setCanGoBack(true);
        QSignalSpy triggered(&swipe,&HistorySwipe::triggered);
        swipe.beginGesture(true); swipe.scrollUpdate(240,0,false);
        QVERIFY(!swipe.active()); swipe.endGesture(); QCOMPARE(triggered.size(),0);
        swipe.beginGesture(true); swipe.scrollUpdate(200,0,false); swipe.overscroll(200,0,true);
        QVERIFY(swipe.active()); QCOMPARE(swipe.direction(),-1); QCOMPARE(swipe.progress(),1.);
        QCOMPARE(triggered.size(),0);
        swipe.endGesture(); QCOMPARE(triggered.size(),1); QCOMPARE(triggered.first().first().toInt(),-1);
        QVERIFY(!swipe.active());
        swipe.overscroll(200,0,true); swipe.endGesture(); QCOMPARE(triggered.size(),1);
    }
    void forwardAndCancellations() {
        QQuickItem view; view.setWidth(1000);
        HistorySwipe swipe; swipe.setView(&view); swipe.setCanGoBack(true); swipe.setCanGoForward(true);
        QSignalSpy triggered(&swipe,&HistorySwipe::triggered);
        auto gesture=[&] { swipe.beginGesture(true); swipe.scrollUpdate(200,0,false); swipe.overscroll(200,0,true); };
        swipe.beginGesture(true); swipe.scrollUpdate(-200,0,false); swipe.overscroll(-200,0,true); swipe.endGesture();
        QCOMPARE(triggered.size(),1); QCOMPARE(triggered.takeFirst().first().toInt(),1);
        swipe.beginGesture(true); swipe.scrollUpdate(50,0,false); swipe.overscroll(50,0,true); swipe.endGesture();
        QCOMPARE(triggered.size(),0);
        gesture(); swipe.scrollUpdate(-20,0,false); swipe.endGesture(); QCOMPARE(triggered.size(),0);
        gesture(); swipe.cancel(); swipe.endGesture(); QCOMPARE(triggered.size(),0);
        gesture(); swipe.setEnabled(false); swipe.endGesture(); QCOMPARE(triggered.size(),0);
        swipe.setEnabled(true); gesture(); swipe.setCanGoBack(false); swipe.endGesture(); QCOMPARE(triggered.size(),0);
    }
    void contentConsumptionCssVerticalAndSyntheticEventsCannotNavigate() {
        QQuickItem view; HistorySwipe swipe; swipe.setView(&view); swipe.setCanGoBack(true);
        QSignalSpy triggered(&swipe,&HistorySwipe::triggered);
        for(int kind=0;kind<5;++kind) {
            swipe.beginGesture(kind!=0);
            if(kind==1) swipe.scrollUpdate(250,0,true); // nested/root content consumed it
            if(kind==2) swipe.overscroll(250,0,false); // overscroll-behavior contain/none
            if(kind==3) swipe.scrollUpdate(20,100,false); // ordinary vertical scrolling
            if(kind==4) swipe.scrollUpdate(qQNaN(),0,false);
            swipe.scrollUpdate(250,0,false); swipe.overscroll(250,0,true); swipe.endGesture();
            QCOMPARE(triggered.size(),0);
        }
    }
    void bridgeLifetimeAndStalledGesture() {
        QQuickItem view;
        auto *swipe=new HistorySwipe; swipe->setView(&view); swipe->setCanGoBack(true);
        QCOMPARE(view.property("_arbiterHistorySwipe").value<QObject *>(),swipe);
        swipe->beginGesture(true); swipe->scrollUpdate(250,0,false); swipe->overscroll(250,0,true);
        QSignalSpy triggered(swipe,&HistorySwipe::triggered);
        QTRY_VERIFY_WITH_TIMEOUT(!swipe->active(),2200); QCOMPARE(triggered.size(),0);
        delete swipe;
        QVERIFY(!view.property("_arbiterHistorySwipe").isValid());
    }
};
QTEST_MAIN(HistorySwipeTest)
#include "historyswipe_test.moc"
