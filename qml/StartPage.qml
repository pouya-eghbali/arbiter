pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic as Basic

FocusScope {
    id: start
    required property string addressShortcut
    required property string tabsShortcut
    readonly property alias addressField: address
    readonly property bool compact: width < 400 || height < 300
    signal navigateRequested(string text)
    signal tabsRequested()
    signal settingsRequested()
    function focusAddress(select: bool) {
        address.forceActiveFocus()
        if (select) address.selectAll()
    }
    Column {
        anchors.centerIn: parent
        width: Math.max(0, Math.min(560, start.width - (start.compact ? 24 : 64)))
        spacing: start.compact ? 10 : 16
        Basic.Label {
            anchors.horizontalCenter: parent.horizontalCenter
            text: qsTr("Arbiter")
            font.pixelSize: start.compact ? 26 : 38
            font.weight: Font.Medium
            color: Theme.text
        }
        UiTextField {
            id: address
            objectName: "startAddress"
            width: parent.width
            implicitHeight: start.compact ? 42 : 46
            font.pixelSize: 15
            placeholderText: qsTr("Search or enter address")
            Accessible.name: qsTr("Address and search")
            onAccepted: start.navigateRequested(text)
        }
        Basic.Label {
            visible: start.height >= 240
            anchors.horizontalCenter: parent.horizontalCenter
            text: qsTr("%1 to go anywhere").arg(start.addressShortcut)
            font.pixelSize: 12
            color: Theme.muted
        }
        RowLayout {
            visible: start.height >= 180
            width: parent.width
            spacing: 8
            UiButton {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                text: start.compact ? qsTr("Tabs") : qsTr("Tabs · %1").arg(start.tabsShortcut)
                flat: true
                onClicked: start.tabsRequested()
            }
            UiButton {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                text: qsTr("Settings")
                flat: true
                onClicked: start.settingsRequested()
            }
        }
    }
}
