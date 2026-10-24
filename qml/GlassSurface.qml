pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Effects
import Arbiter

Rectangle {
    id: glass
    radius: Theme.panelRadius
    color: "transparent"
    border.width: 0
    property bool dense: false
    property bool shadowEnabled: true
    readonly property var ownerWindow: Window.window
    property Item sourceItem: ownerWindow && ownerWindow["glassBackdrop"] !== undefined
        ? ownerWindow["glassBackdrop"] : Theme.backdrop
    readonly property bool effectsAvailable: GraphicsInfo.api !== GraphicsInfo.Software && GraphicsInfo.api !== GraphicsInfo.Unknown
    readonly property bool samplesBackdrop: visible && effectsAvailable && sourceItem !== null
        && ownerWindow !== null && sourceItem.Window.window === ownerWindow
    readonly property point samplePosition: {
        // mapToItem doesn't notify bindings about ancestor motion. Track both
        // chains so animated pills/popups stay aligned without a polling timer.
        let geometry = 0
        for (let item = glass; item; item = item.parent)
            geometry += item.x + item.y + item.width + item.height + item.scale + item.rotation
        for (let item = sourceItem; item; item = item.parent)
            geometry += item.x + item.y + item.width + item.height + item.scale + item.rotation
        return samplesBackdrop && Number.isFinite(geometry) ? mapToItem(sourceItem, 0, 0) : Qt.point(0, 0)
    }
    readonly property int blurRadius: 64
    readonly property rect captureRect: {
        const left = Math.max(0, samplePosition.x - blurRadius)
        const top = Math.max(0, samplePosition.y - blurRadius)
        const right = Math.min(sourceItem ? sourceItem.width : 0, samplePosition.x + width + blurRadius)
        const bottom = Math.min(sourceItem ? sourceItem.height : 0, samplePosition.y + height + blurRadius)
        return Qt.rect(left, top, Math.max(1, right - left), Math.max(1, bottom - top))
    }
    RectangularShadow {
        anchors.fill: parent
        z: -2
        visible: glass.shadowEnabled && glass.effectsAvailable
        radius: glass.radius
        blur: 32
        spread: 2
        offset: Qt.vector2d(0, 10)
        color: Theme.panelShadow
        cached: true
    }
    RectangularShadow {
        anchors.fill: parent
        z: -1
        visible: glass.shadowEnabled && glass.effectsAvailable
        radius: glass.radius
        blur: 10
        offset: Qt.vector2d(0, 2)
        color: Theme.panelContactShadow
        cached: true
    }
    Rectangle {
        id: roundedMask
        anchors.fill: parent
        radius: glass.radius
        color: "white"
        border.width: 0
        visible: glass.samplesBackdrop
    }
    ShaderEffectSource {
        id: maskCapture
        anchors.fill: parent
        sourceItem: glass.samplesBackdrop ? roundedMask : null
        hideSource: true
        visible: false
    }
    ShaderEffectSource {
        id: capture
        width: glass.captureRect.width
        height: glass.captureRect.height
        sourceItem: glass.samplesBackdrop ? glass.sourceItem : null
        sourceRect: glass.captureRect
        textureSize: Qt.size(Math.max(1, Math.ceil(width / 2)), Math.max(1, Math.ceil(height / 2)))
        live: glass.samplesBackdrop
        visible: false
    }
    Item {
        anchors.fill: parent
        visible: glass.samplesBackdrop
        clip: true
        layer.enabled: glass.samplesBackdrop
        layer.effect: MultiEffect {
            autoPaddingEnabled: false
            maskEnabled: true
            maskSource: maskCapture
        }
        // Blur the surrounding page before cropping to the rounded surface.
        // Even a six-pixel pill gets the same radius as a full-size panel.
        MultiEffect {
            x: glass.captureRect.x - glass.samplePosition.x
            y: glass.captureRect.y - glass.samplePosition.y
            width: capture.width
            height: capture.height
            source: capture
            blurEnabled: true
            blurMax: glass.blurRadius
            blur: 1
            saturation: -0.25
            brightness: Theme.darkSurface ? -0.22 : -0.04
            autoPaddingEnabled: false
        }
    }
    Rectangle {
        anchors.fill: parent
        radius: glass.radius
        border.width: 0
        color: glass.samplesBackdrop ? (glass.dense ? Theme.glassDense : Theme.glass) : Theme.popupSolid
    }
    // A broad, quiet sheen gives depth without drawing a bright perimeter.
    Rectangle {
        anchors.fill: parent
        radius: glass.radius
        border.width: 0
        gradient: Gradient {
            GradientStop { position: 0; color: Theme.panelSheen }
            GradientStop { position: 0.45; color: "transparent" }
            GradientStop { position: 1; color: "#05000000" }
        }
    }
}
