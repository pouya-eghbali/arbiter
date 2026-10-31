pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic as Controls
import QtQuick.Layouts
import QtQuick.Effects
import Arbiter

Item {
    id: root
    signal navigate(string url)
    readonly property var sites: [
        {
            name: "Wikipedia",
            url: "https://www.wikipedia.org",
            glyph: "W",
            color: "transparent",
            ink: Theme.text
        },
        {
            name: "GitHub",
            url: "https://github.com",
            glyph: "G",
            color: "transparent",
            ink: Theme.text
        },
        {
            name: "YouTube",
            url: "https://www.youtube.com",
            glyph: "▶",
            color: "#ee3434",
            ink: "white"
        },
        {
            name: "Reddit",
            url: "https://www.reddit.com",
            glyph: "r",
            color: "#f2572b",
            ink: "white"
        },
        {
            name: "Hacker News",
            url: "https://news.ycombinator.com",
            glyph: "Y",
            color: "#ee7926",
            ink: "white"
        },
        {
            name: "MDN",
            url: "https://developer.mozilla.org",
            glyph: "M",
            color: "transparent",
            ink: Theme.text
        }
    ]
    ColumnLayout {
        width: Math.min(root.width - 64, 310)
        anchors.horizontalCenter: parent.horizontalCenter
        y: Math.max(34, root.height * .19)
        spacing: 28
        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: 10
            Image {
                source: "qrc:/brand/valkyrie-source.png"
                sourceSize.width: Math.ceil(32 * root.Screen.devicePixelRatio)
                Layout.preferredWidth: 32
                Layout.preferredHeight: 32
                layer.enabled: !Theme.dark
                layer.effect: MultiEffect {
                    colorization: 1
                    colorizationColor: Theme.text
                }
            }
            Text {
                text: "Arbiter"
                color: Theme.text
                font.pixelSize: 27
                font.weight: Font.Medium
            }
        }
        GridLayout {
            columns: 3
            columnSpacing: 12
            rowSpacing: 14
            Layout.fillWidth: true
            Repeater {
                model: root.sites
                delegate: Controls.AbstractButton {
                    id: tile
                    required property var modelData
                    Layout.fillWidth: true
                    Layout.preferredHeight: 91
                    Accessible.name: modelData.name
                    onClicked: root.navigate(modelData.url)
                    background: Rectangle {
                        radius: 15
                        color: tile.down ? Theme.selection : Theme.field
                        border.color: Qt.alpha(Theme.text, .06)
                        border.width: 1
                    }
                    contentItem: ColumnLayout {
                        spacing: 7
                        Item {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 40
                            Rectangle {
                                anchors.centerIn: parent
                                width: 30
                                height: 30
                                radius: tile.modelData.name === "Reddit" ? 15 : 7
                                color: tile.modelData.color
                                Text {
                                    anchors.centerIn: parent
                                    text: tile.modelData.glyph
                                    color: tile.modelData.ink
                                    font.pixelSize: tile.modelData.name === "Wikipedia" ? 29 : 23
                                    font.family: tile.modelData.name === "Wikipedia" ? "serif" : Qt.application.font.family
                                    font.weight: Font.Medium
                                }
                            }
                        }
                        Text {
                            Layout.fillWidth: true
                            text: tile.modelData.name
                            color: Theme.text
                            font.pixelSize: 11
                            horizontalAlignment: Text.AlignHCenter
                            elide: Text.ElideRight
                        }
                    }
                }
            }
        }
        Text {
            visible: browser.privateMode
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("Private browsing")
            font.pixelSize: 12
            color: Theme.muted
        }
    }
}
