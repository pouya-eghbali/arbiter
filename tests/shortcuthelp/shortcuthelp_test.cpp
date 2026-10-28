#include "../../src/keybindings.h"
#include "../../src/settings.h"
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QQuickItem>
#include <QTemporaryDir>
#include <QtTest>

class ShortcutHelpTest : public QObject {
    Q_OBJECT
    static QQuickItem *findItem(QQuickItem *parent,const QString &name) {
        if(parent->objectName()==name) return parent;
        for(auto *child:parent->childItems()) if(auto *found=findItem(child,name)) return found;
        return nullptr;
    }
private slots:
    void sheetInteractions() {
        QTemporaryDir config; qputenv("XDG_CONFIG_HOME",config.path().toUtf8());
        qputenv("XDG_CONFIG_DIRS",config.path().toUtf8()); Settings().clear();
        KeyBindings keys;
        QQmlApplicationEngine engine;
        engine.setInitialProperties({{"keyBindings",QVariant::fromValue(&keys)}});
        engine.loadData(R"QML(
import QtQuick
import QtQuick.Controls.Basic as Basic
import Arbiter
Basic.ApplicationWindow {
    id: root; width: 1000; height: 720; visible: true
    required property var keyBindings
    property bool allRequested: false
    property alias sheetOpen: sheet.opened
    property alias sheetVisible: sheet.visible
    property alias sheetWidth: sheet.width
    property alias sheetHeight: sheet.height
    Shortcut { sequence: root.keyBindings.bindings.shortcutHelp; onActivated: sheet.opened ? sheet.close() : sheet.open() }
    ShortcutHelp {
        id: sheet; keyBindings: root.keyBindings
        onAllShortcutsRequested: root.allRequested = true
        onClosed: root.contentItem.forceActiveFocus()
    }
})QML",QUrl("qrc:/shortcut-help-fixture.qml"));
        QVERIFY(!engine.rootObjects().isEmpty());
        auto *window=qobject_cast<QQuickWindow *>(engine.rootObjects().first()); QVERIFY(window);
        QVERIFY(QTest::qWaitForWindowExposed(window)); window->requestActivate(); QTRY_VERIFY(window->isActive());
        QTest::keyClick(window,Qt::Key_Slash,Qt::ControlModifier);
        QTRY_VERIFY(window->property("sheetOpen").toBool());
        QQuickItem *address=nullptr;
        QTRY_VERIFY((address=findItem(window->contentItem(),"shortcutHelpKey_address")));
        QCOMPARE(address->property("text").toString(),keys.nativeSequence(keys.bindings()["address"].toString()));
        int row=-1;
        for(int i=0;i<keys.rowCount();++i) if(keys.data(keys.index(i),Qt::UserRole)=="address") row=i;
        QVERIFY(row>=0); QVERIFY(keys.assign(row,Qt::Key_K,Qt::ControlModifier|Qt::ShiftModifier).isEmpty());
        QTRY_VERIFY(([&] { const auto *label=findItem(window->contentItem(),"shortcutHelpKey_address");
            return label && label->property("text").toString()==keys.nativeSequence("Ctrl+Shift+K"); })());
        window->resize(420,360);
        QTRY_VERIFY(window->property("sheetWidth").toInt()<=window->width()-32);
        QTRY_VERIFY(window->property("sheetHeight").toInt()<=window->height()-48);
        QTest::keyClick(window,Qt::Key_Escape); QTRY_VERIFY(!window->property("sheetOpen").toBool());
        QTRY_VERIFY(!window->property("sheetVisible").toBool());
        QTest::keyClick(window,Qt::Key_Slash,Qt::ControlModifier); QTRY_VERIFY(window->property("sheetOpen").toBool());
        auto *all=window->findChild<QObject *>("allShortcutsButton"); QVERIFY(all);
        QVERIFY(QMetaObject::invokeMethod(all,"clicked"));
        QTRY_VERIFY(window->property("allRequested").toBool()); QTRY_VERIFY(!window->property("sheetOpen").toBool());
    }
};
QTEST_MAIN(ShortcutHelpTest)
#include "shortcuthelp_test.moc"
