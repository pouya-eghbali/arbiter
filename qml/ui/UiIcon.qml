import QtQuick
import Arbiter

Item {
    id: icon
    property string name: "user"
    property color color: Theme.accent
    implicitWidth: 28
    implicitHeight: 28
    Item {
        anchors.centerIn: parent
        width: 24; height: 24
        visible: icon.name === "phone"
        Rectangle { x: 5; y: 1; width: 14; height: 22; radius: 3; color: "transparent"; border.width: 2; border.color: icon.color }
        Rectangle { x: 10; y: 18; width: 4; height: 2; radius: 1; color: icon.color }
    }
    Item {
        anchors.centerIn: parent
        width: 24; height: 24
        visible: icon.name === "user"
        Rectangle { x: 8; y: 1; width: 9; height: 9; radius: 4.5; color: icon.color }
        Rectangle { x: 3; y: 12; width: 19; height: 11; radius: 5; color: icon.color }
    }
    Item {
        anchors.centerIn: parent
        width: 24; height: 24
        visible: icon.name === "key"
        Rectangle { x: 1; y: 3; width: 12; height: 12; radius: 6; color: "transparent"; border.width: 2; border.color: icon.color }
        Rectangle { x: 11; y: 8; width: 12; height: 3; radius: 1; color: icon.color }
        Rectangle { x: 18; y: 9; width: 3; height: 7; color: icon.color }
    }
}
