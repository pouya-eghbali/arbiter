pragma ComponentBehavior: Bound
import QtQuick
import Arbiter
import QtQuick.Layouts
import QtQuick.Controls.Basic as Basic

ColumnLayout {
    id: root
    required property var manager
    spacing: 20
    Basic.Label { text: qsTr("Bring your vault along"); color: Theme.text; font.pixelSize: 22; font.weight: Font.DemiBold }
    Basic.Label {
        Layout.fillWidth: true
        text: qsTr("Enable sync once to share passwords and passkeys between paired devices over local Wi-Fi. Sync starts automatically whenever Arbiter opens and keeps running until you turn it off. No separate vault, passphrase, or cloud account is needed.")
        color: Theme.muted; wrapMode: Text.WordWrap
    }
    Basic.Label {
        visible: !root.manager
        text: qsTr("Open Sync in a regular browser window to pair your devices.")
        color: Theme.muted; Layout.fillWidth: true; wrapMode: Text.WordWrap
    }
    ColumnLayout {
        visible: !!root.manager && !root.manager.unlocked
        Layout.fillWidth: true; spacing: 12
        UiButton {
            id: enableSync
            text: root.manager && root.manager.enabled ? qsTr("Retry sync") : qsTr("Enable sync")
            primary: true
            enabled: !!root.manager && !root.manager.busy
            onClicked: root.manager.setEnabled(true)
        }
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
            Basic.Label { text: qsTr("Sync is enabled"); color: Theme.text; font.weight: Font.DemiBold }
            Item { Layout.fillWidth: true }
            UiButton { text: qsTr("Turn off sync"); onClicked: root.manager.setEnabled(false) }
        }
        Basic.Label {
            Layout.fillWidth: true; wrapMode: Text.WordWrap; color: Theme.muted
            text: qsTr("Sync continues while Arbiter is open, including when this computer is locked. Sleeping or offline devices reconnect when available. Password filling and passkey sign-in keep their own verification requirements.")
        }
        UiButton {
            text: qsTr("Add a device")
            primary: true
            enabled: !!root.manager && !root.manager.busy
            onClicked: root.manager.pair()
        }
        GridLayout {
            visible: !!root.manager && root.manager.pairingImage.length > 0
            columns: root.width > 640 ? 2 : 1
            columnSpacing: 24; rowSpacing: 16; Layout.fillWidth: true
            Rectangle {
                Layout.preferredWidth: 260; Layout.preferredHeight: 260
                color: "white"; radius: 18
                Image { anchors.fill: parent; anchors.margins: 12; source: root.manager ? root.manager.pairingImage : ""; smooth: false; fillMode: Image.PreserveAspectFit }
            }
            ColumnLayout {
                Layout.fillWidth: true; spacing: 16
                Basic.Label {
                    Layout.fillWidth: true; wrapMode: Text.WordWrap; color: Theme.muted
                    text: qsTr("Scan with Arbiter or your phone’s Camera app. Review the device before connecting. This invitation grants access to your vault, expires in two minutes, and works once.")
                }
                RowLayout {
                    UiIconButton { iconName: "copy"; label: qsTr("Copy pairing code for another computer"); onClicked: root.manager.copyPairingCode() }
                    UiIconButton { iconName: "x"; label: qsTr("Cancel pairing"); onClicked: root.manager.cancelPairing() }
                }
            }
        }
        RowLayout {
            Layout.fillWidth: true; spacing: 10
            UiTextField { id: invitation; Layout.fillWidth: true; placeholderText: qsTr("Paste a pairing code from another computer"); enabled: !!root.manager && !root.manager.busy }
            UiButton { text: qsTr("Review connection"); enabled: invitation.text.trim().length > 0 && !!root.manager && !root.manager.busy; onClicked: { root.manager.reviewPairing(invitation.text); invitation.clear() } }
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
