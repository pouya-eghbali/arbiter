pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic as Controls
import QtQuick.Layouts
import Arbiter

Item {
    id: root
    signal dismiss
    property string editingId: ""
    function edit(id, name) {
        editingId = id;
        workspaceName.text = name;
        editor.open();
    }
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 16
        RowLayout {
            Layout.fillWidth: true
            Text {
                text: qsTr("Workspaces")
                color: Theme.text
                font.pixelSize: 24
                font.weight: Font.DemiBold
                Layout.fillWidth: true
            }
            MobileIconButton {
                iconName: "plus"
                label: qsTr("New workspace")
                onClicked: root.edit("", "")
            }
            MobileIconButton {
                iconName: "x"
                label: qsTr("Done")
                onClicked: root.dismiss()
            }
        }
        Controls.ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: availableWidth
            clip: true
            ColumnLayout {
                width: parent.width
                spacing: 12
                Repeater {
                    model: browser.workspaces
                    delegate: Rectangle {
                        id: card
                        required property var modelData
                        readonly property var previews: browser.tabs.filter(t => t.workspace === modelData.id && Boolean(t.private) === browser.privateMode)
                        Layout.fillWidth: true
                        implicitHeight: body.implicitHeight + 28
                        radius: 18
                        color: modelData.id === browser.workspaceId ? Qt.alpha(Theme.accent, .12) : Theme.field
                        border.width: modelData.id === browser.workspaceId ? 1.3 : 0
                        border.color: Qt.alpha(Theme.accent, .75)
                        UiButton {
                            anchors.fill: parent
                            flat: true
                            text: ""
                            background: Item {}
                            Accessible.name: card.modelData.name
                            onClicked: {
                                browser.selectWorkspace(card.modelData.id);
                                root.dismiss();
                            }
                            onPressAndHold: root.edit(card.modelData.id, card.modelData.name)
                        }
                        ColumnLayout {
                            id: body
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.margins: 14
                            spacing: 10
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 10
                                Rectangle {
                                    width: 13
                                    height: 13
                                    radius: 7
                                    color: card.modelData.color || Theme.accent
                                }
                                Text {
                                    textFormat: Text.PlainText
                                    text: card.modelData.name
                                    color: Theme.text
                                    font.pixelSize: 16
                                    font.weight: Font.Medium
                                    Layout.fillWidth: true
                                    elide: Text.ElideRight
                                }
                                MobileIconButton {
                                    iconName: "ellipsis"
                                    label: qsTr("Edit workspace")
                                    implicitWidth: 32
                                    implicitHeight: 32
                                    iconSize: 18
                                    onClicked: root.edit(card.modelData.id, card.modelData.name)
                                }
                                Rectangle {
                                    width: 23
                                    height: 23
                                    radius: 12
                                    color: card.modelData.id === browser.workspaceId ? Theme.accent : "transparent"
                                    border.width: 1
                                    border.color: card.modelData.id === browser.workspaceId ? Theme.accent : Theme.muted
                                    UiIcon {
                                        anchors.centerIn: parent
                                        width: 14
                                        height: 14
                                        name: "check"
                                        visible: card.modelData.id === browser.workspaceId
                                        color: Theme.contrastingText(Theme.accent)
                                    }
                                }
                            }
                            Repeater {
                                model: card.previews.slice(0, 3)
                                delegate: RowLayout {
                                    required property var modelData
                                    spacing: 9
                                    Layout.fillWidth: true
                                    UiIcon {
                                        name: modelData.sleeping ? "moon" : "globe"
                                        color: Theme.muted
                                        Layout.preferredWidth: 16
                                        Layout.preferredHeight: 16
                                    }
                                    Text {
                                        Layout.fillWidth: true
                                        textFormat: Text.PlainText
                                        text: (modelData.title || qsTr("New tab")) + (modelData.url ? " — " + modelData.url.replace(/^https?:\/\//, "").split("/")[0] : "")
                                        color: Theme.muted
                                        font.pixelSize: 12
                                        elide: Text.ElideRight
                                    }
                                }
                            }
                            Text {
                                visible: card.previews.length > 3
                                text: "+ " + (card.previews.length === 4 ? qsTr("1 other tab") : qsTr("%1 other tabs").arg(card.previews.length - 3))
                                color: Theme.muted
                                font.pixelSize: 12
                                Layout.topMargin: 2
                            }
                            Text {
                                visible: card.previews.length === 0
                                text: qsTr("No open tabs")
                                color: Theme.muted
                                font.pixelSize: 12
                            }
                        }
                    }
                }
            }
        }
    }
    Controls.Popup {
        id: editor
        anchors.centerIn: parent
        width: Math.min(parent.width - 24, 380)
        padding: 20
        modal: true
        background: GlassSurface {
            dense: true
            radius: 24
        }
        contentItem: ColumnLayout {
            spacing: 14
            Text {
                text: root.editingId ? qsTr("Edit workspace") : qsTr("New workspace")
                font.pixelSize: 21
                color: Theme.text
            }
            UiTextField {
                id: workspaceName
                Layout.fillWidth: true
                placeholderText: qsTr("Workspace name")
                onAccepted: save.clicked()
            }
            UiCheckBox {
                id: isolated
                visible: !root.editingId
                text: qsTr("Use a separate session")
            }
            RowLayout {
                Layout.fillWidth: true
                UiButton {
                    visible: Boolean(root.editingId) && browser.workspaces.length > 1
                    iconName: "trash-2"
                    text: qsTr("Delete")
                    onClicked: {
                        browser.removeWorkspace(root.editingId);
                        editor.close();
                    }
                }
                Item {
                    Layout.fillWidth: true
                }
                UiButton {
                    id: save
                    text: qsTr("Save")
                    primary: true
                    enabled: workspaceName.text.trim().length > 0
                    onClicked: {
                        if (root.editingId)
                            browser.renameWorkspace(root.editingId, workspaceName.text);
                        else
                            browser.addWorkspace(workspaceName.text, isolated.checked);
                        editor.close();
                    }
                }
            }
        }
        onOpened: workspaceName.forceActiveFocus()
        onClosed: Qt.inputMethod.hide()
    }
}
