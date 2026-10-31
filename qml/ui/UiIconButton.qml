pragma ComponentBehavior: Bound
import QtQuick
import Arbiter

UiButton {
    id: control
    required property string label
    property bool toolTipEnabled:true
    text: ""
    implicitWidth: 32
    implicitHeight: 32
    padding: 6
    horizontalPadding: 6
    iconSize: 18
    flat: true
    Accessible.name: label
    UiToolTip {
        visible: control.toolTipEnabled && (control.hovered || control.activeFocus)
        text: control.label
    }
}
