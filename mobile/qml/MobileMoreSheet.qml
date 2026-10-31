pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic as Controls
import QtQuick.Layouts
import Arbiter

Item {
    id: root
    property bool blocking: true
    property bool hasPage: true
    signal dismiss
    signal action(string name)
    Rectangle {
        anchors.fill: parent
        color: Qt.alpha(Theme.window, .35)
        TapHandler {
            onTapped: root.dismiss()
        }
    }
    Item {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: Math.min(parent.height - 32, body.implicitHeight + 40)
        GlassSurface {
            anchors.fill: parent
            radius: 29
            dense: true
        }
        ColumnLayout {
            id: body
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 20
            spacing: 12
            Rectangle {
                Layout.alignment: Qt.AlignHCenter
                width: 32
                height: 4
                radius: 2
                color: Qt.alpha(Theme.muted, .42)
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
                            iconName: modelData.icon
                            label: modelData.label
                            enabled: root.hasPage
                            implicitWidth: 48
                            implicitHeight: 48
                            onClicked: root.action(modelData.action)
                            background: Rectangle {
                                radius: 24
                                color: Theme.field
                            }
                        }
                        Text {
                            Layout.alignment: Qt.AlignHCenter
                            text: modelData.label
                            color: Theme.text
                            font.pixelSize: 12
                        }
                    }
                }
            }
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
                    required property var modelData
                    Layout.fillWidth: true
                    implicitHeight: 43
                    Accessible.name: modelData.label
                    visible: modelData.action !== "block" || root.hasPage
                    onClicked: root.action(modelData.action)
                    background: Rectangle {
                        radius: 12
                        color: row.down ? Theme.hover : "transparent"
                    }
                    contentItem: RowLayout {
                        spacing: 13
                        UiIcon {
                            name: row.modelData.icon
                            color: Theme.text
                            Layout.preferredWidth: 21
                            Layout.preferredHeight: 21
                        }
                        Text {
                            Layout.fillWidth: true
                            text: row.modelData.label
                            color: Theme.text
                            font.pixelSize: 14
                            elide: Text.ElideRight
                        }
                        UiIcon {
                            name: "chevron-right"
                            color: Theme.muted
                            Layout.preferredWidth: 16
                            Layout.preferredHeight: 16
                        }
                    }
                    Rectangle {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        height: 1
                        color: Qt.alpha(Theme.text, .06)
                    }
                }
            }
        }
    }
}
