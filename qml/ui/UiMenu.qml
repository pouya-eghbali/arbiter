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
    delegate: UiMenuItem { }
    background: GlassSurface { dense: true; radius: Theme.menuRadius }
}
