pragma ComponentBehavior: Bound
import QtQuick
import Arbiter

Item {
    id: edge
    required property string mode
    required property string iconName
    required property string label
    property bool available: true
    property bool revealed: false
    property real dragX: 0
    property real dragY: 0
    readonly property bool ready: dragX <= -48 && dragY >= 48
    signal triggered()
    visible: available && mode !== "off"
    onEnabledChanged: if (!enabled) reset()
    onVisibleChanged: if (!visible) reset()
    onModeChanged: reset()
    function reset() { dwell.stop(); hide.stop(); revealed = false; dragX = 0; dragY = 0 }

    Item {
        id: activationZone
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        width: edge.revealed ? edge.width : 20
        HoverHandler {
            id: hover
            enabled: edge.mode === "hover"
            onHoveredChanged: {
                if (hovered) { hide.stop(); dwell.restart() }
                else { dwell.stop(); hide.restart() }
            }
        }
        DragHandler {
            id: drag
            enabled: edge.mode === "drag"
            target: null
            acceptedButtons: Qt.LeftButton
            acceptedModifiers: Qt.NoModifier
            acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
            onActiveTranslationChanged: if (active) {
                edge.dragX = activeTranslation.x
                edge.dragY = centroid.position.y - centroid.pressPosition.y
            }
            onActiveChanged: {
                if (active) { edge.dragX = 0; edge.dragY = 0 }
                else {
                    const commit = edge.ready && edge.enabled && edge.visible
                    edge.dragX = 0; edge.dragY = 0
                    if (commit) edge.triggered()
                }
            }
            onCanceled: { edge.dragX = 0; edge.dragY = 0 }
        }
    }
    // Track the entire revealed control, including its button: the narrow
    // activation strip can lose hover as soon as the button is above it.
    HoverHandler {
        id: revealedHover
        enabled: edge.revealed && edge.mode === "hover"
        onHoveredChanged: { if (hovered) hide.stop(); else hide.restart() }
    }
    Timer { id: dwell; interval: 180; onTriggered: if (edge.enabled && hover.hovered) edge.revealed = true }
    Timer { id: hide; interval: 700; onTriggered: if (!hover.hovered && !revealedHover.hovered && !actionButton.hovered && !actionButton.down) edge.revealed = false }
    GlassSurface {
        anchors.centerIn: parent
        width: 38; height: 38; radius: 19
        opacity: edge.revealed || drag.active ? 1 : 0
        visible: opacity > 0
        Behavior on opacity { NumberAnimation { duration: Theme.enterDuration; easing.type: Easing.OutCubic } }
        UiIconButton {
            id: actionButton
            onHoveredChanged: { if (hovered) hide.stop(); else hide.restart() }
            onDownChanged: if (!down && !hovered) hide.restart()
            anchors.fill: parent
            iconName: edge.iconName
            label: edge.label
            enabled: edge.mode === "hover" && edge.revealed
            iconColor: drag.active && edge.ready ? Theme.accent : Theme.text
            onClicked: { edge.revealed = false; edge.triggered() }
        }
    }
}
