pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Dialogs
import QtQuick.Layouts
import QtQuick.Controls.Basic as Basic
import Arbiter

Basic.Pane {
    id: picker
    required property string title
    required property url value
    property color backgroundColor: Theme.window
    property string error: ""
    signal fileSelected(url file)
    signal removeRequested()
    padding: 16
    background: Rectangle { radius: 16; color: Theme.field }
    contentItem: ColumnLayout {
        spacing: 12
        Basic.Label { text: picker.title; color: Theme.text; font.weight: Font.Medium }
        RowLayout {
            Layout.fillWidth: true
            spacing: 16
            Rectangle {
                Layout.preferredWidth: picker.width < 360 ? 72 : 112
                Layout.preferredHeight: 80
                radius: 10
                color: picker.backgroundColor
                border.color: Theme.border
                Image {
                    id: preview
                    anchors.fill: parent
                    anchors.margins: 4
                    clip: true
                    source: picker.value
                    asynchronous: true
                    fillMode: Image.PreserveAspectCrop
                    sourceSize: Qt.size(224, 160)
                }
            }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 8
                Basic.Label {
                    Layout.fillWidth: true
                    text: picker.value.toString().length === 0 ? qsTr("Use background color")
                        : preview.status === Image.Error ? qsTr("Image unavailable. Choose another or remove it.") : qsTr("Start page and split-view background")
                    wrapMode: Text.Wrap
                    color: Theme.muted
                }
                RowLayout {
                    Layout.fillWidth: true
                    UiButton {
                        Layout.maximumWidth: parent.width - removeButton.width - parent.spacing
                        text: qsTr("Choose image…")
                        Accessible.name: qsTr("Choose %1").arg(picker.title)
                        onClicked: dialog.open()
                    }
                    UiIconButton {
                        id: removeButton
                        label: qsTr("Remove")
                        iconName: "trash"
                        enabled: picker.value.toString().length > 0
                        onClicked: { picker.error = ""; picker.removeRequested() }
                    }
                }
            }
        }
        Basic.Label { Layout.fillWidth: true; visible: text.length > 0; text: picker.error; color: Theme.muted; wrapMode: Text.Wrap }
    }
    FileDialog {
        id: dialog
        title: qsTr("Choose %1").arg(picker.title)
        parentWindow: picker.Window.window
        fileMode: FileDialog.OpenFile
        nameFilters: [qsTr("Images (*.png *.jpg *.jpeg *.webp *.avif *.heic *.bmp *.tif *.tiff)"), qsTr("All files (*)")]
        onAccepted: { picker.error = ""; picker.fileSelected(selectedFile) }
    }
}
