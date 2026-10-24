pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Dialogs
import QtQuick.Layouts
import QtQuick.Controls.Basic as Basic
import Arbiter

ColumnLayout {
    id: picker
    required property string title
    required property url value
    property string error: ""
    signal fileSelected(url file)
    signal removeRequested()
    spacing: 10
    Basic.Label { text: picker.title; color: Theme.text; font.weight: Font.Medium }
    RowLayout {
        Layout.fillWidth: true
        spacing: 12
        Rectangle {
            implicitWidth: 88
            implicitHeight: 56
            radius: 10
            color: Theme.field
            border.color: Theme.border
            Image {
                id: preview
                anchors.fill: parent
                anchors.margins: 5
                clip: true
                source: picker.value
                asynchronous: true
                fillMode: Image.PreserveAspectCrop
                sourceSize: Qt.size(176,112)
            }
        }
        Basic.Label {
            Layout.fillWidth: true
            text: picker.value.toString().length === 0 ? qsTr("Use background color")
                : preview.status === Image.Error ? qsTr("Image unavailable. Choose another or remove it.") : qsTr("Start page and split-view background")
            wrapMode: Text.Wrap
            color: Theme.muted
        }
    }
    RowLayout {
        UiButton { text: qsTr("Choose image…"); Accessible.name: qsTr("Choose %1").arg(picker.title); onClicked: dialog.open() }
        UiButton { text: qsTr("Remove"); enabled: picker.value.toString().length > 0; flat: true; onClicked: { picker.error = ""; picker.removeRequested() } }
    }
    Basic.Label { Layout.fillWidth: true; visible: text.length > 0; text: picker.error; color: Theme.muted; wrapMode: Text.Wrap }
    FileDialog {
        id: dialog
        title: qsTr("Choose %1").arg(picker.title)
        parentWindow: picker.Window.window
        fileMode: FileDialog.OpenFile
        nameFilters: [qsTr("Images (*.png *.jpg *.jpeg *.webp *.avif *.heic *.bmp *.tif *.tiff)"), qsTr("All files (*)")]
        onAccepted: { picker.error = ""; picker.fileSelected(selectedFile) }
    }
}
