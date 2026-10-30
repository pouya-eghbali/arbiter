pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import QtQuick.Dialogs
import Arbiter

ColumnLayout {
    id: root
    property bool dark: Theme.dark
    property string choosing: "accent"
    spacing: 14
    RowLayout {
        UiButton {
            text: qsTr("Light")
            checkable: true
            checked: !root.dark
            onClicked: root.dark = false
        }
        UiButton {
            text: qsTr("Dark")
            checkable: true
            checked: root.dark
            onClicked: root.dark = true
        }
    }
    Text {
        textFormat: Text.PlainText
        text: qsTr("Accent color")
        color: Theme.text
        font.weight: Font.DemiBold
    }
    Flow {
        Layout.fillWidth: true
        spacing: 10
        Repeater {
            model: ["#7bb7ff", "#ab4dff", "#ef478b", "#ee6a26", "#e0ad00", "#009f76", "#00a3b7", "#8895aa"]
            delegate: Rectangle {
                required property string modelData
                width: 30
                height: 30
                radius: 15
                color: modelData
                border.color: Theme.text
                border.width: (root.dark ? browser.settings.darkAccent : browser.settings.lightAccent) === modelData ? 2 : 0
                TapHandler {
                    onTapped: browser.setSetting(root.dark ? "darkAccent" : "lightAccent", modelData)
                }
            }
        }
    }
    UiButton {
        text: qsTr("Choose color…")
        onClicked: {
            root.choosing = "Accent";
            picker.selectedColor = root.dark ? browser.settings.darkAccent : browser.settings.lightAccent;
            picker.open();
        }
    }
    Text {
        textFormat: Text.PlainText
        text: qsTr("Background color")
        color: Theme.text
        font.weight: Font.DemiBold
    }
    UiButton {
        text: qsTr("Choose color…")
        onClicked: {
            root.choosing = "Background";
            picker.selectedColor = root.dark ? browser.settings.darkBackground : browser.settings.lightBackground;
            picker.open();
        }
    }
    UiButton {
        text: qsTr("Choose background image")
        onClicked: image.open()
    }
    UiButton {
        text: qsTr("Remove background image")
        visible: Boolean(root.dark ? browser.settings.darkImage : browser.settings.lightImage)
        onClicked: browser.setSetting(root.dark ? "darkImage" : "lightImage", "")
    }
    ColorDialog {
        id: picker
        title: qsTr("Choose color")
        onAccepted: browser.setSetting((root.dark ? "dark" : "light") + root.choosing, selectedColor.toString())
    }
    FileDialog {
        id: image
        title: qsTr("Choose background image")
        nameFilters: [qsTr("Images (*.png *.jpg *.jpeg *.webp)")]
        onAccepted: browser.chooseBackground(selectedFile, root.dark)
    }
}
