pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Effects

Rectangle {
    id: glass
    radius: 18
    color: Theme.surface
    property bool dense: false
    property Item sourceItem: Theme.backdrop
    readonly property point samplePosition: {
        // Re-evaluate when the panel or source geometry changes.
        const geometry = [width, height, x, y, visible, sourceItem ? sourceItem.width : 0, sourceItem ? sourceItem.height : 0]
        return sourceItem && geometry.length ? mapToItem(sourceItem, 0, 0) : Qt.point(0, 0)
    }
    Rectangle {
        id: roundedMask
        anchors.fill: parent
        radius: glass.radius
        layer.enabled: true
        visible: false
    }
    ShaderEffectSource {
        id: capture
        sourceItem: glass.sourceItem
        sourceRect: Qt.rect(glass.samplePosition.x, glass.samplePosition.y, glass.width, glass.height)
        textureSize: Qt.size(Math.max(1, Math.ceil(glass.width / 2)), Math.max(1, Math.ceil(glass.height / 2)))
        live: glass.visible
        visible: false
    }
    MultiEffect {
        anchors.fill: parent
        visible: glass.sourceItem !== null
        source: capture
        blurEnabled: true
        blurMax: 32
        blur: 1
        autoPaddingEnabled: false
        maskEnabled: true
        maskSource: roundedMask
    }
    Rectangle {
        anchors.fill: parent
        radius: glass.radius
        color: glass.dense || !glass.sourceItem ? Theme.surface : Theme.glass
        border.color: Theme.border
        border.width: 1
    }
}
