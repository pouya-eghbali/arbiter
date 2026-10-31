pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic as Controls
import QtQuick.Layouts
import Arbiter

Item {
    id: root
    required property var tabs
    required property string workspaceName
    signal dismiss
    signal workspacesRequested
    property string query: ""
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 15
        RowLayout {
            Layout.fillWidth: true
            UiButton {
                flat: true
                text: root.workspaceName
                contentItem: RowLayout {
                    spacing: 7
                    Text {
                        text: root.workspaceName
                        font.pixelSize: 21
                        color: Theme.text
                        font.weight: Font.Medium
                    }
                    UiIcon {
                        name: "chevron-down"
                        color: Theme.text
                        Layout.preferredWidth: 17
                        Layout.preferredHeight: 17
                    }
                }
                textAlignment: Text.AlignLeft
                font.pixelSize: 21
                onClicked: root.workspacesRequested()
            }
            Item {
                Layout.fillWidth: true
            }
            MobileIconButton {
                iconName: "x"
                label: qsTr("Done")
                onClicked: root.dismiss()
            }
        }
        UiTextField {
            Layout.fillWidth: true
            placeholderText: qsTr("Search tabs")
            onTextEdited: root.query = text
        }
        GridView {
            id: grid
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            cellWidth: width / 2
            cellHeight: Math.max(152, Math.min(180, cellWidth * .96))
            boundsBehavior: Flickable.StopAtBounds
            model: root.tabs.filter(t => !root.query || (t.title || "").toLowerCase().includes(root.query.toLowerCase()) || (t.url || "").toLowerCase().includes(root.query.toLowerCase()))
            Controls.ScrollBar.vertical: Controls.ScrollBar {}
            delegate: Item {
                id: cell
                required property var modelData
                width: grid.cellWidth
                height: grid.cellHeight
                Rectangle {
                    anchors.fill: parent
                    anchors.margins: 5
                    radius: 16
                    color: cell.modelData.id === browser.activeId ? Qt.alpha(Theme.accent, .18) : Theme.field
                    border.width: cell.modelData.id === browser.activeId ? 2 : 0
                    border.color: Theme.accent
                    UiButton {
                        anchors.fill: parent
                        flat: true
                        text: ""
                        background: Item {}
                        Accessible.name: cell.modelData.title || qsTr("New tab")
                        onClicked: {
                            browser.selectTab(cell.modelData.id);
                            root.dismiss();
                        }
                    }
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 7
                        spacing: 5
                        MobileThumbnail {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            source: browser.previews[cell.modelData.id] || ""
                            fallback: cell.modelData.title || qsTr("New tab")
                        }
                        Text {
                            Layout.fillWidth: true
                            textFormat: Text.PlainText
                            text: cell.modelData.title || qsTr("New tab")
                            color: Theme.text
                            font.pixelSize: 12
                            elide: Text.ElideRight
                        }
                        Text {
                            Layout.fillWidth: true
                            textFormat: Text.PlainText
                            text: (cell.modelData.url || "").replace(/^https?:\/\//, "").split("/")[0]
                            color: Theme.muted
                            font.pixelSize: 10
                            elide: Text.ElideRight
                        }
                    }
                    MobileIconButton {
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.margins: 3
                        iconName: "x"
                        label: qsTr("Close tab")
                        objectName: "closeTab-" + cell.modelData.id
                        implicitWidth: 38
                        implicitHeight: 38
                        iconSize: 16
                        onClicked: browser.closeTab(cell.modelData.id)
                        background: Rectangle {
                            radius: 19
                            color: Theme.popupSolid
                        }
                    }
                }
            }
        }
        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: 61
            GlassSurface {
                anchors.fill: parent
                radius: 30
                dense: true
            }
            RowLayout {
                anchors.fill: parent
                anchors.margins: 5
                MobileIconButton {
                    iconName: "venetian-mask"
                    label: qsTr("Private browsing")
                    iconColor: browser.privateMode ? Theme.accent : Theme.text
                    onClicked: browser.togglePrivate()
                }
                Item {
                    Layout.fillWidth: true
                }
                UiButton {
                    id: addTabButton
                    text: ""
                    iconName: "plus"
                    primary: true
                    implicitWidth: 48
                    implicitHeight: 48
                    background: Rectangle {
                        radius: 24
                        color: addTabButton.primaryFill
                    }
                    Accessible.name: qsTr("New tab")
                    onClicked: {
                        browser.addTab();
                        root.dismiss();
                    }
                }
                Item {
                    Layout.fillWidth: true
                }
                UiButton {
                    flat: true
                    text: qsTr("Done")
                    onClicked: root.dismiss()
                }
            }
        }
    }
}
