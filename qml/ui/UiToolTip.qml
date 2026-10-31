pragma ComponentBehavior: Bound
import QtQuick
import Arbiter
import QtQuick.Controls.Basic as Basic

Basic.ToolTip {
    id: tip
    readonly property var coveredOwner: {
        const attachedWindow = parent ? parent.Window.window : null
        return SurfacePalette.owner(parent)
    }
    Connections {
        target: tip.coveredOwner
        ignoreUnknownSignals: true
        function onPrivateCoveredChanged() {
            if (tip.coveredOwner && tip.coveredOwner["privateCovered"] === true) {
                SurfacePalette.conceal(tip.contentItem)
                tip.close()
            }
        }
    }
    Connections {
        target: tip
        function onVisibleChanged() { if (tip.visible && tip.coveredOwner && tip.coveredOwner["privateCovered"] === true) Qt.callLater(() => tip.close()) }
    }
    objectName: "roundedToolTip"
    popupType: Theme.nativePopups ? Basic.Popup.Window : Basic.Popup.Item
    delay: 650
    timeout: 4000
    padding: 9
    horizontalPadding: 12
    font.pixelSize: 12
    contentItem: Basic.Label {
        text: tip.text
        textFormat: Text.PlainText
        color: Theme.textFor(parent)
        font: tip.font
        wrapMode: Text.Wrap
    }
    background: GlassSurface { popup: tip; radius: Theme.tooltipRadius; dense: true }
}
