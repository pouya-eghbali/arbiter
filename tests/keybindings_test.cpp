#include "../src/keybindings.h"
#include <QSettings>
#include <QTemporaryDir>
#include <QtTest>

class KeyBindingsTest : public QObject
{
    Q_OBJECT
    QTemporaryDir directory;
private slots:
    void initTestCase() {
        QVERIFY(directory.isValid());
        QCoreApplication::setOrganizationName("ArbiterTests");
        QCoreApplication::setApplicationName("KeyBindings");
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, directory.path());
    }
    void init() { QSettings().clear(); }
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
        QSettings().setValue("KeyboardBindings/address", "Ctrl+,");
        KeyBindings keys;
        QCOMPARE(keys.bindings()["address"].toString(), QString("Ctrl+L"));
    }
};
QTEST_MAIN(KeyBindingsTest)
#include "keybindings_test.moc"
