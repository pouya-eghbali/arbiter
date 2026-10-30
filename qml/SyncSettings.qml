pragma ComponentBehavior: Bound
import QtQuick
import Arbiter
import QtQuick.Layouts
import QtQuick.Controls.Basic as Basic

ColumnLayout {
    id: root
    required property var manager
    required property var passkeys
    signal passkeySettingsRequested()
    spacing: 20
    onVisibleChanged: { if (!visible) phrase.clear() }
    Basic.Label { text: qsTr("Bring your vault along"); color: Theme.text; font.pixelSize: 26; font.weight: Font.DemiBold }
    Basic.Label {
        Layout.fillWidth: true
        text: qsTr("Pair Arbiter on your phone to sync your saved passwords and Arbiter passkeys directly over your local Wi-Fi. Keep both apps open and unlock the desktop vault. Your credentials stay encrypted in transit and on each device; no cloud account is needed.")
        color: Theme.muted; wrapMode: Text.WordWrap
    }
    Basic.Label {
        visible: !root.manager
        text: qsTr("Open Sync in a regular browser window to pair your devices.")
        color: Theme.muted; Layout.fillWidth: true; wrapMode: Text.WordWrap
    }
    ColumnLayout {
        visible: !!root.manager && !root.passkeys.configured
        Layout.fillWidth: true; spacing: 12
        Basic.Label {
            text: qsTr("Set up your Arbiter passkey vault first. Its passphrase or biometric verification will also unlock your sync session.")
            color: Theme.muted; Layout.fillWidth: true; wrapMode: Text.WordWrap
        }
        UiButton { text: qsTr("Set up passkey vault"); onClicked: root.passkeySettingsRequested() }
    }
    ColumnLayout {
        visible: !!root.manager && root.passkeys.configured && !root.manager.unlocked
        Layout.fillWidth: true; spacing: 12
        UiTextField {
            id: phrase
            Layout.fillWidth: true; Layout.maximumWidth: 420
            placeholderText: qsTr("Vault passphrase")
            echoMode: TextInput.Password
            enabled: !!root.manager && !root.manager.busy
            onAccepted: unlock.clicked()
        }
        RowLayout {
            spacing: 10
            UiButton {
                id: unlock
                text: qsTr("Unlock sync")
                enabled: !!root.manager && !root.manager.busy && phrase.text.length > 0
                onClicked: { root.manager.unlock(phrase.text, false); phrase.clear() }
            }
            UiButton {
                visible: root.passkeys.biometricReady
                text: qsTr("Unlock with %1").arg(root.passkeys.biometricName)
                enabled: !!root.manager && !root.manager.busy
                onClicked: root.manager.unlock("", true)
            }
        }
    }
    UiCheckBox {
        visible: !!root.manager
        text: qsTr("Keep sync unlocked until this computer locks")
        checked: root.manager ? root.manager.keepUnlocked : false
        onToggled: if(root.manager) root.manager.keepUnlocked=checked
    }
    Basic.Label {
        visible: !!root.manager
        Layout.fillWidth: true; wrapMode: Text.WordWrap; color: Theme.muted
        text: qsTr("Optional for phone background sync. Paired devices can sync while this computer stays unlocked. Locking the computer or quitting Arbiter always locks sync.")
    }
    RowLayout {
        visible: !!root.manager && root.manager.busy
        spacing: 12
        Basic.BusyIndicator { running: visible; Layout.preferredWidth: 24; Layout.preferredHeight: 24 }
        Basic.Label { text: qsTr("Working with your secure vault…"); color: Theme.muted }
    }
    Basic.Label {
        visible: !!root.manager && root.manager.error.length > 0
        text: root.manager ? root.manager.error : ""
        color: Theme.text; Layout.fillWidth: true; wrapMode: Text.WordWrap
    }
    ColumnLayout {
        visible: !!root.manager && root.manager.unlocked
        Layout.fillWidth: true; spacing: 16
        RowLayout {
            spacing: 10
            UiIcon { name: "shield-check"; color: Theme.accent; Layout.preferredWidth: 22; Layout.preferredHeight: 22 }
            Basic.Label { text: qsTr("Sync is unlocked"); color: Theme.text; font.weight: Font.DemiBold }
            Item { Layout.fillWidth: true }
            UiButton { text: qsTr("Lock"); onClicked: root.manager.lock() }
        }
        Basic.Label {
            Layout.fillWidth: true; wrapMode: Text.WordWrap; color: Theme.muted
            text: root.manager && root.manager.keepUnlocked
                ? qsTr("This session stays unlocked until you lock this computer, lock sync, or quit Arbiter.")
                : qsTr("This session locks after 10 minutes or when your computer locks. Keep Arbiter open on both devices while syncing.")
        }
        UiButton {
            text: qsTr("Pair a phone")
            primary: true
            enabled: !!root.manager && !root.manager.busy
            onClicked: root.manager.pair()
        }
        ColumnLayout {
            visible: !!root.manager && root.manager.pairingImage.length > 0
            spacing: 12; Layout.fillWidth: true
            Rectangle {
                Layout.preferredWidth: 280; Layout.preferredHeight: 280
                color: "white"; radius: 18
                Image { anchors.fill: parent; anchors.margins: 10; source: root.manager ? root.manager.pairingImage : ""; smooth: false; fillMode: Image.PreserveAspectFit }
            }
            Basic.Label {
                Layout.fillWidth: true; wrapMode: Text.WordWrap; color: Theme.muted
                text: qsTr("On your phone, open Passwords & passkeys → Scan desktop code, then scan this code. It grants access to this vault. The invitation expires in two minutes and can be used once.")
            }
            UiButton { text: qsTr("Cancel pairing"); onClicked: root.manager.cancelPairing() }
        }
        Basic.Label { text: qsTr("Paired devices"); color: Theme.text; font.pixelSize: 18; font.weight: Font.DemiBold }
        Basic.Label {
            visible: !!root.manager && root.manager.devices.length === 0
            text: qsTr("Your paired phones will appear here."); color: Theme.muted
        }
        Repeater {
            model: root.manager ? root.manager.devices : []
            delegate: RowLayout {
                required property var modelData
                Layout.fillWidth: true; spacing: 12
                UiIcon { name: "smartphone"; color: Theme.muted; Layout.preferredWidth: 20; Layout.preferredHeight: 20 }
                Basic.Label { text: modelData.name; color: Theme.text; Layout.fillWidth: true; elide: Text.ElideRight }
                UiIconButton {
                    iconName: "trash"; label: qsTr("Remove device")
                    onClicked: root.manager.revoke(modelData.id)
                }
            }
        }
        Basic.Label {
            visible: !!root.manager && root.manager.conflicts > 0
            text: qsTr("%n credential conflict(s) need a choice in your phone’s vault. Both versions have been kept.", "", root.manager ? root.manager.conflicts : 0)
            color: Theme.text; Layout.fillWidth: true; wrapMode: Text.WordWrap
        }
    }
}
