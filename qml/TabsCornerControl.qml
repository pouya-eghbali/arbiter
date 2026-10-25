pragma ComponentBehavior: Bound
import QtQuick

Item {
    id: controls
    required property string mode
    signal tabsRequested()
    // A small corner target leaves the rest of each page available for input.
    EdgeAction {
        objectName: "tabsEdgeAction"
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.topMargin: 20
        width: 52; height: 52
        mode: controls.mode
        iconName: "panels-top-left"
        label: qsTr("Show tabs")
        available: controls.width >= 180 && controls.height >= 160
        onTriggered: controls.tabsRequested()
    }
}
