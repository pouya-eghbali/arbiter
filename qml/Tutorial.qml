pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic as Basic
import "ui"

Basic.Popup {
    id: tour
    required property var controller
    required property var keyBindings
    required property bool navigationPill
    property int step: 0
    signal addressRequested()
    signal tabsRequested()
    signal workspacesRequested()
    signal splitRequested()
    parent: Basic.Overlay.overlay
    width: Math.min(430, parent.width - 32)
    x: parent.width - width - 16
    y: parent.height - height - 20
    padding: 22
    modal: false
    dim: false
    focus: true
    closePolicy: Basic.Popup.NoAutoClose
    onOpened: step = 0
    function shortcut(action: string): string { return keyBindings.nativeSequence(keyBindings.bindings[action]) }
    background: Rectangle { color: Theme.popupSolid; radius: 18; border.width: 1; border.color: Theme.border }
    contentItem: ColumnLayout {
        spacing: 14
        RowLayout {
            Layout.fillWidth: true
            Basic.Label { text: qsTr("A quick look around · %1 / 3").arg(tour.step + 1); color: Theme.muted; font.pixelSize: 12; Layout.fillWidth: true }
            UiButton { text: qsTr("Skip"); flat: true; onClicked: tour.controller.dismissTutorial() }
        }
        Basic.Label {
            text: [qsTr("Go somewhere."), qsTr("Keep your worlds together."), qsTr("Make room for two. Or more.")][tour.step]
            font.pixelSize: 22; font.weight: Font.DemiBold; color: Theme.text
            Layout.fillWidth: true; wrapMode: Text.Wrap
        }
        Basic.Label {
            text: tour.step === 0
                ? qsTr("Press %1 to type an address or search, from any page. %2").arg(tour.shortcut("address")).arg(tour.navigationPill ? qsTr("Hover over the tiny bottom handle to reveal navigation.") : qsTr("Your pill is hidden; shortcuts and the native menus are always available."))
                : tour.step === 1
                    ? qsTr("See your tabs with %1. Use %2 to group tabs into workspaces for different parts of your day.").arg(tour.shortcut("tabs")).arg(tour.shortcut("workspaces"))
                    : qsTr("Split left/right with %1, or top/bottom with %2. Drag the divider to resize. Hold %3 and drag a page onto another to swap them.").arg(tour.shortcut("splitHorizontal")).arg(tour.shortcut("splitVertical")).arg(Qt.platform.os === "osx" ? qsTr("Command + Shift") : qsTr("Control + Shift"))
            color: Theme.muted; Layout.fillWidth: true; wrapMode: Text.Wrap; textFormat: Text.PlainText
        }
        RowLayout {
            UiButton {
                text: tour.step === 0 ? qsTr("Try an address") : tour.step === 1 ? qsTr("Show tabs") : qsTr("Try a split")
                onClicked: { if (tour.step === 0) tour.addressRequested(); else if (tour.step === 1) tour.tabsRequested(); else tour.splitRequested() }
            }
            UiButton { text: qsTr("Workspaces"); visible: tour.step === 1; onClicked: tour.workspacesRequested() }
        }
        RowLayout {
            Layout.fillWidth: true
            UiButton { text: qsTr("Back"); visible: tour.step > 0; onClicked: --tour.step }
            Item { Layout.fillWidth: true }
            UiButton {
                objectName: "tutorialNext"
                primary: true
                text: tour.step === 2 ? qsTr("Happy browsing") : qsTr("Next")
                onClicked: { if (tour.step < 2) ++tour.step; else tour.controller.dismissTutorial() }
            }
        }
    }
}
