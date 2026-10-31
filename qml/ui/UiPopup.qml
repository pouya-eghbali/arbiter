pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic as Basic
import Arbiter

Basic.Popup {
    id: popupControl
    readonly property var coveredOwner: {
        const attachedWindow = parent ? parent.Window.window : null
        return SurfacePalette.owner(parent)
    }
    Connections {
        target: popupControl.coveredOwner
        ignoreUnknownSignals: true
        function onPrivateCoveredChanged() {
            if (popupControl.coveredOwner && popupControl.coveredOwner["privateCovered"] === true) {
                SurfacePalette.conceal(popupControl.contentItem)
                popupControl.close()
            }
        }
    }
    Connections {
        target: popupControl
        function onVisibleChanged() { if (popupControl.visible && popupControl.coveredOwner && popupControl.coveredOwner["privateCovered"] === true) Qt.callLater(() => popupControl.close()) }
    }
    popupType: Theme.nativePopups ? Basic.Popup.Window : Basic.Popup.Item
    palette.windowText: Theme.textFor(contentItem)
    palette.text: Theme.textFor(contentItem)
    palette.buttonText: Theme.textFor(contentItem)
    palette.placeholderText: Theme.mutedFor(contentItem)
    background: GlassSurface { popup: popupControl }
    Binding { target: popupControl.background; property: "popup"; value: popupControl; when: popupControl.background && popupControl.background["popup"] !== undefined }
    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Theme.enterDuration; easing.type: Easing.OutCubic }
    }
    exit: Transition {
        NumberAnimation { property: "opacity"; from: 1; to: 0; duration: Theme.exitDuration; easing.type: Easing.OutCubic }
    }
}
