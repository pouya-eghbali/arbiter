pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic as Basic

FocusScope {
    property bool privateMode: false
    property bool adBlockEnabled: true
    property bool adBlockActive: true
    property int adBlockBlocked: 0
    signal adBlockRequested()
    function dismissMenu() { more.close() }
    signal privateWindowRequested()
    signal lockPrivateRequested()
    id: pill
    required property var view
    required property var appearance
    required property string workspaceName
    required property color workspaceColor
    required property int tabCount
    property bool revealed: false
    readonly property bool expanded: revealed || activeFocus || more.visible
    signal addressRequested()
    signal tabsRequested()
    signal passwordsRequested()
    signal settingsRequested()
    signal historyRequested()
    signal workspacesRequested()
    signal splitHorizontalRequested()
    signal splitVerticalRequested()
    signal splitWithRequested()
    readonly property real expandedWidth: Math.min(560, parent.width - 32)
    property real revealProgress: expanded ? 1 : 0
    width: expanded ? expandedWidth : 76
    height: expanded ? 56 : 28
    Behavior on revealProgress { NumberAnimation { duration: 240; easing.type: Easing.OutCubic } }
    onVisibleChanged: if (!visible) { revealed = false; focus = false; collapse.stop() }
    onActiveFocusChanged: if (!activeFocus && !hover.hovered) collapse.restart()
    Keys.onEscapePressed: { revealed = false; focus = false; if (view) view.forceActiveFocus() }
    HoverHandler {
        id: hover
        objectName: "navigationHover"
        onHoveredChanged: {
            if (hovered) { collapse.stop(); pill.revealed = true }
            else collapse.restart()
        }
    }
    Timer { id: collapse; interval: 1000; onTriggered: if (!hover.hovered && !pill.activeFocus && !more.visible) pill.revealed = false }
    GlassSurface {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 10 * (1 - pill.revealProgress)
        width: 48 + (pill.expandedWidth - 48) * pill.revealProgress
        height: 6 + 50 * pill.revealProgress
        radius: height / 2
    }
    UiButton {
        anchors.fill: parent
        visible: !pill.expanded
        text: qsTr("Show navigation")
        Accessible.name: text
        background: Item { }
        contentItem: Item { }
        onClicked: { pill.revealed = true; addressButton.forceActiveFocus() }
        onActiveFocusChanged: if (activeFocus) pill.revealed = true
    }
    component NavButton: UiButton {
        id: button
        implicitWidth: 36
        implicitHeight: 36
        padding: 6
        horizontalPadding: 6
        font.pixelSize: 20
        flat: true
        UiToolTip {
            visible: button.hovered && pill.revealProgress > 0.99
            text: button.Accessible.name
        }
    }
    RowLayout {
        width: pill.expandedWidth - 20
        height: 36
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 10
        spacing: 3
        opacity: Math.max(0, (pill.revealProgress - 0.85) / 0.15)
        visible: opacity > 0
        enabled: pill.expanded && pill.revealProgress > 0.95
        NavButton { text: "‹"; Accessible.name: qsTr("Back"); enabled: pill.view && pill.view.canGoBack; onClicked: { pill.view.goBack(); pill.view.forceActiveFocus() } }
        NavButton { text: "›"; Accessible.name: qsTr("Forward"); enabled: pill.view && pill.view.canGoForward; onClicked: { pill.view.goForward(); pill.view.forceActiveFocus() } }
        NavButton {
            text: pill.view && pill.view.loading ? "×" : "↻"
            Accessible.name: pill.view && pill.view.loading ? qsTr("Stop loading") : qsTr("Reload")
            enabled: pill.view !== null
            onClicked: { if (pill.view.loading) pill.view.stop(); else pill.view.reload(); pill.view.forceActiveFocus() }
        }
        Rectangle { Layout.preferredWidth: 1; Layout.preferredHeight: 20; color: Theme.border }
        UiButton {
            id: addressButton
            Layout.fillWidth: true
            Layout.minimumWidth: 40
            implicitHeight: 36
            padding: 8
            hoverEnabled: true
            text: pill.appearance.displayAddress(pill.view ? pill.view.url : "about:blank")
            Accessible.name: qsTr("Edit address: %1").arg(text)
            contentItem: Basic.Label { text: addressButton.text; color: Theme.text; elide: Text.ElideMiddle; verticalAlignment: Text.AlignVCenter; horizontalAlignment: Text.AlignHCenter; font.pixelSize: 13 }
            flat: true
            onClicked: pill.addressRequested()
        }
        Rectangle { Layout.preferredWidth: 1; Layout.preferredHeight: 20; color: Theme.border }
        NavButton {
            text: "●"
            Accessible.name: qsTr("Workspaces: %1").arg(pill.workspaceName)
            contentItem: Basic.Label { text: "●"; color: pill.workspaceColor; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
            onClicked: pill.workspacesRequested()
        }
        NavButton {
            objectName: "adBlockPillButton"
            visible: pill.expandedWidth >= 440
            enabled: pill.adBlockEnabled
            Accessible.name: pill.adBlockActive ? qsTr("Pause ad blocking in this view (%1 blocked)").arg(pill.adBlockBlocked) : qsTr("Resume ad blocking in this view")
            contentItem: UiIcon { name: "shield"; color: pill.adBlockActive ? Theme.accent : Theme.muted }
            onClicked: pill.adBlockRequested()
        }
        NavButton { objectName: "navigationTabsButton"; text: String(pill.tabCount); font.pixelSize: 13; Accessible.name: pill.tabCount === 1 ? qsTr("Show 1 tab") : qsTr("Show %1 tabs").arg(pill.tabCount); onClicked: pill.tabsRequested() }
        NavButton { text: "⋯"; Accessible.name: qsTr("More browser controls"); onClicked: more.open() }
    }
    Basic.Menu {
        id: more
        y: -height - 8
        x: Math.max(0, pill.width - width)
        palette.text: Theme.text
        palette.windowText: Theme.text
        palette.base: Theme.window
        palette.highlight: Theme.selection
        palette.highlightedText: Theme.text
        padding: 6
        background: GlassSurface { dense: true }
        onOpened: pill.revealed = true
        onClosed: collapse.restart()
        UiMenuItem { text: qsTr("Split left / right"); onTriggered: pill.splitHorizontalRequested() }
        UiMenuItem { text: qsTr("Split top / bottom"); onTriggered: pill.splitVerticalRequested() }
        UiMenuItem { text: qsTr("Split with…"); onTriggered: pill.splitWithRequested() }
        UiMenuItem { text: qsTr("Workspaces…"); onTriggered: pill.workspacesRequested() }
        UiMenuItem { text: pill.adBlockActive ? qsTr("Pause ad blocking in this view") : qsTr("Resume ad blocking in this view"); enabled: pill.adBlockEnabled; onTriggered: pill.adBlockRequested() }
        UiMenuItem { text: qsTr("History…"); onTriggered: pill.historyRequested() }
        UiMenuItem { text: qsTr("Passwords…"); onTriggered: pill.passwordsRequested() }
        UiMenuItem { text: qsTr("New private window"); onTriggered: pill.privateWindowRequested() }
        UiMenuItem { visible: pill.privateMode; text: qsTr("Lock private window"); onTriggered: pill.lockPrivateRequested() }
        UiMenuItem { text: qsTr("Settings…"); onTriggered: pill.settingsRequested() }
    }
}
