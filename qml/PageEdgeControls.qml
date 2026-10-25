pragma ComponentBehavior: Bound
import QtQuick

Item {
    id: controls
    required property string mode
    property bool canGoBack: false
    property bool canGoForward: false
    signal backRequested()
    signal forwardRequested()
    signal tabsRequested()
    // Only small edge strips participate; the rest of each page receives input normally.
    EdgeAction {
        objectName: "backEdgeAction"
        anchors.left: parent.left
        anchors.verticalCenter: parent.verticalCenter
        width: 52; height: Math.min(160, Math.max(0, controls.height - 120))
        mode: controls.mode
        direction: "right"
        iconName: "arrow-left"
        label: qsTr("Back")
        available: controls.canGoBack && height > 0
        onTriggered: controls.backRequested()
    }
    EdgeAction {
        objectName: "forwardEdgeAction"
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        width: 52; height: Math.min(160, Math.max(0, controls.height - 120))
        mode: controls.mode
        direction: "left"
        iconName: "arrow-right"
        label: qsTr("Forward")
        available: controls.canGoForward && height > 0
        onTriggered: controls.forwardRequested()
    }
    EdgeAction {
        objectName: "tabsEdgeAction"
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.topMargin: 20
        width: 52; height: 52
        mode: controls.mode
        direction: "down-left"
        iconName: "panels-top-left"
        label: qsTr("Show tabs")
        available: controls.width >= 180 && controls.height >= 160
        onTriggered: controls.tabsRequested()
    }
}
