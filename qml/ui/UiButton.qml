pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic as Basic
import Arbiter

Basic.Button {
    id: control
    property bool primary: false
    implicitWidth: Math.max(40, implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(40, implicitContentHeight + topPadding + bottomPadding)
    padding: 10
    horizontalPadding: 16
    hoverEnabled: true
    opacity: enabled ? 1 : 0.45
    contentItem: Basic.Label {
        text: control.text
        textFormat: Text.PlainText
        color: control.primary ? Theme.onAccent : Theme.text
        font: control.font
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
    background: Rectangle {
        radius: 12
        color: control.primary ? Theme.accent
            : control.down ? Theme.pressed : control.checked || control.highlighted ? Theme.selection
            : control.hovered || control.activeFocus ? Theme.hover : control.flat ? "transparent" : Theme.field
        border.width: control.activeFocus || control.primary && (control.hovered || control.down) ? 1 : 0
        border.color: control.primary ? Theme.onAccent : Theme.accent
        Behavior on color { ColorAnimation { duration: 100 } }
    }
}
