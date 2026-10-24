pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic as Basic

ColumnLayout {
    id: progress
    required property var controller
    property string heading: ""
    visible: controller.busy
    Layout.fillWidth: true
    spacing: 10
    RowLayout {
        Layout.fillWidth: true
        spacing: 12
        Basic.BusyIndicator {
            objectName: "importSpinner"
            Layout.preferredWidth: 24
            Layout.preferredHeight: 24
            padding: 0
            palette.dark: Theme.accent
            running: progress.visible && progress.controller.busy
            Accessible.name: qsTr("Import in progress")
        }
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 5
            Basic.Label {
                visible: text.length > 0
                text: progress.heading
                color: Theme.text
                textFormat: Text.PlainText
                Layout.fillWidth: true
                wrapMode: Text.Wrap
            }
            Basic.Label {
                objectName: "importStage"
                text: progress.controller.progress
                color: Theme.muted
                textFormat: Text.PlainText
                Layout.fillWidth: true
                wrapMode: Text.Wrap
            }
        }
    }
    Basic.ProgressBar {
        id: saving
        objectName: "passwordImportProgress"
        visible: progress.controller.importing && progress.controller.passwordCount > 0
        Layout.fillWidth: true
        from: 0
        to: Math.max(1, progress.controller.passwordCount)
        value: progress.controller.processed
        background: Rectangle { implicitHeight: 4; radius: 2; color: Theme.border }
        contentItem: Item {
            implicitHeight: 4
            Rectangle { width: parent.width * saving.visualPosition; height: parent.height; radius: 2; color: Theme.accent }
        }
        Accessible.name: qsTr("Passwords saved securely")
    }
}
