import QtQuick
import Arbiter

UiTextField {
    id: control
    readonly property bool nativeMaterial: Theme.nativeFor(control)
    implicitHeight: 48
    font.pixelSize: 16
    inputMethodHints: Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText
    selectionColor: nativeMaterial ? Qt.alpha(Theme.textFor(control), .2) : Theme.selection
    background: Rectangle {
        radius: control.nativeMaterial ? height / 2 : 12
        color: control.activeFocus ? Theme.hoverFor(control) : Theme.fieldFor(control)
        border.width: control.nativeMaterial && !control.activeFocus ? 0 : 1
        border.color: control.nativeMaterial ? Qt.alpha(Theme.textFor(control), .22)
            : control.activeFocus ? Theme.accent : Theme.borderFor(control)
    }
}
