pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic as Controls
import QtQuick.Layouts
import Arbiter

Item {
    id: root
    objectName: "mobilePill"
    required property var current
    readonly property bool nativeActive: nativeGlass.active
    readonly property color pillText: nativeActive ? nativeGlass.foregroundColor : Theme.text
    readonly property color pillMuted: nativeActive ? Qt.alpha(nativeGlass.foregroundColor, .72) : Theme.muted
    property int tabCount: 1
    property bool suggestions: false
    property point lastDrag: Qt.point(0, 0)
    signal searchRequested
    signal tabsRequested
    signal moreRequested
    signal cycleRequested(int direction)
    implicitHeight: suggestions ? 106 : 58
    NativeGlassHost {
        id: nativeGlass
        anchors.fill: parent
        requested: browser.settings.liquidGlass === true
        radius: 29
        content: pillContent
    }
    GlassSurface {
        visible: !nativeGlass.active
        forceOpaque: nativeGlass.reducedTransparency
        anchors.fill: parent
        radius: 29
        dense: true
    }
    Item {
        id: pillContent
        objectName: "mobilePillContent"
        anchors.fill: parent
        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 5
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
                            font.pixelSize: 14
                            elide: Text.ElideRight
                            verticalAlignment: Text.AlignVCenter
                        }
                    }
                }
                Controls.AbstractButton {
                    Layout.preferredWidth: 44
                    Layout.fillHeight: true
                    Accessible.name: qsTr("Tabs")
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
