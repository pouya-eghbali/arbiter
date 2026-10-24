pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic as Basic
import Arbiter

Basic.Popup {
    popupType: Basic.Popup.Item
    id: panel
    required property var manager
    function bytes(value: real): string {
        if (value < 0) return qsTr("Unknown size")
        if (value < 1024) return qsTr("%1 B").arg(Math.round(value))
        if (value < 1024 * 1024) return qsTr("%1 KB").arg((value / 1024).toFixed(1))
        if (value < 1024 * 1024 * 1024) return qsTr("%1 MB").arg((value / 1024 / 1024).toFixed(1))
        return qsTr("%1 GB").arg((value / 1024 / 1024 / 1024).toFixed(1))
    }
    function remaining(seconds: real): string {
        if (seconds < 0) return qsTr("Estimating…")
        if (seconds < 1) return qsTr("Finishing…")
        if (seconds < 60) return qsTr("%1s left").arg(Math.ceil(seconds))
        if (seconds < 3600) return qsTr("%1m %2s left").arg(Math.floor(seconds / 60)).arg(Math.floor(seconds % 60))
        return qsTr("%1h %2m left").arg(Math.floor(seconds / 3600)).arg(Math.floor(seconds % 3600 / 60))
    }
    parent: Basic.Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(760, parent.width - 32)
    height: Math.min(600, parent.height - 48, Math.max(220,
        topPadding + bottomPadding + heading.implicitHeight + footer.implicitHeight
        + (privateNote.visible ? privateNote.implicitHeight + downloadsContent.spacing : 0)
        + (errorNote.visible ? errorNote.implicitHeight + downloadsContent.spacing : 0)
        + downloadsContent.spacing * 2 + downloadRows.implicitHeight))
    padding: 18
    modal: true
    dim: true
    focus: true
    closePolicy: Basic.Popup.CloseOnEscape | Basic.Popup.CloseOnPressOutside
    background: GlassSurface { dense: true; radius: Theme.panelRadius }
    Basic.Overlay.modal: Rectangle { color: Theme.scrim }
    contentItem: ColumnLayout {
        id: downloadsContent
        spacing: 12
        RowLayout {
            id: heading
            Layout.fillWidth: true
            Basic.Label { Layout.fillWidth: true; text: qsTr("Downloads"); font.pixelSize: 22; font.weight: Font.DemiBold; color: Theme.text }
            UiIconButton { label: qsTr("Clear finished"); iconName: "list-x"; enabled: panel.manager.items.some(item => !item.active); onClicked: panel.manager.clearFinished() }
            UiIconButton { label: qsTr("Close downloads"); iconName: "x"; onClicked: panel.close() }
        }
        Basic.Label {
            id: privateNote
            Layout.fillWidth: true
            visible: panel.manager.privateMode
            text: qsTr("This list is private to this window. Saved files remain on your device.")
            color: Theme.muted
            wrapMode: Text.Wrap
        }
        Basic.Label { id: errorNote; Layout.fillWidth: true; visible: text.length > 0; text: panel.manager.error; textFormat: Text.PlainText; color: Theme.muted; wrapMode: Text.Wrap }
        Basic.ScrollView {
            id: scroll
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentWidth: availableWidth
            ColumnLayout {
                id: downloadRows
                width: scroll.availableWidth
                spacing: 6
                Basic.Label { Layout.fillWidth: true; visible: panel.manager.items.length === 0; text: qsTr("No downloads yet."); color: Theme.muted; wrapMode: Text.Wrap }
                Repeater {
                    model: panel.manager.items
                    delegate: Rectangle {
                        id: card
                        required property var modelData
                        Layout.fillWidth: true
                        objectName: "downloadRow"
                        implicitHeight: entry.implicitHeight + 20
                        radius: 12
                        color: Theme.field
                        RowLayout {
                            id: entry
                            anchors.fill: parent
                            anchors.margins: 10
                            spacing: 10
                            Image {
                                objectName: "downloadFileIcon"
                                Layout.preferredWidth: 36
                                Layout.preferredHeight: 36
                                Layout.alignment: Qt.AlignTop
                                Layout.topMargin: 2
                                source: card.modelData.iconSource
                                sourceSize: Qt.size(96, 96)
                                fillMode: Image.PreserveAspectFit
                                Accessible.ignored: true
                            }
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 4
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 8
                                    Basic.Label {
                                        Layout.fillWidth: true
                                        text: card.modelData.fileName
                                        textFormat: Text.PlainText
                                        font.pixelSize: 13
                                        font.weight: Font.DemiBold
                                        color: Theme.text
                                        elide: Text.ElideMiddle
                                    }
                                    RowLayout {
                                        spacing: 2
                                        UiIconButton {
                                            visible: card.modelData.active
                                            label: card.modelData.paused ? qsTr("Resume download") : qsTr("Pause download")
                                            iconName: card.modelData.paused ? "play" : "pause"
                                            onClicked: if (card.modelData.paused) panel.manager.resume(card.modelData.id); else panel.manager.pause(card.modelData.id)
                                        }
                                        UiIconButton {
                                            visible: card.modelData.active
                                            label: qsTr("Cancel download")
                                            iconName: "x"
                                            onClicked: panel.manager.cancel(card.modelData.id)
                                        }
                                        UiIconButton {
                                            visible: card.modelData.canRetry
                                            label: qsTr("Retry download")
                                            iconName: "rotate-cw"
                                            onClicked: panel.manager.retry(card.modelData.id)
                                        }
                                        UiIconButton {
                                            visible: card.modelData.completed
                                            label: qsTr("Open file")
                                            iconName: "external-link"
                                            onClicked: panel.manager.open(card.modelData.id)
                                        }
                                        UiIconButton {
                                            visible: card.modelData.path.length > 0
                                            label: qsTr("Show in folder")
                                            iconName: "folder-open"
                                            onClicked: panel.manager.showFolder(card.modelData.id)
                                        }
                                    }
                                }
                                Basic.Label {
                                    Layout.fillWidth: true
                                    text: card.modelData.source
                                    textFormat: Text.PlainText
                                    color: Theme.muted
                                    elide: Text.ElideMiddle
                                    font.pixelSize: 11
                                }
                                Basic.Label {
                                    objectName: "downloadTransferDetails"
                                    Layout.fillWidth: true
                                    text: {
                                        const item = card.modelData
                                        let detail = item.status + " · " + panel.bytes(item.received)
                                        if (item.total >= 0) detail += " / " + panel.bytes(item.total)
                                        if (item.active && !item.paused) {
                                            detail += " · " + panel.bytes(item.bytesPerSecond) + "/s"
                                            if (item.total >= 0) detail += " · " + panel.remaining(item.secondsRemaining)
                                        }
                                        return detail
                                    }
                                    color: Theme.muted
                                    font.pixelSize: 11
                                    wrapMode: Text.Wrap
                                }
                                Basic.ProgressBar {
                                    id: progress
                                    Layout.fillWidth: true
                                    visible: card.modelData.active
                                    indeterminate: card.modelData.total <= 0 && !card.modelData.paused
                                    from: 0
                                    to: Math.max(1, card.modelData.total)
                                    value: card.modelData.received
                                    background: Rectangle { implicitHeight: 3; radius: 1.5; color: Theme.border }
                                    contentItem: Item {
                                        implicitHeight: 3
                                        clip: true
                                        Rectangle {
                                            width: progress.indeterminate ? parent.width / 3 : parent.width * progress.visualPosition
                                            height: parent.height
                                            color: Theme.accent
                                            radius: 1.5
                                            SequentialAnimation on x {
                                                running: progress.visible && progress.indeterminate
                                                loops: Animation.Infinite
                                                NumberAnimation { from: -progress.width / 3; to: progress.width; duration: 1300 }
                                            }
                                            // Restore determinate positioning when the total becomes known.
                                            Binding on x { when: !progress.indeterminate; value: 0 }
                                        }
                                    }
                                    Accessible.name: qsTr("Download progress")
                                }
                                Basic.Label { Layout.fillWidth: true; visible: text.length > 0; text: card.modelData.error; textFormat: Text.PlainText; font.pixelSize: 11; color: Theme.muted; wrapMode: Text.Wrap }
                                Basic.Label { Layout.fillWidth: true; visible: text.length > 0; text: card.modelData.warning; textFormat: Text.PlainText; font.pixelSize: 11; color: Theme.muted; wrapMode: Text.Wrap }
                            }
                        }
                    }
                }
            }
        }
        Basic.Label { id: footer; text: qsTr("%1 active").arg(panel.manager.activeCount); font.pixelSize: 11; color: Theme.muted }
    }
}
