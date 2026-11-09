pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic as Basic
import QtWebEngine
import Arbiter

Rectangle {
    id: fullscreen
    property WebEngineView view: null
    readonly property bool active: view !== null
    readonly property var hostWindow: fullscreen.Window.window
    property string origin: ""
    property int previousVisibility: Window.Windowed
    property bool changingVisibility: false
    anchors.fill: parent
    color: "black"
    visible: active
    z: 30

    function request(source: WebEngineView, request: fullScreenRequest, allowed: bool): void {
        if (!request.toggleOn) {
            request.accept()
            if (source === view) finish(true)
            return
        }
        if (!allowed || (view && view !== source)) {
            // Qt must acknowledge the engine state before its cancellation API
            // can unwind DOM fullscreen. Never promote this view or its window.
            request.accept()
            Qt.callLater(function() { if (source && source !== fullscreen.view) source.fullScreenCancelled() })
            return
        }
        if (!view) previousVisibility = hostWindow.visibility
        origin = request.origin.toString()
        view = source
        request.accept()
        changingVisibility = true
        hostWindow.showFullScreen()
        changingVisibility = false
        notice.restart()
        source.forceActiveFocus()
    }
    function finish(restore: bool) {
        if (!view) return
        view = null
        notice.stop()
        if (restore && previousVisibility !== Window.FullScreen) {
            if (previousVisibility === Window.Maximized) hostWindow.showMaximized()
            else hostWindow.showNormal()
        }
    }
    function exit() {
        const source = view
        if (!source) return
        finish(true)
        source.fullScreenCancelled()
        source.forceActiveFocus()
    }
    function closeFor(source: WebEngineView) { if (view === source) exit() }

    // Override and restore the original split geometry bindings. The same view,
    // document, media element and playback session stay alive throughout.
    Binding { target: fullscreen.view; property: "parent"; value: fullscreen; when: fullscreen.active; restoreMode: Binding.RestoreBindingOrValue }
    Binding { target: fullscreen.view; property: "x"; value: 0; when: fullscreen.active; restoreMode: Binding.RestoreBindingOrValue }
    Binding { target: fullscreen.view; property: "y"; value: 0; when: fullscreen.active; restoreMode: Binding.RestoreBindingOrValue }
    Binding { target: fullscreen.view; property: "width"; value: fullscreen.width; when: fullscreen.active; restoreMode: Binding.RestoreBindingOrValue }
    Binding { target: fullscreen.view; property: "height"; value: fullscreen.height; when: fullscreen.active; restoreMode: Binding.RestoreBindingOrValue }
    Binding { target: fullscreen.view; property: "visible"; value: true; when: fullscreen.active; restoreMode: Binding.RestoreBindingOrValue }
    Binding { target: fullscreen.view; property: "layer.enabled"; value: false; when: fullscreen.active; restoreMode: Binding.RestoreBindingOrValue }

    Shortcut { sequence: "Escape"; enabled: fullscreen.active; onActivated: fullscreen.exit() }
    Connections {
        target: fullscreen.hostWindow
        function onVisibilityChanged() {
            if (fullscreen.active && !fullscreen.changingVisibility
                    && fullscreen.hostWindow.visibility !== Window.FullScreen) {
                const source = fullscreen.view
                fullscreen.finish(false)
                source.fullScreenCancelled()
            }
        }
    }
    Connections {
        target: fullscreen.view
        function onUrlChanged() { fullscreen.exit() }
        function onRenderProcessTerminated() { fullscreen.exit() }
        function onIsFullScreenChanged() {
            if (fullscreen.view && !fullscreen.view.isFullScreen) fullscreen.finish(true)
        }
    }
    Timer { id: notice; interval: 3500 }
    HoverHandler { id: topHover; parent: exitBar }
    Item {
        anchors.top: parent.top
        anchors.horizontalCenter: parent.horizontalCenter
        width: exitBar.width
        height: 16
        z: 3
        HoverHandler { onHoveredChanged: if (hovered) notice.restart() }
    }
    Rectangle {
        id: exitBar
        anchors.top: parent.top
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.topMargin: 16
        width: Math.min(parent.width - 32, 520)
        height: 52
        radius: 12
        z: 2
        color: Theme.popupSolid
        opacity: notice.running || topHover.hovered ? 1 : 0
        enabled: opacity > 0
        Basic.Label {
            anchors.left: parent.left
            anchors.leftMargin: 16
            anchors.right: exitButton.left
            anchors.rightMargin: 12
            anchors.verticalCenter: parent.verticalCenter
            text: fullscreen.origin
            textFormat: Text.PlainText
            color: Theme.text
            elide: Text.ElideRight
        }
        UiButton {
            id: exitButton
            anchors.right: parent.right
            anchors.rightMargin: 6
            anchors.verticalCenter: parent.verticalCenter
            text: qsTr("Exit fullscreen (Esc)")
            onClicked: fullscreen.exit()
        }
    }
}
