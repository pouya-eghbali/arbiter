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
    function shareAnchor() { return shareButton.visible ? shareButton.mapToItem(null, shareButton.width / 2, shareButton.height / 2) : Qt.point(pill.x + pill.width / 2, pill.y) }
    function reveal() {
        collapse.stop()
        focusAddressPending = true
        revealFocusAnchor.forceActiveFocus(Qt.ShortcutFocusReason)
        revealed = true
        focusAddressIfReady()
    }
    function focusAddressIfReady() {
        if (!focusAddressPending || !expanded || revealProgress < 0.99) return
        focusAddressPending = false
        Qt.callLater(function() { if (pill.shown && pill.expanded && pill.activeFocus && !pill.forceHidden) addressButton.forceActiveFocus(Qt.ShortcutFocusReason) })
    }
    signal privateWindowRequested()
    signal lockPrivateRequested()
    id: pill
    required property var view
    required property var appearance
    required property string workspaceName
    required property color workspaceColor
    required property int tabCount
    property bool revealed: false
    property bool focusAddressPending: false
    readonly property bool edgeReveal: appearance.navigationRevealMode === "edge"
    property bool edgeHovered: false
    readonly property bool engaged: expanded || revealProgress > 0.001
    onEdgeHoveredChanged: {
        if (edgeHovered) { collapse.stop(); revealed = true }
        else collapse.restart()
    }
    property bool shown: true
    property bool forceHidden: false
    readonly property bool presented: shown && !forceHidden
    property real presenceProgress: presented ? 1 : 0
    readonly property real slideOffset: 10 * (1 - presenceProgress)
    visible: !forceHidden && (presented || presenceProgress > 0.001)
    enabled: presented
    opacity: presenceProgress
    Behavior on presenceProgress {
        NumberAnimation { duration: pill.forceHidden ? 0 : pill.presented ? Theme.enterDuration : Theme.exitDuration; easing.type: Easing.OutCubic }
    }
    readonly property bool expanded: revealed || activeFocus || more.visible
    signal shareRequested()
    signal extensionsRequested()
    property bool canShare: false
    signal addressRequested()
    signal tabsRequested()
    signal passwordsRequested()
    signal settingsRequested()
    signal historyRequested()
    signal developerToolsRequested()
    signal downloadsRequested()
    property int activeDownloads: 0
    signal workspacesRequested()
    signal splitHorizontalRequested()
    signal splitVerticalRequested()
    signal splitWithRequested()
    readonly property real expandedWidth: Math.min(560, parent.width - 32)
    property real revealProgress: expanded ? 1 : 0
    width: 76 + (expandedWidth - 76) * revealProgress
    height: 28 + 28 * revealProgress
    Behavior on revealProgress {
        enabled: pill.visible
        NumberAnimation { duration: Theme.reducedMotion ? 0 : 240; easing.type: Easing.OutCubic }
    }
    onRevealProgressChanged: focusAddressIfReady()
    onVisibleChanged: if (!visible) { focusAddressPending = false; revealed = false; focus = false; collapse.stop() }
    onActiveFocusChanged: if (!activeFocus && !hover.hovered) collapse.restart()
    Keys.onShortcutOverride: function(event) { if (event.key === Qt.Key_Escape && expanded) event.accepted = true }
    Keys.onEscapePressed: { revealed = false; focus = false; if (view) view.forceActiveFocus() }
    // Keep shortcut focus stable while the controls are still clipped/hidden.
    // Once the reveal finishes it moves to the address button.
    Item { id: revealFocusAnchor; Accessible.ignored: true }
    HoverHandler {
        id: hover
        objectName: "navigationHover"
        onHoveredChanged: {
            if (hovered) { collapse.stop(); pill.revealed = true }
            else collapse.restart()
        }
    }
    Timer { id: collapse; interval: 1000; onTriggered: if (!hover.hovered && !pill.edgeHovered && !pill.activeFocus && !more.visible) pill.revealed = false }
    GlassSurface {
        id: pillSurface
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
        onClicked: pill.reveal()
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
    Item {
        width: Math.max(0, pillSurface.width - 20)
        height: 36
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 10
        clip: true
        opacity: Math.max(0, (pill.revealProgress - 0.65) / 0.35)
        visible: opacity > 0
        enabled: pill.expanded && pill.revealProgress > 0.95
    RowLayout {
        width: pill.expandedWidth - 20
        height: 36
        anchors.centerIn: parent
        spacing: 3
        NavButton { iconName: "arrow-left"; Accessible.name: qsTr("Back"); enabled: pill.view && pill.view.canGoBack; onClicked: { pill.view.goBack(); pill.view.forceActiveFocus() } }
        NavButton { iconName: "arrow-right"; Accessible.name: qsTr("Forward"); enabled: pill.view && pill.view.canGoForward; onClicked: { pill.view.goForward(); pill.view.forceActiveFocus() } }
        NavButton {
            iconName: pill.view && pill.view.loading ? "x" : "rotate-cw"
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
            iconName: "layers"
            iconColor: pill.workspaceColor
            Accessible.name: qsTr("Workspaces: %1").arg(pill.workspaceName)
            onClicked: pill.workspacesRequested()
        }
        NavButton {
            objectName: "adBlockPillButton"
            visible: pill.expandedWidth >= 440
            enabled: pill.adBlockEnabled
            Accessible.name: pill.adBlockActive ? qsTr("Pause ad blocking in this view (%1 blocked)").arg(pill.adBlockBlocked) : qsTr("Resume ad blocking in this view")
            iconName: pill.adBlockActive ? "shield-check" : "shield"
            iconColor: pill.adBlockActive ? Theme.accent : Theme.muted
            onClicked: pill.adBlockRequested()
        }
        NavButton {
            id: shareButton
            objectName: "sharePillButton"
            visible: pill.expandedWidth >= 480
            enabled: pill.canShare
            Accessible.name: qsTr("Share current page")
            iconName: "share"
            iconColor: pill.canShare ? Theme.text : Theme.muted
            onPressed: pill.shareRequested()
        }
        NavButton { objectName: "navigationTabsButton"; text: String(pill.tabCount); font.pixelSize: 13; Accessible.name: pill.tabCount === 1 ? qsTr("Show 1 tab") : qsTr("Show %1 tabs").arg(pill.tabCount); onClicked: pill.tabsRequested() }
        NavButton { iconName: "ellipsis"; Accessible.name: qsTr("More browser controls"); onClicked: more.open() }
    }
    }
    UiMenu {
        preferredWidth: 320
        id: more
        y: -height - 8
        x: Math.max(0, pill.width - width)
        palette.text: Theme.text
        palette.windowText: Theme.text
        palette.base: Theme.window
        palette.highlight: Theme.selection
        palette.highlightedText: Theme.text
        onOpened: pill.revealed = true
        onClosed: collapse.restart()
        UiMenuItem { text: qsTr("Split left / right"); onTriggered: pill.splitHorizontalRequested() }
        UiMenuItem { text: qsTr("Split top / bottom"); onTriggered: pill.splitVerticalRequested() }
        UiMenuItem { text: qsTr("Split with…"); onTriggered: pill.splitWithRequested() }
        UiMenuItem { text: qsTr("Workspaces…"); onTriggered: pill.workspacesRequested() }
        UiMenuItem { text: pill.adBlockActive ? qsTr("Pause ad blocking in this view") : qsTr("Resume ad blocking in this view"); enabled: pill.adBlockEnabled; onTriggered: pill.adBlockRequested() }
        UiMenuItem { text: qsTr("Share current page…"); enabled: pill.canShare; onTriggered: pill.shareRequested() }
        UiMenuItem { text: qsTr("Extensions…"); visible: !pill.privateMode; onTriggered: pill.extensionsRequested() }
        UiMenuItem { text: qsTr("Developer tools…"); onTriggered: pill.developerToolsRequested() }
        UiMenuItem { text: pill.activeDownloads > 0 ? qsTr("Downloads (%1 active)…").arg(pill.activeDownloads) : qsTr("Downloads…"); onTriggered: pill.downloadsRequested() }
        UiMenuItem { text: qsTr("History…"); onTriggered: pill.historyRequested() }
        UiMenuItem { text: qsTr("Passwords…"); onTriggered: pill.passwordsRequested() }
        UiMenuItem { text: qsTr("New private window"); onTriggered: pill.privateWindowRequested() }
        UiMenuItem { visible: pill.privateMode; text: qsTr("Lock private window"); onTriggered: pill.lockPrivateRequested() }
        UiMenuItem { text: qsTr("Settings…"); onTriggered: pill.settingsRequested() }
    }
}
