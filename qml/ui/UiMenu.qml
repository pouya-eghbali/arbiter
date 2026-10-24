pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic as Basic
import Arbiter

Basic.Menu {
    id: menu
    property real preferredWidth: 280
    readonly property var hostWindow: parent ? parent.Window.window : null
    popupType: Basic.Popup.Item
    padding: 6
    margins: 12
    width: Math.max(0, Math.min(preferredWidth, hostWindow ? hostWindow.width - 24 : preferredWidth))
    height: Math.max(0, Math.min(implicitHeight, hostWindow ? hostWindow.height - 24 : implicitHeight))
    enter: Transition { NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Theme.enterDuration; easing.type: Easing.OutCubic } }
    exit: Transition { NumberAnimation { property: "opacity"; from: 1; to: 0; duration: Theme.exitDuration; easing.type: Easing.OutCubic } }
    delegate: UiMenuItem { }
    background: GlassSurface { dense: true; radius: Theme.menuRadius }
}
