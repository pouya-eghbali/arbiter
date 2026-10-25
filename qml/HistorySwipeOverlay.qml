pragma ComponentBehavior: Bound
import QtQuick

Item {
    id: overlay
    required property var controller
    property int lastDirection: -1
    Connections {
        target: overlay.controller
        function onChanged() { if (overlay.controller.direction) overlay.lastDirection = overlay.controller.direction }
    }
    GlassSurface {
        id: indicator
        x: overlay.lastDirection === -1 ? 12 + overlay.controller.progress * 16
            : overlay.width - width - 12 - overlay.controller.progress * 16
        anchors.verticalCenter: parent.verticalCenter
        width: 54; height: 54; radius: 27
        opacity: overlay.controller.active ? 1 : 0
        visible: opacity > 0
        Behavior on opacity { NumberAnimation { duration: Theme.enterDuration; easing.type: Easing.OutCubic } }
        Accessible.role: Accessible.Indicator
        Accessible.name: overlay.lastDirection === -1 ? qsTr("Back") : qsTr("Forward")
        Accessible.description: overlay.controller.progress >= 1 ? qsTr("Release to navigate") : qsTr("Keep swiping to navigate")
        Canvas {
            id: ring
            anchors.fill: parent
            readonly property real progress: overlay.controller.progress
            readonly property color tint: Theme.accent
            onProgressChanged: requestPaint()
            onTintChanged: requestPaint()
            onPaint: {
                const ctx = getContext("2d")
                ctx.reset()
                ctx.strokeStyle = tint
                ctx.lineWidth = 3
                ctx.lineCap = "round"
                ctx.beginPath()
                ctx.arc(width / 2, height / 2, width / 2 - 4, -Math.PI / 2, -Math.PI / 2 + progress * 2 * Math.PI)
                ctx.stroke()
            }
        }
        UiIcon {
            anchors.centerIn: parent
            name: overlay.lastDirection === -1 ? "arrow-left" : "arrow-right"
            width: 24; height: 24
            color: overlay.controller.progress >= 1 ? Theme.accent : Theme.text
        }
    }
}
