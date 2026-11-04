pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Window

Item {
    id: root
    required property Window targetWindow
    readonly property int grip: 5
    readonly property int corner: 12
    visible: targetWindow.visibility === Window.Windowed
    Repeater {
        model: [
            {edges: Qt.LeftEdge, cursor: Qt.SizeHorCursor, x: 0, y: root.corner, w: root.grip, h: root.height - 2 * root.corner},
            {edges: Qt.RightEdge, cursor: Qt.SizeHorCursor, x: root.width - root.grip, y: root.corner, w: root.grip, h: root.height - 2 * root.corner},
            {edges: Qt.TopEdge, cursor: Qt.SizeVerCursor, x: root.corner, y: 0, w: root.width - 2 * root.corner, h: root.grip},
            {edges: Qt.BottomEdge, cursor: Qt.SizeVerCursor, x: root.corner, y: root.height - root.grip, w: root.width - 2 * root.corner, h: root.grip},
            {edges: Qt.LeftEdge | Qt.TopEdge, cursor: Qt.SizeFDiagCursor, x: 0, y: 0, w: root.corner, h: root.corner},
            {edges: Qt.RightEdge | Qt.TopEdge, cursor: Qt.SizeBDiagCursor, x: root.width - root.corner, y: 0, w: root.corner, h: root.corner},
            {edges: Qt.LeftEdge | Qt.BottomEdge, cursor: Qt.SizeBDiagCursor, x: 0, y: root.height - root.corner, w: root.corner, h: root.corner},
            {edges: Qt.RightEdge | Qt.BottomEdge, cursor: Qt.SizeFDiagCursor, x: root.width - root.corner, y: root.height - root.corner, w: root.corner, h: root.corner}
        ]
        delegate: MouseArea {
            required property var modelData
            x: modelData.x; y: modelData.y
            width: Math.max(0, modelData.w); height: Math.max(0, modelData.h)
            cursorShape: modelData.cursor
            acceptedButtons: Qt.LeftButton
            onPressed: root.targetWindow.startSystemResize(modelData.edges)
        }
    }
}
