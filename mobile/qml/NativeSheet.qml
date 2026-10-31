pragma ComponentBehavior: Bound
import QtQuick
import Arbiter

Item {
    id: sheet
    default property alias surfaceData: body.data
    property real radius: 28
    property bool requested: Theme.liquidGlass
    readonly property bool nativeActive: host.active
    property Item desiredFocus: null
    signal escapeRequested()
    function focusControl(item) { desiredFocus = item; host.focusControl(item) }
    onNativeActiveChanged: if (nativeActive && desiredFocus) Qt.callLater(() => { if (sheet.visible && host.active && sheet.desiredFocus) host.focusControl(sheet.desiredFocus) })
    onVisibleChanged: if (!visible) desiredFocus = null
    data: [
        NativeGlassHost {
            id: host
            parent: sheet
            anchors.fill: parent
            requested: sheet.requested
            radius: sheet.radius
            content: body
            onEscapeRequested: sheet.escapeRequested()
            WindowContainer { anchors.fill: parent; window: host.containerWindow; visible: host.active }
        },
        GlassSurface {
            parent: sheet
            anchors.fill: parent
            visible: !host.active
            radius: sheet.radius
            dense: true
            forceOpaque: host.reducedTransparency
        },
        FocusScope { id: body; parent: sheet; anchors.fill: parent }
    ]
}
