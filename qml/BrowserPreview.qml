pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic as Basic

Rectangle {
    id: preview
    property bool decorations: false
    property bool pill: true
    property string caption: qsTr("A little room to explore.")
    color: Theme.window
    radius: 12
    border.color: Theme.border
    border.width: 1
    implicitHeight: 150
    clip: true
    Rectangle {
        visible: preview.decorations
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 1
        height: 25
        radius: 11
        color: Theme.field
        Row {
            anchors.left: parent.left; anchors.leftMargin: 10
            anchors.verticalCenter: parent.verticalCenter
            spacing: 4
            Repeater { model: 3; Rectangle { width: 5; height: 5; radius: 3; color: Theme.muted; opacity: 0.5 } }
        }
        Basic.Label { anchors.centerIn: parent; text: "Arbiter"; color: Theme.muted; font.pixelSize: 9 }
    }
    Column {
        anchors.centerIn: parent
        width: parent.width - 26
        spacing: 10
        Basic.Label { text: preview.caption; width: parent.width; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.Wrap; color: Theme.text; font.pixelSize: 12 }
        Rectangle { anchors.horizontalCenter: parent.horizontalCenter; width: Math.min(180, parent.width); height: 20; radius: 7; color: Theme.field
            Rectangle { anchors.left: parent.left; anchors.leftMargin: 8; anchors.verticalCenter: parent.verticalCenter; width: parent.width * 0.52; height: 3; radius: 2; color: Theme.muted; opacity: 0.25 }
        }
    }
    Rectangle {
        visible: preview.pill
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 10
        width: 42; height: 9; radius: 5
        color: Theme.accent
        opacity: 0.8
    }
}
