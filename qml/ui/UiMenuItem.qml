pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic as Basic
import Arbiter

Basic.MenuItem {
    id: control
    implicitHeight: 36
    font.pixelSize: 13
    opacity: enabled ? 1 : 0.45
    horizontalPadding: 12
    contentItem: Basic.Label {
        text: control.text
        textFormat: Text.PlainText
        color: Theme.text
        font: control.font
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
    background: Rectangle { radius: 10; color: control.highlighted ? Theme.selection : control.hovered ? Theme.hover : "transparent" }
}
