pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic as Basic
import Arbiter

Basic.Menu {
    id: menu
    readonly property var coveredOwner: {
        const attachedWindow = parent ? parent.Window.window : null
        return SurfacePalette.owner(parent)
    }
    Connections {
        target: menu.coveredOwner
        ignoreUnknownSignals: true
        function onPrivateCoveredChanged() {
            if (menu.coveredOwner && menu.coveredOwner["privateCovered"] === true) {
                SurfacePalette.conceal(menu.contentItem)
                menu.close()
            }
        }
    }
    Connections {
        target: menu
        function onVisibleChanged() { if (menu.visible && menu.coveredOwner && menu.coveredOwner["privateCovered"] === true) Qt.callLater(() => menu.close()) }
    }
    property real preferredWidth: 280
    readonly property var hostWindow: parent ? parent.Window.window : null
    popupType: Theme.nativePopups ? Basic.Popup.Window : Basic.Popup.Item
    padding: 6
    margins: 12
    width: Math.max(0, Math.min(preferredWidth, hostWindow ? hostWindow.width - 24 : preferredWidth))
    height: Math.max(0, Math.min(implicitHeight, hostWindow ? hostWindow.height - 24 : implicitHeight))
    enter: Transition { NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Theme.enterDuration; easing.type: Easing.OutCubic } }
    exit: Transition { NumberAnimation { property: "opacity"; from: 1; to: 0; duration: Theme.exitDuration; easing.type: Easing.OutCubic } }
    delegate: UiMenuItem { }
    background: GlassSurface { popup: menu; dense: true; radius: Theme.menuRadius }
}
