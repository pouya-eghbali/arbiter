pragma ComponentBehavior: Bound
import QtQuick
import Arbiter
import QtQuick.Controls.Basic as Basic

Basic.CheckBox {
    id: control
    implicitHeight: 40
    spacing: 12
    padding: 4
    hoverEnabled: true
    indicator: Rectangle {
        x: control.leftPadding
        y: (control.height - height) / 2
        width: 22
        height: 22
        radius: 7
        color: control.checked ? Theme.accent : control.hovered ? Theme.hover : Theme.field
        border.width: 1
        border.color: control.activeFocus ? Theme.accent : Theme.border
        UiIcon {
            anchors.centerIn: parent
            width: 16; height: 16
            name: "check"
            visible: control.checked
            color: Theme.accentText
        }
    }
    contentItem: Basic.Label {
        text: control.text
        textFormat: Text.PlainText
        font: control.font
        color: control.enabled ? Theme.text : Theme.muted
        leftPadding: control.indicator.width + control.spacing
        verticalAlignment: Text.AlignVCenter
        wrapMode: Text.Wrap
    }
}
