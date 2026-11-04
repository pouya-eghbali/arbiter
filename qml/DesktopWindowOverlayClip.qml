pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Window
import QtQuick.Effects
import QtQuick.Controls.Basic as Basic

Item {
    id: root
    required property Window targetWindow
    required property bool framed
    property int shadowInset: framed ? 24 : 0
    readonly property Item overlay: Basic.Overlay.overlay
    visible: false
    Binding {
        target: root.overlay ? root.overlay.layer : null
        property: "enabled"
        value: root.framed
        when: Qt.platform.os === "linux" && root.overlay !== null
    }
    Binding {
        target: root.overlay ? root.overlay.layer : null
        property: "effect"
        value: overlayEffect
        when: Qt.platform.os === "linux" && root.overlay !== null
    }
    Component { id: overlayEffect; MultiEffect { maskEnabled: true; maskSource: mask } }
    Rectangle {
        id: mask
        width: root.targetWindow.width
        height: root.targetWindow.height
        color: "transparent"
        layer.enabled: root.framed
        visible: false
        Rectangle {
            anchors.fill: parent
            anchors.margins: root.shadowInset
            color: "white"
            radius: root.framed ? 12 : 0
        }
    }
}
