pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic as Controls
import QtQuick.Layouts
import Arbiter

MobilePresentation {
    id: root
    objectName: "mobilePill"
    required property var current
    readonly property bool nativeActive: nativeGlass.active
    readonly property color pillText: nativeActive ? nativeGlass.foregroundColor : Theme.textFor(parent)
    readonly property color pillMuted: nativeActive ? Qt.alpha(nativeGlass.foregroundColor, .72) : Theme.mutedFor(parent)
    property real expandedWidth: width
    readonly property real expandedHeight: (suggestions ? 106 : 58) + Math.max(0, 18 * (MobileMetrics.textScale - 1))
    property bool collapsed: false
    readonly property bool compact: collapsed && Boolean(current.url) && !suggestions
    property real compactProgress: compact ? 1 : 0
    Behavior on compactProgress {
        NumberAnimation { duration: Theme.reducedMotion ? 0 : 260; easing.type: Easing.OutCubic }
    }
    // Reserve full space until collapse finishes, and restore it before
    // expansion starts. WebKit need not reflow the page on every visual frame.
    readonly property real viewportHeight: compact && compactProgress > .999 ? implicitHeight : expandedHeight
    function expand() { collapsed = false; expandedByTouch() }
    signal expandedByTouch
    property int tabCount: 1
    property bool suggestions: false
    onSuggestionsChanged: if (suggestions) collapsed = false
    property point lastDrag: Qt.point(0, 0)
    signal searchRequested
    signal tabsRequested
    signal moreRequested
    signal cycleRequested(int direction)
    implicitHeight: (suggestions ? 106 : 58 - 14 * compactProgress) + Math.max(0, 18 * (MobileMetrics.textScale - 1))
    NativeGlassHost {
        id: nativeGlass
        anchors.fill: parent
        requested: browser.settings.liquidGlass === true
        radius: root.height / 2
        content: pillContent
        minimumRenderSize: Qt.platform.os === "ios" ? Qt.size(root.expandedWidth, root.expandedHeight) : Qt.size(0, 0)
        WindowContainer {
            anchors.fill: parent
            window: nativeGlass.containerWindow
            visible: nativeGlass.active
        }
    }
    GlassSurface {
        visible: !nativeGlass.preferred
        forceOpaque: nativeGlass.reducedTransparency
        anchors.fill: parent
        radius: root.height / 2
        dense: true
    }
    Item {
        id: pillContent
        enabled: root.presented
        objectName: "mobilePillContent"
        width: root.width
        height: root.height
        clip: true
        Controls.AbstractButton {
            objectName: "mobileExpandPillButton"
            anchors.fill: parent
            visible: root.compactProgress > 0
            opacity: root.compactProgress
            enabled: root.compact
            Accessible.name: qsTr("Show browser controls")
            onClicked: root.expand()
            padding: 0
            contentItem: Item {
                RowLayout {
                    objectName: "mobileCompactAddressGroup"
                    anchors.centerIn: parent
                    width: Math.min(implicitWidth, Math.max(0, root.width - 32))
                    height: parent.height
                    spacing: 6
                    UiIcon {
                        name: root.current.url && root.current.url.startsWith("https:") ? "lock" : "globe"
                        color: root.pillMuted
                        Layout.preferredWidth: 13
                        Layout.preferredHeight: 13
                        Layout.alignment: Qt.AlignVCenter
                    }
                    Text {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.maximumWidth: Math.max(0, root.width - 51)
                        text: root.current.url ? root.current.url.replace(/^https?:\/\//, "").split("/")[0] : ""
                        textFormat: Text.PlainText
                        color: root.pillText
                        font.pixelSize: 13 * MobileMetrics.textScale
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideRight
                    }
                }
            }
        }
        ColumnLayout {
            visible: root.compactProgress < .99
            opacity: 1 - root.compactProgress
            enabled: !root.compact
            width: root.expandedWidth - 10
            height: root.expandedHeight - 10
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.verticalCenter: parent.verticalCenter
            spacing: 2
            RowLayout {
                visible: root.suggestions
                Layout.fillWidth: true
                spacing: 2
                UiButton {
                    nativeMaterial: root.nativeActive
                    iconColor: root.pillText
                    textColor: root.pillText
                    Layout.fillWidth: true
                    flat: true
                    text: qsTr("Passwords")
                    iconName: "key-round"
                    onClicked: browser.passwordAccounts()
                }
                UiButton {
                    nativeMaterial: root.nativeActive
                    iconColor: root.pillText
                    textColor: root.pillText
                    Layout.fillWidth: true
                    flat: true
                    text: qsTr("Suggest")
                    iconName: "sparkles"
                    onClicked: browser.suggestPassword()
                }
                MobileIconButton {
                    nativeMaterial: root.nativeActive
                    iconColor: root.pillText
                    iconName: "x"
                    label: qsTr("Dismiss")
                    onClicked: browser.dismissSuggestions()
                }
            }
            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 1
                MobileIconButton {
                    nativeMaterial: root.nativeActive
                    iconColor: root.pillText
                    visible: Boolean(root.current.url)
                    iconName: "arrow-left"
                    label: qsTr("Back")
                    enabled: Boolean(root.current.back)
                    onClicked: browser.pageAction(browser.activeId, "back", "")
                }
                MobileIconButton {
                    nativeMaterial: root.nativeActive
                    iconColor: root.pillText
                    visible: Boolean(root.current.forward)
                    iconName: "arrow-right"
                    label: qsTr("Forward")
                    onClicked: browser.pageAction(browser.activeId, "forward", "")
                }
                UiIcon {
                    visible: !root.current.url
                    name: "search"
                    color: root.pillMuted
                    Layout.leftMargin: 11
                    Layout.preferredWidth: 20
                    Layout.preferredHeight: 20
                }
                Controls.AbstractButton {
                    id: address
                    objectName: "mobileAddressButton"
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Accessible.name: qsTr("Search or enter address")
                    onClicked: root.searchRequested()
                    contentItem: RowLayout {
                        spacing: 6
                        UiIcon {
                            visible: Boolean(root.current.url)
                            name: root.current.url && root.current.url.startsWith("https:") ? "lock" : "globe"
                            color: root.pillMuted
                            Layout.preferredWidth: 14
                            Layout.preferredHeight: 14
                        }
                        Text {
                            Layout.fillWidth: true
                            textFormat: Text.PlainText
                            text: root.current.url ? root.current.url.replace(/^https?:\/\//, "").split("/")[0] : qsTr("Search or enter address")
                            color: root.current.url ? root.pillText : root.pillMuted
                            font.pixelSize: 14 * MobileMetrics.textScale
                            elide: Text.ElideRight
                            verticalAlignment: Text.AlignVCenter
                        }
                    }
                }
                Controls.AbstractButton {
                    Layout.preferredWidth: MobileMetrics.touchTarget
                    Layout.fillHeight: true
                    objectName: "mobileTabsButton"
                    Accessible.name: qsTr("Tabs") + ": " + root.tabCount
                    onClicked: root.tabsRequested()
                    contentItem: Item {
                        UiIcon {
                            anchors.centerIn: parent
                            name: "copy"
                            width: 23
                            height: 23
                            color: root.pillText
                        }
                        Rectangle {
                            anchors.centerIn: parent
                            anchors.horizontalCenterOffset: 3
                            anchors.verticalCenterOffset: 3
                            width: 13
                            height: 13
                            radius: 3
                            color: root.nativeActive ? Qt.alpha(root.pillText, .10) : Theme.popupSolid
                            Text {
                                anchors.centerIn: parent
                                text: Math.min(root.tabCount, 99)
                                // Keep the count inside its icon; expose the value to assistive technology.
                                font.pixelSize: 9
                                font.weight: Font.DemiBold
                                color: root.pillText
                            }
                        }
                    }
                }
                MobileIconButton {
                    nativeMaterial: root.nativeActive
                    iconColor: root.pillText
                    objectName: "mobileMoreButton"
                    iconName: "ellipsis"
                    label: qsTr("More")
                    onClicked: root.moreRequested()
                }
            }
        }
        Rectangle {
            visible: Boolean(root.current.loading)
            x: 20
            y: parent.height - 3
            width: Math.max(0, (parent.width - 40) * (root.current.progress || 0) / 100)
            height: 2
            radius: 1
            color: Theme.accent
        }
        DragHandler {
            target: null
            onActiveTranslationChanged: if (active && (activeTranslation.x !== 0 || activeTranslation.y !== 0))
                root.lastDrag = Qt.point(activeTranslation.x, activeTranslation.y)
            onActiveChanged: {
                if (active)
                    root.lastDrag = Qt.point(0, 0);
                else if (root.lastDrag.y < -36 && Math.abs(root.lastDrag.y) > Math.abs(root.lastDrag.x))
                    root.tabsRequested();
                else if (Math.abs(root.lastDrag.x) > 48)
                    root.cycleRequested(root.lastDrag.x < 0 ? 1 : -1);
            }
        }
    }
}
