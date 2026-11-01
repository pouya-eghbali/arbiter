pragma ComponentBehavior: Bound
import QtQuick
import Arbiter

MobileIconButton {
    id: control
    property bool overPreview: false
    iconName: "x"
    implicitWidth: MobileMetrics.touchTarget
    implicitHeight: MobileMetrics.touchTarget
    iconSize: 14
    iconColor: overPreview ? "white" : Theme.textFor(control)
    background: Rectangle {
        anchors.centerIn: parent
        width: 28
        height: 28
        radius: 14
        color: control.overPreview ? Qt.rgba(0, 0, 0, control.down ? .72 : .48)
            : control.down ? Theme.pressedFor(control) : Theme.fieldFor(control)
        border.width: control.activeFocus ? 1 : 0
        border.color: control.iconColor
    }
}
