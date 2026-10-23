pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Dialogs
import QtQuick.Layouts
import QtQuick.Controls.Basic as Basic
import Arbiter

ColumnLayout {
    id: picker
    required property string title
    required property color value
    required property color defaultColor
    property var presets: [defaultColor.toString(), "#a855f7", "#e45b8a", "#de6c36", "#d4a52a", "#279c73", "#279db0", "#8793a6"]
    signal colorSelected(color color)
    function choose(selected: color) {
        colorSelected(selected)
    }
    spacing: 10
    Basic.Label { text: picker.title; color: Theme.text; font.weight: Font.Medium }
    Flow {
        Layout.fillWidth: true
        spacing: 8
        Repeater {
            model: picker.presets
            delegate: UiButton {
                id: swatch
                required property string modelData
                readonly property color swatchColor: modelData
                width: 30
                height: 30
                padding: 0
                Accessible.name: qsTr("%1: %2").arg(picker.title).arg(modelData)
                Accessible.role: Accessible.RadioButton
                Accessible.checked: picker.value.toString() === modelData.toLowerCase()
                contentItem: Item { }
                background: Rectangle {
                    anchors.fill: parent
                    anchors.margins: 3
                    radius: width / 2
                    color: swatch.swatchColor
                    Rectangle {
                        anchors.fill: parent
                        anchors.margins: -3
                        radius: width / 2
                        color: "transparent"
                        border.width: 2
                        border.color: swatch.activeFocus || picker.value.toString() === swatch.modelData.toLowerCase() ? Theme.text : "transparent"
                    }
                }
                onClicked: picker.choose(swatchColor)
            }
        }
    }
    RowLayout {
        Layout.fillWidth: true
        UiButton {
            id: chooseButton
            Accessible.name: qsTr("Choose %1 color").arg(picker.title)
            contentItem: RowLayout {
                spacing: 10
                Rectangle { implicitWidth: 18; implicitHeight: 18; radius: 6; color: picker.value; border.color: Theme.border }
                Basic.Label { text: qsTr("Choose color…"); color: Theme.text; font: chooseButton.font }
            }
            onClicked: { dialog.selectedColor = picker.value; dialog.open() }
        }
        UiButton {
            text: qsTr("Reset")
            flat: true
            onClicked: picker.choose(picker.defaultColor)
        }
    }
    ColorDialog {
        id: dialog
        objectName: "colorDialog"
        title: qsTr("Choose %1 color").arg(picker.title)
        parentWindow: picker.Window.window
        options: ColorDialog.DontUseNativeDialog
        popupType: Basic.Popup.Item
        onAccepted: picker.choose(Qt.rgba(selectedColor.r, selectedColor.g, selectedColor.b, 1))
    }
}
