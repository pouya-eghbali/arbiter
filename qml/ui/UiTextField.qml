pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic as Basic
import Arbiter

Basic.TextField {
    id: control
    implicitHeight: 44
    padding: 12
    color: Theme.textFor(parent)
    placeholderTextColor: Theme.mutedFor(parent)
    selectionColor: Theme.selection
    selectedTextColor: Theme.textFor(parent)
    verticalAlignment: TextInput.AlignVCenter
    selectByMouse: true
    opacity: enabled ? 1 : 0.45
    background: Rectangle {
        radius: 12
        color: control.activeFocus ? Theme.hoverFor(parent) : Theme.fieldFor(parent)
        border.color: control.activeFocus ? Theme.accent : Theme.borderFor(parent)
        Behavior on color { ColorAnimation { duration: 100 } }
    }
}
