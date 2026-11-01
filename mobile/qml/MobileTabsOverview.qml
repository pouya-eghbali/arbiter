pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic as Controls
import QtQuick.Layouts
import Arbiter

MobilePresentation {
    id: root
    required property var tabs
    required property string workspaceName
    signal dismiss
    signal workspacesRequested
    property string query: ""
    NativeSheet {
        anchors.fill: parent
        anchors.margins: 12
        anchors.topMargin: 12 + 16 * (1 - root.progress)
        anchors.bottomMargin: 12 - 16 * (1 - root.progress)
        interactive: root.presented
        onEscapeRequested: root.dismiss()
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
                objectName: "mobileWorkspaceSelectorButton"
                Accessible.name: root.workspaceName
                onClicked: root.workspacesRequested()
                contentItem: RowLayout {
                    spacing: 8
                    Text {
                        Layout.fillWidth: true
                        text: root.workspaceName
                        textFormat: Text.PlainText
                        font.pixelSize: 20 * MobileMetrics.textScale
                        color: Theme.textFor(parent)
                        font.weight: Font.DemiBold
                        elide: Text.ElideRight
                    }
                    UiIcon {
                        name: "chevron-down"
                        color: Theme.mutedFor(parent)
                        Layout.preferredWidth: 18
                        Layout.preferredHeight: 18
                    }
                }
            }
            MobileCloseButton {
                objectName: "mobileTabsDoneButton"
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
            cellWidth: width / (MobileMetrics.textScale >= 1.6 ? 1 : 2)
            cellHeight: Math.max(152, Math.min(180, cellWidth * .96)) + 42 * (MobileMetrics.textScale - 1)
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
                    id: tabCard
                    anchors.fill: parent
                    anchors.margins: 5
                    radius: 16
                    readonly property bool selected: cell.modelData.id === browser.activeId
                    readonly property bool nativeMaterial: Theme.nativeFor(tabCard)
                    color: nativeMaterial ? Qt.alpha(Theme.textFor(tabCard), selected ? .10 : .025)
                        : selected ? Qt.alpha(Theme.accent, .18) : Theme.fieldFor(parent)
                    border.width: selected ? 1 : 0
                    border.color: nativeMaterial ? Qt.alpha(Theme.textFor(tabCard), .28) : Theme.accent
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
                            color: Theme.textFor(parent)
                            font.pixelSize: 12 * MobileMetrics.textScale
                            elide: Text.ElideRight
                        }
                        Text {
                            Layout.fillWidth: true
                            textFormat: Text.PlainText
                            text: (cell.modelData.url || "").replace(/^https?:\/\//, "").split("/")[0]
                            color: Theme.mutedFor(parent)
                            font.pixelSize: 10 * MobileMetrics.textScale
                            elide: Text.ElideRight
                        }
                    }
                    MobileCloseButton {
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.margins: 3
                        label: qsTr("Close tab")
                        objectName: "closeTab-" + cell.modelData.id
                        overPreview: true
                        onClicked: browser.closeTab(cell.modelData.id)
                    }
                }
            }
        }
        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: Math.max(61, doneButton.implicitHeight + 10)
            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                height: 1
                color: Theme.borderFor(parent)
            }
            Item {
                anchors.fill: parent
                anchors.margins: 5
                MobileIconButton {
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    iconName: "venetian-mask"
                    label: qsTr("Private browsing")
                    iconColor: browser.privateMode && !nativeMaterial ? Theme.accent : Theme.textFor(parent)
                    checked: browser.privateMode
                    onClicked: browser.togglePrivate()
                }
                MobileIconButton {
                    id: addTabButton
                    anchors.centerIn: parent
                    label: qsTr("New tab")
                    iconName: "plus"
                    primary: true
                    iconColor: nativeMaterial ? Theme.textFor(addTabButton) : foregroundColor
                    implicitWidth: 48
                    implicitHeight: 48
                    background: Rectangle {
                        radius: 24
                        color: addTabButton.nativeMaterial ? Qt.alpha(Theme.textFor(addTabButton), addTabButton.down ? .22 : .13) : addTabButton.primaryFill
                    }
                    onClicked: {
                        browser.addTab();
                        root.dismiss();
                    }
                }
                MobileButton {
                    id: doneButton
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    flat: true
                    text: qsTr("Done")
                    onClicked: root.dismiss()
                }
            }
        }
    }    }

}
