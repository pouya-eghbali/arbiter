pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Effects

Rectangle {
    id: root
    required property bool framed
    required property bool windowActive
    property int shadowInset: framed ? 24 : 0
    anchors.fill: parent
    anchors.margins: shadowInset
    radius: framed ? 12 : 0
    border.width: framed ? 1 : 0
    border.color: Qt.rgba(1, 1, 1, windowActive ? 0.15 : 0.08)
    layer.enabled: framed
    layer.effect: MultiEffect {
        shadowEnabled: true
        shadowBlur: 0.7
        shadowVerticalOffset: 5
        shadowOpacity: root.windowActive ? 0.45 : 0.25
        shadowColor: "black"
    }
}
