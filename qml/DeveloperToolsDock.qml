pragma ComponentBehavior: Bound
import QtQuick
import QtWebEngine
import Arbiter

Rectangle {
    id: dock
    property WebEngineView target: null
    property real preferredHeight: 340
    property real maximumHeight: 660
    readonly property WebEngineView frontend: tools.item as WebEngineView
    signal pageFocusRequested()
    height: target ? Math.min(preferredHeight, maximumHeight) : 0
    visible: target !== null
    color: Theme.window
    z: 15

    function close() {
        // Detach synchronously before deleting the frontend or its inspected tab.
        if (frontend) frontend.inspectedView = null
        tools.active = false
        target = null
    }
    function closeFor(view: WebEngineView) { if (target === view) close() }
    function openFor(view: WebEngineView, inspect = false) {
        if (!view) return
        if (target !== view) {
            close()
            view.lifecycleState = WebEngineView.LifecycleState.Active
            target = view
            tools.active = true
        }
        if (frontend) {
            frontend.forceActiveFocus(Qt.ShortcutFocusReason)
            // This action uses the last native context-menu location. It must
            // only be invoked from that menu, never from an arbitrary shortcut.
            if (inspect) view.triggerWebAction(WebEngineView.InspectElement)
        }
    }

    Rectangle {
        anchors.top: parent.top
        width: parent.width
        height: 1
        color: Theme.border
    }
    MouseArea {
        id: resizeHandle
        objectName: "developerToolsResizeHandle"
        anchors.top: parent.top
        width: parent.width
        height: 6
        z: 2
        cursorShape: Qt.SplitVCursor
        property real initialY: 0
        property real initialHeight: 0
        onPressed: function(mouse) {
            initialY = mapToItem(null, mouse.x, mouse.y).y
            initialHeight = dock.height
        }
        onPositionChanged: function(mouse) {
            if (pressed) dock.preferredHeight = Math.max(160, Math.min(dock.maximumHeight,
                initialHeight + initialY - mapToItem(null, mouse.x, mouse.y).y))
        }
        onDoubleClicked: dock.preferredHeight = Math.min(340, dock.maximumHeight)
        Accessible.name: qsTr("Resize developer tools")
    }
    Loader {
        id: tools
        anchors.top: resizeHandle.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        active: false
        sourceComponent: WebEngineView {
            objectName: "developerToolsView"
            // No browser credentials, extensions, scripts or persistent profile
            // are shared with the debugger frontend, even in a normal window.
            profile: WebEngineProfile { offTheRecord: true }
            inspectedView: dock.target
            onWindowCloseRequested: { dock.close(); dock.pageFocusRequested() }
        }
    }
}
