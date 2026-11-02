pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic as Basic

ColumnLayout {
    id: root
    required property var manager
    signal manageRequested()
    spacing: 0
    UiSettingsRow {
        Layout.fillWidth: true
        title: qsTr("Saved passwords")
        description: qsTr("%1 saved logins, protected by your operating system.").arg(root.manager.entries.length)
        UiButton { text: qsTr("Manage saved passwords…"); iconName: "key-round"; onClicked: root.manageRequested() }
    }
    UiSettingsRow {
        Layout.fillWidth: true
        title: qsTr("Offer to save and fill passwords")
        description: qsTr("Save new logins and suggest matching accounts on websites.")
        UiCheckBox { checked: root.manager.enabled; Accessible.name: qsTr("Offer to save and fill passwords"); onToggled: root.manager.enabled = checked }
    }
    UiSettingsRow {
        Layout.fillWidth: true
        title: qsTr("Verify my identity before filling saved passwords")
        description: qsTr("Use device authentication before filling a saved login.")
        UiCheckBox { objectName: "passwordBiometricPreference"; checked: root.manager.biometricEnabled; enabled: root.manager.enabled; Accessible.name: qsTr("Verify my identity before filling saved passwords"); onToggled: root.manager.biometricEnabled = checked }
    }
    UiSettingsRow {
        Layout.fillWidth: true
        title: qsTr("Ask me to verify again")
        description: qsTr("Locking, sleeping, or restarting clears the remembered verification.")
        UiComboBox {
            objectName: "passwordVerificationCooldown"
            readonly property var durations: [0, 60, 300, 900, 1800, 3600]
            model: [qsTr("Every time"), qsTr("After 1 minute"), qsTr("After 5 minutes"), qsTr("After 15 minutes"), qsTr("After 30 minutes"), qsTr("After 1 hour")]
            currentIndex: durations.indexOf(root.manager.verificationCooldown)
            displayText: currentIndex < 0 ? qsTr("After %1 seconds").arg(root.manager.verificationCooldown) : currentText
            enabled: root.manager.enabled && root.manager.biometricEnabled
            implicitWidth: 220
            Accessible.name: qsTr("Ask me to verify again")
            onActivated: root.manager.verificationCooldown = durations[currentIndex]
        }
    }
}
