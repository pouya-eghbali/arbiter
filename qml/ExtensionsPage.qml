pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic as Basic
import QtQuick.Dialogs as Dialogs
import Arbiter

ColumnLayout {
    id: page
    objectName: "extensionsSettingsPage"
    required property var controller
    signal pageRequested(string extensionId, bool options)
    signal browseRequested()
    property string removingId: ""
    spacing: 16
    onControllerChanged: removingId = ""
    Basic.Label {
        Layout.fillWidth: true
        text: page.controller && page.controller.available
            ? qsTr("Extensions apply to the focused view’s browser profile, including workspaces that share it. Install from the Chrome Web Store, a Manifest V3 ZIP, or an unpacked folder. Some Chrome APIs remain unavailable; installation does not guarantee compatibility. Automatic updates are not supported yet.")
            : qsTr("Extensions are unavailable in private windows.")
        color: Theme.muted
        wrapMode: Text.Wrap
    }
    ColumnLayout {
        Layout.fillWidth: true
        visible: page.controller && page.controller.available
        spacing: 8
        Basic.Label { text: qsTr("Chrome Web Store"); color: Theme.text; font.weight: Font.DemiBold }
        UiTextField {
            id: storeLink
            objectName: "extensionStoreLink"
            Layout.fillWidth: true
            placeholderText: qsTr("Paste an extension’s store link or ID")
            Accessible.name: placeholderText
            enabled: page.controller && !page.controller.busy && !page.controller.review.id
            onAccepted: if (storeInstall.enabled) page.controller.installFromStore(text)
        }
        RowLayout {
            UiButton {
                id: storeInstall
                objectName: "extensionStoreInstall"
                text: qsTr("Install from store")
                primary: true
                enabled: page.controller && !page.controller.busy && !page.controller.review.id && storeLink.text.trim().length > 0
                onClicked: page.controller.installFromStore(storeLink.text)
            }
            UiButton { text: qsTr("Browse store…"); enabled: page.controller && !page.controller.busy; onClicked: page.browseRequested() }
        }
        Basic.ProgressBar {
            id: storeProgress
            Layout.fillWidth: true
            visible: page.controller && page.controller.canCancel && page.controller.progress >= 0
            value: page.controller ? page.controller.progress : 0
            background: Rectangle { implicitHeight: 4; radius: 2; color: Theme.border }
            contentItem: Item {
                implicitHeight: 4
                Rectangle { width: parent.width * storeProgress.visualPosition; height: parent.height; radius: 2; color: Theme.accent }
            }
            Accessible.name: qsTr("Extension installation progress")
        }
        UiButton { objectName: "cancelStoreInstallButton"; visible: page.controller && page.controller.canCancel; text: qsTr("Cancel installation"); onClicked: page.controller.cancelInstall() }
    }
    RowLayout {
        visible: page.controller && page.controller.available
        spacing: 8
        UiButton { text: qsTr("Install ZIP…"); enabled: page.controller && !page.controller.busy && !page.controller.review.id; onClicked: zipDialog.open() }
        UiButton { text: qsTr("Install folder…"); enabled: page.controller && !page.controller.busy && !page.controller.review.id; onClicked: folderDialog.open() }
        Basic.BusyIndicator { visible: page.controller && page.controller.busy; running: visible; implicitWidth: 28; implicitHeight: 28 }
    }
    Dialogs.FileDialog {
        id: zipDialog
        title: qsTr("Install Chrome extension")
        nameFilters: [qsTr("Extension ZIP (*.zip)")]
        onAccepted: page.controller.install(selectedFile)
    }
    Dialogs.FolderDialog { id: folderDialog; title: qsTr("Choose extension folder containing manifest.json"); onAccepted: page.controller.install(selectedFolder) }
    Basic.Label {
        Layout.fillWidth: true
        visible: page.controller && page.controller.status.length > 0
        text: page.controller ? page.controller.status : ""
        textFormat: Text.PlainText
        wrapMode: Text.Wrap
        color: Theme.muted
    }
    Basic.Label {
        visible: page.controller && page.controller.available && page.controller.items.length === 0 && !page.controller.busy
        text: qsTr("No extensions installed in this profile yet.")
        color: Theme.muted
        Layout.fillWidth: true
    }
    Repeater {
        model: page.controller ? page.controller.items : []
        delegate: Rectangle {
            id: card
            required property var modelData
            Layout.fillWidth: true
            implicitHeight: details.implicitHeight + 32
            radius: 16
            color: Theme.field
            ColumnLayout {
                id: details
                anchors.fill: parent
                anchors.margins: 16
                spacing: 10
                RowLayout {
                    Layout.fillWidth: true
                    Basic.Label {
                        Layout.fillWidth: true
                        text: card.modelData.name
                        textFormat: Text.PlainText
                        color: Theme.text
                        font.pixelSize: 18
                        font.weight: Font.DemiBold
                        elide: Text.ElideRight
                    }
                    UiCheckBox {
                        text: qsTr("Enabled")
                        checked: card.modelData.enabled
                        enabled: !page.controller.busy
                        onToggled: if (card.modelData.enabled) page.controller.disable(card.modelData.id); else page.controller.requestEnable(card.modelData.id)
                    }
                }
                Basic.Label { Layout.fillWidth: true; text: card.modelData.description; textFormat: Text.PlainText; color: Theme.muted; wrapMode: Text.Wrap; visible: text.length > 0 }
                Basic.Label { Layout.fillWidth: true; text: qsTr("Version %1 · %2").arg(card.modelData.version).arg(card.modelData.id); textFormat: Text.PlainText; color: Theme.muted; wrapMode: Text.WrapAnywhere; font.pixelSize: 12 }
                RowLayout {
                    UiButton { visible: card.modelData.hasPopup; enabled: card.modelData.enabled && !page.controller.busy; text: qsTr("Open"); onClicked: page.pageRequested(card.modelData.id, false) }
                    UiButton { visible: card.modelData.hasOptions; enabled: card.modelData.enabled && !page.controller.busy; text: qsTr("Options"); onClicked: page.pageRequested(card.modelData.id, true) }
                    UiButton { text: qsTr("Remove…"); enabled: !page.controller.busy; onClicked: page.removingId = card.modelData.id }
                }
                RowLayout {
                    visible: page.removingId === card.modelData.id
                    Basic.Label { text: qsTr("Remove this extension?"); color: Theme.text }
                    UiButton { text: qsTr("Cancel"); onClicked: page.removingId = "" }
                    UiButton { text: qsTr("Remove"); onClicked: { page.controller.remove(card.modelData.id); page.removingId = "" } }
                }
            }
        }
    }
}
