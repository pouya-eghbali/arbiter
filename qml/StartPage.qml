pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic as Basic

FocusScope {
    id: start
    property var history: null
    property var tabs: null
    property int currentTabId: 0
    required property string addressShortcut
    required property string tabsShortcut
    readonly property alias addressField: addressPrompt.addressField
    readonly property bool compact: width < 400 || height < 300
    signal navigateRequested(string text)
    signal tabRequested(int tabId)
    signal tabsRequested()
    signal settingsRequested()
    function focusAddress(select: bool) {
        addressPrompt.focusAddress(select)
    }
    Column {
        anchors.centerIn: parent
        width: Math.max(0, Math.min(760, start.width - (start.compact ? 24 : 64)))
        spacing: start.compact ? 10 : 16
        Basic.Label {
            anchors.horizontalCenter: parent.horizontalCenter
            text: qsTr("Arbiter")
            font.pixelSize: start.compact ? 26 : 38
            font.weight: Font.Medium
            color: Theme.text
        }
        AddressPrompt {
            id: addressPrompt
            width: parent.width
            maximumHeight: Math.max(64, start.height - (start.compact ? 130 : 190))
            history: start.history
            tabs: start.tabs
            currentTabId: start.currentTabId
            onNavigateRequested: function(input) { start.navigateRequested(input) }
            onTabRequested: function(tabId) { start.tabRequested(tabId) }
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
