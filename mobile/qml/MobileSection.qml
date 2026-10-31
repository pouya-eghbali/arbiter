import QtQuick
import QtQuick.Layouts
import Arbiter

ColumnLayout {
    id: root
    property string title: ""
    property string description: ""
    default property alias controls: body.data
    spacing: 12
    Text {
        visible: root.title.length > 0
        Layout.fillWidth: true
        text: root.title
        textFormat: Text.PlainText
        font.pixelSize: 14
        font.weight: Font.DemiBold
        color: Theme.muted
        wrapMode: Text.WordWrap
    }
    Rectangle {
        Layout.fillWidth: true
        implicitHeight: body.implicitHeight + 32
        radius: 18
        color: Theme.field
        ColumnLayout {
            id: body
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 16
            spacing: 12
        }
    }
    Text {
        visible: root.description.length > 0
        Layout.fillWidth: true
        text: root.description
        textFormat: Text.PlainText
        font.pixelSize: 13
        lineHeight: 1.2
        color: Theme.muted
        wrapMode: Text.WordWrap
    }
}
