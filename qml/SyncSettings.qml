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
        text: qsTr("Connect your computers and phones to share one credential vault over local Wi-Fi. Connected devices exchange encrypted changes, including deletions. Keep Arbiter open and desktop sync unlocked; no cloud account is needed.")
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
            text: qsTr("Add a device")
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
            UiButton { text: qsTr("Copy pairing code for another computer"); onClicked: root.manager.copyPairingCode() }
            UiButton { text: qsTr("Cancel pairing"); onClicked: root.manager.cancelPairing() }
        }
        UiTextField {
            id: invitation
            Layout.fillWidth: true
            placeholderText: qsTr("Paste a pairing code from another computer")
            enabled: !!root.manager && !root.manager.busy
        }
        UiButton {
            text: qsTr("Review connection")
            enabled: invitation.text.trim().length > 0 && !!root.manager && !root.manager.busy
            onClicked: { root.manager.reviewPairing(invitation.text); invitation.clear() }
        }
        ColumnLayout {
            visible: !!root.manager && root.manager.pendingDevice.length > 0
            Layout.fillWidth: true
            Basic.Label {
                Layout.fillWidth: true; wrapMode: Text.WordWrap; color: Theme.text; textFormat: Text.PlainText
                text: qsTr("Connect to %1? This shares your vault with this device and the devices it syncs with.").arg(root.manager ? root.manager.pendingDevice : "")
            }
            Basic.Label { Layout.fillWidth: true; wrapMode: Text.WrapAnywhere; color: Theme.muted; text: root.manager ? root.manager.pendingPin : "" }
            RowLayout {
                UiButton { text: qsTr("Connect"); enabled: !!root.manager && !root.manager.busy; primary: true; onClicked: root.manager.confirmPairing() }
                UiButton { text: qsTr("Cancel"); onClicked: root.manager.cancelJoin() }
            }
        }
        UiButton { text: qsTr("Sync now"); iconName: "refresh-cw"; enabled: !!root.manager && !root.manager.busy; onClicked: root.manager.sync() }
        Basic.Label { text: qsTr("Paired devices"); color: Theme.text; font.pixelSize: 18; font.weight: Font.DemiBold }
        Basic.Label {
            visible: !!root.manager && root.manager.devices.length === 0
            text: qsTr("Your connected computers and phones will appear here."); color: Theme.muted
        }
        Repeater {
            model: root.manager ? root.manager.devices : []
            delegate: RowLayout {
                required property var modelData
                Layout.fillWidth: true; spacing: 12
                UiIcon { name: "smartphone"; color: Theme.muted; Layout.preferredWidth: 20; Layout.preferredHeight: 20 }
                ColumnLayout {
                    Layout.fillWidth: true
                    Basic.Label { textFormat: Text.PlainText; text: modelData.name; color: Theme.text; Layout.fillWidth: true; elide: Text.ElideRight }
                    Basic.Label {
                        text: modelData.lastSeen > 0 ? qsTr("Last synced %1").arg(Qt.formatDateTime(new Date(modelData.lastSeen), Qt.DefaultLocaleShortDate)) : qsTr("Not synced yet")
                        color: Theme.muted; font.pixelSize: 12
                    }
                    Basic.Label { visible: modelData.pending; text: qsTr("Changes waiting to sync"); color: Theme.muted; font.pixelSize: 12 }
                    Basic.Label { visible: Boolean(modelData.error); text: modelData.error || ""; color: Theme.muted; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                }
                UiIconButton {
                    iconName: "trash"; label: qsTr("Remove device")
                    enabled: !!root.manager && !root.manager.busy
                    onClicked: { disconnect.deviceId = modelData.id; disconnect.open() }
                }
            }
        }
        Basic.Label {
            visible: !!root.manager && root.manager.conflicts > 0
            text: qsTr("Conflicting changes were kept. Choose which version to use on your connected devices.")
            color: Theme.text; Layout.fillWidth: true; wrapMode: Text.WordWrap
        }
        Repeater {
            model: root.manager ? root.manager.conflictChoices : []
            delegate: ColumnLayout {
                required property var modelData
                Layout.fillWidth: true
                Basic.Label { Layout.fillWidth: true; wrapMode: Text.WordWrap; color: Theme.text; textFormat: Text.PlainText; text: (modelData.origin || modelData.rpId || "") + " · " + (modelData.username || "") }
                Basic.Label { Layout.fillWidth: true; wrapMode: Text.WrapAnywhere; color: Theme.muted; font.pixelSize: 12; textFormat: Text.PlainText; text: modelData.device }
                UiButton {
                    text: modelData.deleted ? qsTr("Delete everywhere") : qsTr("Keep this version")
                    enabled: !!root.manager && !root.manager.busy
                    onClicked: root.manager.resolveConflict(modelData.id, modelData.version, modelData.generation)
                }
            }
        }
    }
    UiPopup {
        id: disconnect
        property string deviceId: ""
        anchors.centerIn: parent
        width: Math.min(420, root.width)
        padding: 20
        modal: true
        contentItem: ColumnLayout {
            spacing: 16
            Basic.Label { Layout.fillWidth: true; wrapMode: Text.WordWrap; color: Theme.text; text: qsTr("Remove this sync connection? Saved credentials are kept. Other connections to this device are unchanged.") }
            UiButton { text: qsTr("Remove device"); onClicked: { root.manager.revoke(disconnect.deviceId); disconnect.close() } }
            UiButton { text: qsTr("Cancel"); onClicked: disconnect.close() }
        }
    }
}
