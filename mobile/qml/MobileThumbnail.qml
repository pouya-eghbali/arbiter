import QtQuick
import QtQuick.Effects
import Arbiter

Rectangle {
    id: root
    property alias source: image.source
    property string fallback: ""
    readonly property bool effectsAvailable: GraphicsInfo.api !== GraphicsInfo.Software && GraphicsInfo.api !== GraphicsInfo.Unknown
    radius: 11
    color: Theme.fieldFor(parent)
    Image {
        id: image
        anchors.fill: parent
        sourceSize.width: 420
        fillMode: Image.PreserveAspectCrop
        verticalAlignment: Image.AlignTop
        cache: false
        layer.enabled: root.effectsAvailable && source.toString().length > 0
        layer.effect: MultiEffect {
            maskEnabled: true
            maskSource: maskCapture
            autoPaddingEnabled: false
            maskThresholdMin: .5
            maskSpreadAtMin: 1
        }
    }
    Rectangle {
        id: mask
        anchors.fill: parent
        radius: root.radius
        color: "white"
        visible: image.source.toString().length > 0
    }
    ShaderEffectSource {
        id: maskCapture
        anchors.fill: parent
        sourceItem: mask
        hideSource: true
        visible: false
    }
    Text {
        visible: !image.source.toString().length
        anchors.centerIn: parent
        text: root.fallback || "Arbiter"
        color: Theme.mutedFor(parent)
        font.pixelSize: 17
        elide: Text.ElideRight
        width: parent.width - 20
        horizontalAlignment: Text.AlignHCenter
        textFormat: Text.PlainText
    }
}
