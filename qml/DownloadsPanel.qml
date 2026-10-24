pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic as Basic
import Arbiter

Basic.Popup {
    id: panel
    required property var manager
    function bytes(value: real): string {
        if (value < 0) return qsTr("Unknown size")
        if (value < 1024) return qsTr("%1 B").arg(Math.round(value))
        if (value < 1024 * 1024) return qsTr("%1 KB").arg((value / 1024).toFixed(1))
        if (value < 1024 * 1024 * 1024) return qsTr("%1 MB").arg((value / 1024 / 1024).toFixed(1))
        return qsTr("%1 GB").arg((value / 1024 / 1024 / 1024).toFixed(1))
    }
    parent: Basic.Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(760, parent.width - 32)
    height: Math.min(640, parent.height - 48)
    padding: 24
    modal: true
    dim: true
    focus: true
    closePolicy: Basic.Popup.CloseOnEscape | Basic.Popup.CloseOnPressOutside
    background: GlassSurface { dense: true; radius: 24 }
    Basic.Overlay.modal: Rectangle { color: Theme.scrim }
    contentItem: ColumnLayout {
        spacing: 12
        RowLayout {
            Layout.fillWidth: true
            Basic.Label { Layout.fillWidth: true; text: qsTr("Downloads"); font.pixelSize: 26; font.weight: Font.DemiBold; color: Theme.text }
            UiButton { text: qsTr("Clear finished"); onClicked: panel.manager.clearFinished() }
            UiButton { text: qsTr("Close"); onClicked: panel.close() }
        }
        Basic.Label {
            Layout.fillWidth: true
            visible: panel.manager.privateMode
            text: qsTr("This list is private to this window. Saved files remain on your device.")
            color: Theme.muted
            wrapMode: Text.Wrap
        }
        Basic.Label { Layout.fillWidth: true; visible: text.length > 0; text: panel.manager.error; textFormat: Text.PlainText; color: Theme.muted; wrapMode: Text.Wrap }
        Basic.ScrollView {
            id: scroll
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentWidth: availableWidth
            ColumnLayout {
                width: scroll.availableWidth
                spacing: 10
                Basic.Label { Layout.fillWidth: true; visible: panel.manager.items.length === 0; text: qsTr("No downloads yet."); color: Theme.muted; wrapMode: Text.Wrap }
                Repeater {
                    model: panel.manager.items
                    delegate: Rectangle {
                        id: card
                        required property var modelData
                        Layout.fillWidth: true
                        implicitHeight: entry.implicitHeight + 28
                        radius: 14
                        color: Theme.field
                        ColumnLayout {
                            id: entry
                            anchors.fill: parent
                            anchors.margins: 14
                            spacing: 8
                            Basic.Label { Layout.fillWidth: true; text: card.modelData.fileName; textFormat: Text.PlainText; font.weight: Font.DemiBold; color: Theme.text; elide: Text.ElideMiddle }
                            Basic.Label { Layout.fillWidth: true; text: card.modelData.source; textFormat: Text.PlainText; color: Theme.muted; elide: Text.ElideMiddle; font.pixelSize: 12 }
                            Basic.Label { Layout.fillWidth: true; text: qsTr("%1 · %2%3").arg(card.modelData.status).arg(panel.bytes(card.modelData.received)).arg(card.modelData.total >= 0 ? " / " + panel.bytes(card.modelData.total) : ""); color: Theme.muted; wrapMode: Text.Wrap }
                            Basic.ProgressBar {
                                id: progress
                                Layout.fillWidth: true
                                visible: card.modelData.active && card.modelData.total > 0
                                from: 0
                                to: Math.max(1, card.modelData.total)
                                value: card.modelData.received
                                background: Rectangle { implicitHeight: 4; radius: 2; color: Theme.border }
                                contentItem: Item { implicitHeight: 4; Rectangle { width: parent.width * progress.visualPosition; height: parent.height; color: Theme.accent; radius: 2 } }
                                Accessible.name: qsTr("Download progress")
                            }
                            Basic.Label { Layout.fillWidth: true; visible: text.length > 0; text: card.modelData.error; textFormat: Text.PlainText; color: Theme.muted; wrapMode: Text.Wrap }
                            Basic.Label { Layout.fillWidth: true; visible: text.length > 0; text: card.modelData.warning; textFormat: Text.PlainText; color: Theme.muted; wrapMode: Text.Wrap }
                            Flow {
                                Layout.fillWidth: true
                                spacing: 6
                                UiButton { visible: card.modelData.active; text: card.modelData.paused ? qsTr("Resume") : qsTr("Pause"); onClicked: if (card.modelData.paused) panel.manager.resume(card.modelData.id); else panel.manager.pause(card.modelData.id) }
                                UiButton { visible: card.modelData.active; text: qsTr("Cancel"); onClicked: panel.manager.cancel(card.modelData.id) }
                                UiButton { visible: card.modelData.canRetry; text: qsTr("Retry"); onClicked: panel.manager.retry(card.modelData.id) }
                                UiButton { visible: card.modelData.completed; text: qsTr("Open"); onClicked: panel.manager.open(card.modelData.id) }
                                UiButton { visible: card.modelData.path.length > 0; text: qsTr("Show in folder"); onClicked: panel.manager.showFolder(card.modelData.id) }
                            }
                        }
                    }
                }
            }
        }
        Basic.Label { text: qsTr("%1 active").arg(panel.manager.activeCount); color: Theme.muted }
    }
}
