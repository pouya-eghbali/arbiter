#include "../src/keybindings.h"
#include "../src/settings.h"
#include <QSet>
#include <QTemporaryDir>
#include <QtTest>

class KeyBindingsTest : public QObject
{
    Q_OBJECT
    QTemporaryDir directory;
private slots:
    void crossWorkspaceSplitShortcutIsCustomizable() {
        Settings().clear(); KeyBindings keys;
        QVERIFY(!keys.bindings().value("splitWith").toString().isEmpty());
        int row=-1;
        for(int i=0;i<keys.rowCount();++i) if(keys.data(keys.index(i),Qt::UserRole)=="splitWith") row=i;
        QVERIFY(row>=0); QVERIFY(keys.assign(row,Qt::Key_F10,Qt::AltModifier).isEmpty());
        KeyBindings restored; QCOMPARE(restored.bindings().value("splitWith"),keys.bindings().value("splitWith"));
    }
    void initTestCase() {
        QVERIFY(directory.isValid());
        QCoreApplication::setOrganizationName("ArbiterTests");
        QCoreApplication::setApplicationName("KeyBindings");
        qputenv("XDG_CONFIG_HOME", directory.path().toUtf8());
        qputenv("XDG_CONFIG_DIRS", directory.path().toUtf8());
    }
    void init() { Settings().clear(); }
    void rejectsConflicts() {
        KeyBindings keys;
        const auto before = keys.bindings();
        const auto settingsKey = QKeySequence::fromString(before["settings"].toString(), QKeySequence::PortableText)[0];
        QVERIFY(!keys.assign(0, settingsKey.key(), settingsKey.keyboardModifiers()).isEmpty());
        QCOMPARE(keys.bindings(), before);
    }
    void protectsTyping() {
        KeyBindings keys;
        const auto before = keys.bindings();
        QVERIFY(!keys.assign(0, Qt::Key_A, Qt::NoModifier).isEmpty());
        QVERIFY(!keys.assign(0, Qt::Key_Control, Qt::ControlModifier).isEmpty());
        QCOMPARE(keys.bindings(), before);
    }
    void persistsAndResets() {
        KeyBindings keys;
        const auto defaults = keys.bindings();
        QVERIFY(keys.assign(0, Qt::Key_K, Qt::ControlModifier | Qt::ShiftModifier).isEmpty());
        KeyBindings restored;
        QCOMPARE(restored.bindings()["address"].toString(), QString("Ctrl+Shift+K"));
        restored.resetDefaults();
        KeyBindings reset;
        QCOMPARE(reset.bindings(), defaults);
    }
    void recoversFromDuplicateSavedBindings() {
        Settings().setValue("KeyboardBindings/address", "Ctrl+,");
        KeyBindings keys;
        QCOMPARE(keys.bindings()["address"].toString(), QString("Ctrl+L"));
    }
    void navigationBindingsPersistIndependently() {
        KeyBindings keys;
        const QStringList actions{"back", "forward", "pageTop", "pageBottom"};
        for(int i=0;i<actions.size();++i) {
            int row=-1;
            for(int j=0;j<keys.rowCount();++j)
                if(keys.data(keys.index(j),Qt::UserRole).toString()==actions[i]) row=j;
            QVERIFY(row>=0);
            QVERIFY(keys.assign(row,Qt::Key_F6+i,Qt::AltModifier).isEmpty());
        }
        KeyBindings restored;
        QCOMPARE(restored.bindings(),keys.bindings());
        // Introducing new page actions must not replace saved navigation keys.
        Settings().remove("KeyboardBindings/pageTop");
        Settings().remove("KeyboardBindings/pageBottom");
        KeyBindings upgraded;
        QCOMPARE(upgraded.bindings()["back"],keys.bindings()["back"]);
        QCOMPARE(upgraded.bindings()["forward"],keys.bindings()["forward"]);
        QVERIFY(!upgraded.bindings()["pageTop"].toString().isEmpty());
        QVERIFY(!upgraded.bindings()["pageBottom"].toString().isEmpty());
        QSet<QString> unique;
        for(const auto &sequence:upgraded.bindings()) unique.insert(sequence.toString());
        QCOMPARE(unique.size(),upgraded.rowCount());
    }
};
QTEST_MAIN(KeyBindingsTest)
#include "keybindings_test.moc"
