pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic as Basic
import Arbiter.Input 1.0

Item {
    id: overlay
    required property var layout
    property int sourcePane: 0
    property int draggedPane: 0
    property point dragPoint: Qt.point(0, 0)
    readonly property int dropPane: paneAt(dragPoint)
    signal focusRequested()
    function paneAt(point: point): int {
        const panes = layout.panes
        for (let i = 0; i < panes.length; ++i) {
            const p = panes[i]
            if (p.paneId !== sourcePane && point.x >= p.x && point.x < p.x + p.width
                    && point.y >= p.y && point.y < p.y + p.height) return p.paneId
        }
        return 0
    }
    visible: layout.multiple
    SplitSwapGesture {
        anchors.fill: overlay
        panes: overlay.layout.panes
        onPanePressed: function(pane) { overlay.layout.activate(pane) }
        onStarted: function(pane, position) {
            overlay.sourcePane = pane
            overlay.dragPoint = position
        }
        onMoved: function(position) {
            overlay.draggedPane = overlay.sourcePane
            overlay.dragPoint = position
        }
        onFinished: function(commit, position) {
            overlay.dragPoint = position
            const source = overlay.sourcePane
            const target = overlay.dropPane
            overlay.draggedPane = 0
            overlay.sourcePane = 0
            if (commit && target) overlay.layout.swap(source, target)
            overlay.focusRequested()
        }
    }
    component Outline: PaneShape {
        required property var info
        property color fill: "transparent"
        property real strokeWidth: 1
        x: info ? info.x : 0
        y: info ? info.y : 0
        width: info ? info.width : 0
        height: info ? info.height : 0
        color: fill
        border.width: strokeWidth
        border.color: Theme.accent
    }
    Repeater {
        model: overlay.layout.panes.length
        delegate: Outline {
            required property int index
            info: overlay.layout.panes[index]
            opacity: info && info.paneId === overlay.layout.activePane ? 0.35 : 0
        }
    }
    Repeater {
        model: overlay.layout.handles.length
        delegate: Rectangle {
            id: handle
            required property int index
            readonly property var info: overlay.layout.handles[index]
            objectName: "splitHandle" + (info ? info.handleId : 0)
            x: info ? info.x : 0
            y: info ? info.y : 0
            width: info ? info.width : 0
            height: info ? info.height : 0
            z: 5
            color: resizeArea.pressed || resizeArea.containsMouse ? Theme.selection : "transparent"
            radius: 4
            Rectangle {
                anchors.centerIn: parent
                width: handle.info && handle.info.horizontal ? 2 : Math.min(32, parent.width)
                height: handle.info && handle.info.horizontal ? Math.min(32, parent.height) : 2
                radius: 1
                color: Theme.muted
            }
            MouseArea {
                id: resizeArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: handle.info && handle.info.horizontal ? Qt.SplitHCursor : Qt.SplitVCursor
                property real grabOffset: 0
                property bool balanced: false
                function resizeAt(mouse: var) {
                    if (!handle.info) return
                    const point = mapToItem(overlay, mouse.x, mouse.y)
                    overlay.layout.resize(handle.info.handleId,
                        point.x - (handle.info.horizontal ? grabOffset : 0),
                        point.y - (handle.info.horizontal ? 0 : grabOffset))
                }
                onPressed: function(mouse) {
                    balanced = false
                    grabOffset = handle.info && handle.info.horizontal ? mouse.x : mouse.y
                }
                onPositionChanged: function(mouse) { if (pressed) resizeAt(mouse) }
                onReleased: function(mouse) { if (!balanced) resizeAt(mouse) }
                onDoubleClicked: {
                    balanced = true
                    if (!handle.info) return
                    const h = handle.info
                    const middle = h.origin + h.length / 2
                    overlay.layout.resize(h.handleId, h.horizontal ? middle : h.x, h.horizontal ? h.y : middle)
                }
                Accessible.role: Accessible.Grip
                Accessible.name: qsTr("Resize split. Double click to balance.")
            }
        }
    }
    Repeater {
        model: overlay.layout.panes.length
        delegate: Outline {
            required property int index
            info: overlay.layout.panes[index]
            z: 6
            visible: overlay.draggedPane !== 0 && info && info.paneId === overlay.dropPane
            fill: Theme.selection
            strokeWidth: 2
            Basic.Label {
                anchors.centerIn: parent
                text: qsTr("Drop to swap panes")
                color: Theme.text
            }
        }
    }
}
