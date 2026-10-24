import QtQuick
import QtQuick.Shapes
import Arbiter

Item {
    id: icon
    property string name: "user"
    property color color: Theme.accent
    implicitWidth: 28
    implicitHeight: 28
    Shape {
        anchors.centerIn: parent
        width: 24; height: 24
        visible: icon.name === "share"
        ShapePath {
            strokeColor: icon.color
            strokeWidth: 1.8
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap
            joinStyle: ShapePath.RoundJoin
            startX: 7; startY: 9
            PathLine { x: 4; y: 9 }
            PathLine { x: 4; y: 21 }
            PathLine { x: 20; y: 21 }
            PathLine { x: 20; y: 9 }
            PathLine { x: 17; y: 9 }
        }
        ShapePath {
            strokeColor: icon.color
            strokeWidth: 1.8
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap
            joinStyle: ShapePath.RoundJoin
            startX: 12; startY: 15
            PathLine { x: 12; y: 2 }
            PathMove { x: 7; y: 7 }
            PathLine { x: 12; y: 2 }
            PathLine { x: 17; y: 7 }
        }
    }
    Shape {
        anchors.centerIn: parent
        width: 24; height: 24
        visible: icon.name === "shield"
        ShapePath {
            strokeColor: icon.color
            strokeWidth: 1.8
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap
            joinStyle: ShapePath.RoundJoin
            startX: 12; startY: 2
            PathLine { x: 21; y: 6 }
            PathLine { x: 20; y: 14 }
            PathQuad { x: 12; y: 22; controlX: 18; controlY: 19 }
            PathQuad { x: 4; y: 14; controlX: 6; controlY: 19 }
            PathLine { x: 3; y: 6 }
            PathLine { x: 12; y: 2 }
        }
    }
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
