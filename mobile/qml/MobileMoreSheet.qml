pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic as Controls
import QtQuick.Layouts
import Arbiter

MobilePresentation {
    id: root
    readonly property color menuText: nativeGlass.active ? nativeGlass.foregroundColor : Theme.textFor(parent)
    readonly property color menuMuted: nativeGlass.active ? Qt.alpha(nativeGlass.foregroundColor, .65) : Theme.mutedFor(parent)
    property real bottomInset: 0
    property bool blocking: true
    property bool hasPage: true
    signal dismiss
    signal action(string name)
    Rectangle {
        anchors.fill: parent
        color: Qt.alpha(Theme.window, .35)
        TapHandler {
            enabled: root.presented
            onTapped: root.dismiss()
        }
    }
    Item {
        width: Math.min(parent.width, 560)
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: -16 * (1 - root.progress)
        height: Math.min(parent.height - 32, body.implicitHeight + 40 + root.bottomInset)
        NativeGlassHost {
            id: nativeGlass
            anchors.fill: parent
            requested: browser.settings.liquidGlass === true
            radius: 29
            onEscapeRequested: root.dismiss()
            content: menuContent
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
            radius: 29
            dense: true
        }
        Item {
            id: menuContent
            enabled: root.presented
            objectName: "mobileMenuContent"
            anchors.fill: parent
            Controls.ScrollView {
                id: menuScroll
                anchors.fill: parent
                anchors.margins: 20
                anchors.bottomMargin: 20 + root.bottomInset
                contentWidth: availableWidth
                clip: true
                ColumnLayout {
                    id: body
                    width: menuScroll.availableWidth
                    spacing: 12
                    Item {
                        Layout.fillWidth: true
                        implicitHeight: 32
                        Rectangle {
                            anchors.centerIn: parent
                            width: 32
                            height: 4
                            radius: 2
                            color: Qt.alpha(root.menuText, .3)
                        }
                        MobileIconButton {
                            objectName: "mobileMenuCloseButton"
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            iconName: "x"
                            label: qsTr("Close")
                            nativeMaterial: nativeGlass.active
                            iconColor: root.menuText
                            implicitWidth: 32
                            implicitHeight: 32
                            iconSize: 18
                            onClicked: root.dismiss()
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Layout.topMargin: 5
                        Layout.bottomMargin: 6
                        Repeater {
                            model: [
                                {
                                    icon: "share-2",
                                    label: qsTr("Share"),
                                    action: "share"
                                },
                                {
                                    icon: "rotate-cw",
                                    label: qsTr("Reload"),
                                    action: "reload"
                                },
                                {
                                    icon: "search",
                                    label: qsTr("Find"),
                                    action: "find"
                                }
                            ]
                            delegate: ColumnLayout {
                                required property var modelData
                                Layout.fillWidth: true
                                Layout.preferredWidth: (root.width - 40) / 3
                                Layout.maximumWidth: 10000
                                spacing: 8
                                MobileIconButton {
                                    Layout.alignment: Qt.AlignHCenter
                                    nativeMaterial: nativeGlass.active
                                    iconColor: root.menuText
                                    iconName: modelData.icon
                                    label: modelData.label
                                    enabled: root.hasPage
                                    implicitWidth: 48
                                    implicitHeight: 48
                                    onClicked: root.action(modelData.action)
                                    background: Rectangle {
                                        radius: 24
                                        color: nativeGlass.active ? Qt.alpha(root.menuText, .08) : Theme.fieldFor(parent)
                                    }
                                }
                                Text {
                                    Layout.alignment: Qt.AlignHCenter
                                    text: modelData.label
                                    color: root.menuText
                                    font.pixelSize: 12
                                }
                            }
                        }
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 0
                        Repeater {
                            model: [
                                {
                                    icon: "history",
                                    label: qsTr("History"),
                                    action: "history"
                                },
                                {
                                    icon: "download",
                                    label: qsTr("Downloads"),
                                    action: "downloads"
                                },
                                {
                                    icon: "key-round",
                                    label: qsTr("Passwords"),
                                    action: "vault"
                                },
                                {
                                    icon: root.blocking ? "shield-check" : "shield",
                                    label: root.blocking ? qsTr("Pause ad blocking for this site") : qsTr("Enable ad blocking for this site"),
                                    action: "block"
                                },
                                {
                                    icon: "venetian-mask",
                                    label: browser.privateMode ? qsTr("Return to regular browsing") : qsTr("New private tab"),
                                    action: "private"
                                },
                                {
                                    icon: "settings",
                                    label: qsTr("Settings"),
                                    action: "settings"
                                }
                            ]
                            delegate: Controls.AbstractButton {
                                id: row
                                objectName: "mobileMenu-" + modelData.action
                                required property var modelData
                                Layout.fillWidth: true
                                implicitHeight: 52
                                topPadding: 1
                                bottomPadding: 0
                                leftPadding: 4
                                rightPadding: 4
                                Accessible.name: modelData.label
                                visible: modelData.action !== "block" || root.hasPage
                                onClicked: root.action(modelData.action)
                                background: Rectangle {
                                    radius: 12
                                    color: row.down ? (nativeGlass.active ? Qt.alpha(root.menuText, .08) : Theme.hoverFor(parent)) : "transparent"
                                }
                                contentItem: RowLayout {
                                    spacing: 13
                                    UiIcon {
                                        name: row.modelData.icon
                                        color: root.menuText
                                        Layout.preferredWidth: 21
                                        Layout.preferredHeight: 21
                                    }
                                    Text {
                                        Layout.fillWidth: true
                                        Layout.fillHeight: true
                                        verticalAlignment: Text.AlignVCenter
                                        text: row.modelData.label
                                        color: root.menuText
                                        font.pixelSize: 14
                                        elide: Text.ElideRight
                                    }
                                    UiIcon {
                                        name: "chevron-right"
                                        color: root.menuMuted
                                        Layout.preferredWidth: 16
                                        Layout.preferredHeight: 16
                                    }
                                }
                                Rectangle {
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.top: parent.top
                                    height: 1
                                    color: Qt.alpha(root.menuText, .06)
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
