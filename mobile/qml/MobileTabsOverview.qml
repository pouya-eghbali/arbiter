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
        width: Math.min(parent.width - 40, 560)
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.topMargin: 12
        anchors.bottomMargin: 12
        spacing: 15
        RowLayout {
            Layout.fillWidth: true
            Controls.AbstractButton {
                Layout.fillWidth: true
                implicitHeight: 48
                Accessible.name: root.workspaceName
                onClicked: root.workspacesRequested()
                contentItem: RowLayout {
                    spacing: 8
                    Text {
                        Layout.fillWidth: true
                        text: root.workspaceName
                        textFormat: Text.PlainText
                        font.pixelSize: 22
                        color: Theme.text
                        font.weight: Font.DemiBold
                        elide: Text.ElideRight
                    }
                    UiIcon {
                        name: "chevron-down"
                        color: Theme.muted
                        Layout.preferredWidth: 18
                        Layout.preferredHeight: 18
                    }
                }
            }
            MobileIconButton {
                iconName: "x"
                label: qsTr("Done")
                onClicked: root.dismiss()
            }
        }
        MobileField {
            Layout.fillWidth: true
            placeholderText: qsTr("Search tabs")
            onTextEdited: root.query = text
        }
        GridView {
            id: grid
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 0
            clip: true
            cellWidth: width / 2
            cellHeight: Math.max(152, Math.min(180, cellWidth * .96))
            boundsBehavior: Flickable.StopAtBounds
            model: root.tabs.filter(t => !root.query || (t.title || "").toLowerCase().includes(root.query.toLowerCase()) || (t.url || "").toLowerCase().includes(root.query.toLowerCase()))
            Controls.ScrollBar.vertical: Controls.ScrollBar {}
            MobileEmptyState {
                anchors.centerIn: parent
                width: parent.width
                visible: grid.count === 0
                title: qsTr("No matching tabs")
            }
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
            Item {
                anchors.fill: parent
                anchors.margins: 5
                MobileIconButton {
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    iconName: "venetian-mask"
                    label: qsTr("Private browsing")
                    iconColor: browser.privateMode ? Theme.accent : Theme.text
                    onClicked: browser.togglePrivate()
                }
                MobileIconButton {
                    id: addTabButton
                    anchors.centerIn: parent
                    label: qsTr("New tab")
                    iconName: "plus"
                    primary: true
                    implicitWidth: 48
                    implicitHeight: 48
                    background: Rectangle {
                        radius: 24
                        color: addTabButton.primaryFill
                    }
                    onClicked: {
                        browser.addTab();
                        root.dismiss();
                    }
                }
                UiButton {
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    flat: true
                    text: qsTr("Done")
                    onClicked: root.dismiss()
                }
            }
        }
    }
}
