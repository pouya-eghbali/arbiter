pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import QtQuick.Dialogs
import Arbiter

ColumnLayout {
    id: root
    property bool dark: Theme.dark
    property string choosing: "Accent"
    readonly property color accent: root.dark ? browser.settings.darkAccent : browser.settings.lightAccent
    readonly property color background: root.dark ? browser.settings.darkBackground : browser.settings.lightBackground
    readonly property string backgroundImage: (root.dark ? browser.settings.darkImage : browser.settings.lightImage) || ""
    spacing: 20
    RowLayout {
        Layout.fillWidth: true
        spacing: 8
        Repeater {
            model: [qsTr("Light"), qsTr("Dark")]
            delegate: MobileButton {
                required property int index
                required property string modelData
                Layout.fillWidth: true
                Layout.preferredWidth: 1
                text: modelData
                checkable: true
                checked: root.dark === (index === 1)
                onClicked: root.dark = index === 1
            }
        }
    }
    MobileSection {
        Layout.fillWidth: true
        title: qsTr("Accent color")
        GridLayout {
            Layout.fillWidth: true
            columns: 4
            columnSpacing: 8
            rowSpacing: 4
            Repeater {
                model: ["#7bb7ff", "#ab4dff", "#ef478b", "#ee6a26", "#e0ad00", "#009f76", "#00a3b7", "#8895aa"]
                delegate: MobileIconButton {
                    id: swatch
                    required property string modelData
                    Layout.fillWidth: true
                    label: modelData
                    iconName: "check"
                    iconColor: root.accent.toString() === modelData ? Theme.contrastingText(modelData) : "transparent"
                    onClicked: browser.setSetting(root.dark ? "darkAccent" : "lightAccent", modelData)
                    background: Item {
                        Rectangle {
                            anchors.centerIn: parent
                            width: 32
                            height: 32
                            radius: 16
                            color: swatch.modelData
                            border.width: root.accent.toString() === swatch.modelData ? 2 : 0
                            border.color: Theme.text
                        }
                    }
                }
            }
        }
        MobileButton {
            Layout.fillWidth: true
            text: qsTr("Choose color…")
            onClicked: {
                root.choosing = "Accent";
                picker.selectedColor = root.accent;
                picker.open();
            }
        }
    }
    MobileSection {
        Layout.fillWidth: true
        title: qsTr("Background color")
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 48
            radius: 12
            color: root.background
            border.width: 1
            border.color: Theme.border
        }
        MobileButton {
            Layout.fillWidth: true
            text: qsTr("Choose color…")
            onClicked: {
                root.choosing = "Background";
                picker.selectedColor = root.background;
                picker.open();
            }
        }
    }
    MobileSection {
        Layout.fillWidth: true
        title: qsTr("Background image")
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 112
            radius: 12
            color: root.background
            clip: true
            Image {
                anchors.fill: parent
                source: root.backgroundImage
                fillMode: Image.PreserveAspectCrop
                visible: source.toString().length > 0
            }
            UiIcon {
                anchors.centerIn: parent
                width: 28
                height: 28
                name: "panels-top-left"
                color: Theme.muted
                visible: !root.backgroundImage
            }
        }
        MobileButton {
            Layout.fillWidth: true
            text: qsTr("Choose background image")
            onClicked: image.open()
        }
        MobileButton {
            Layout.fillWidth: true
            text: qsTr("Remove background image")
            visible: root.backgroundImage.length > 0
            flat: true
            onClicked: browser.setSetting(root.dark ? "darkImage" : "lightImage", "")
        }
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
