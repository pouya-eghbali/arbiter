pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic as Basic
import Arbiter

Basic.Popup {
    popupType: Basic.Popup.Item
    id: panel
    required property var manager
    required property var tabs
    property string editingId: ""
    property string deletingId: ""
    property string deletingName: ""
    property bool editing: false
    property color chosenColor: Theme.accent
    function focusSearch() { if (editing) nameField.forceActiveFocus(); else choices.forceActiveFocus() }
    function editWorkspace(space: var) {
        editingId = space ? space.id : ""
        nameField.text = space ? space.name : ""
        chosenColor = space ? space.color : Theme.accent
        isolated.checked = space ? space.isolated : false
        editing = true
        nameField.forceActiveFocus()
    }
    parent: Basic.Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(780, parent.width - 32)
    height: Math.min(680, parent.height - 48)
    padding: 24
    modal: true
    dim: true
    focus: true
    closePolicy: Basic.Popup.CloseOnEscape | Basic.Popup.CloseOnPressOutside
    background: GlassSurface { dense: true; radius: Theme.panelRadius }
    Basic.Overlay.modal: Rectangle { color: Theme.scrim }
    onOpened: {
        editing = false
        deletingId = ""
        choices.currentIndex = manager.choices.findIndex(space => space.id === manager.currentId)
        choices.forceActiveFocus()
    }
    contentItem: ColumnLayout {
        spacing: 12
        RowLayout {
            Layout.fillWidth: true
            Basic.Label { text: qsTr("Workspaces"); color: Theme.text; font.pixelSize: 24; font.weight: Font.DemiBold; Layout.fillWidth: true }
            UiButton { text: qsTr("New…"); onClicked: panel.editWorkspace(null) }
            UiIconButton { label: qsTr("Close"); iconName: "x"; onClicked: panel.close() }
        }
        Basic.Label {
            visible: panel.manager.error.length > 0
            Layout.fillWidth: true
            text: panel.manager.error
            textFormat: Text.PlainText
            wrapMode: Text.Wrap
            color: Theme.text
        }
        Basic.ScrollView {
            visible: panel.editing
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: availableWidth
            ColumnLayout {
                width: parent.width
                spacing: 16
                UiTextField { id: nameField; objectName: "workspaceName"; Layout.fillWidth: true; maximumLength: 48; placeholderText: qsTr("Workspace name"); Accessible.name: placeholderText }
                UiColorPicker {
                    Layout.fillWidth: true
                    title: qsTr("Workspace color")
                    value: panel.chosenColor
                    defaultColor: Theme.accent
                    onColorSelected: color => panel.chosenColor = color
                }
                UiCheckBox { id: isolated; text: qsTr("Separate website sign-in session"); enabled: panel.editingId.length === 0 }
                Basic.Label {
                    text: qsTr("Shared workspaces use the same website accounts. Separate sessions have their own cookies and website storage. This choice is fixed when you create the workspace; saved passwords, passkeys, and history remain shared.")
                    color: Theme.muted
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                }
                RowLayout {
                    UiButton { text: qsTr("Cancel"); onClicked: panel.editing = false }
                    UiButton {
                        text: panel.editingId ? qsTr("Save") : qsTr("Create workspace")
                        primary: true
                        enabled: nameField.text.trim().length > 0
                        onClicked: {
                            if (panel.editingId) {
                                if (panel.manager.edit(panel.editingId, nameField.text, panel.chosenColor)) panel.editing = false
                            } else if (panel.manager.create(nameField.text, panel.chosenColor, isolated.checked)) panel.editing = false
                        }
                    }
                }
            }
        }
        ColumnLayout {
            visible: panel.deletingId.length > 0 && !panel.editing
            Layout.fillWidth: true
            Basic.Label {
                text: qsTr("Delete “%1” and all its tabs? Unsaved page changes will be lost. Website sign-in data will remain stored.").arg(panel.deletingName)
                textFormat: Text.PlainText
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                color: Theme.text
            }
            RowLayout {
                UiButton { text: qsTr("Cancel"); onClicked: panel.deletingId = "" }
                UiButton { text: qsTr("Delete workspace"); onClicked: if (panel.manager.remove(panel.deletingId)) panel.deletingId = "" }
            }
        }
        ListView {
            id: choices
            objectName: "workspaceChoices"
            visible: !panel.editing
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 8
            keyNavigationEnabled: true
            model: panel.manager.choices
            Basic.ScrollBar.vertical: Basic.ScrollBar { }
            function chooseSelected() {
                const selected = currentItem as WorkspaceChoice
                if (selected) { panel.manager.activate(selected.modelData.id); panel.close() }
            }
            Keys.onReturnPressed: chooseSelected()
            Keys.onEnterPressed: chooseSelected()
            delegate: WorkspaceChoice { }
            component WorkspaceChoice: ColumnLayout {
                id: choice
                required property var modelData
                required property int index
                width: choices.width - 12
                spacing: 4
                RowLayout {
                    Layout.fillWidth: true
                    Rectangle { implicitWidth: 12; implicitHeight: 12; radius: 6; color: choice.modelData.color }
                    UiButton {
                        Layout.fillWidth: true
                        text: choice.modelData.name
                        highlighted: choice.modelData.id === panel.manager.currentId || (choices.activeFocus && choice.index === choices.currentIndex)
                        onClicked: { panel.manager.activate(choice.modelData.id); panel.close() }
                    }
                    UiButton { text: qsTr("Edit"); flat: true; onClicked: panel.editWorkspace(choice.modelData) }
                    UiButton { iconName: "arrow-up"; flat: true; enabled: choice.index > 0; Accessible.name: qsTr("Move workspace up"); onClicked: panel.manager.reorder(choice.modelData.id, -1) }
                    UiButton { iconName: "arrow-down"; flat: true; enabled: choice.index < panel.manager.count-1; Accessible.name: qsTr("Move workspace down"); onClicked: panel.manager.reorder(choice.modelData.id, 1) }
                    UiButton { iconName: "x"; flat: true; enabled: panel.manager.count > 1; Accessible.name: qsTr("Delete %1").arg(choice.modelData.name); onClicked: { panel.deletingId = choice.modelData.id; panel.deletingName = choice.modelData.name } }
                }
                RowLayout {
                    Layout.fillWidth: true
                    Basic.Label {
                        Layout.fillWidth: true
                        text: qsTr("%1 tabs · %2").arg(choice.modelData.tabs).arg(choice.modelData.isolated ? qsTr("Separate session") : qsTr("Shared session"))
                        font.pixelSize: 12
                        color: Theme.muted
                    }
                    UiButton {
                        visible: choice.modelData.id !== panel.manager.currentId
                        text: panel.tabs.profileAt(panel.tabs.currentIndex) === choice.modelData.profile ? qsTr("Move tab here") : qsTr("Open copy here")
                        flat: true
                        enabled: panel.tabs.currentIndex >= 0
                        onClicked: {
                            if (panel.tabs.profileAt(panel.tabs.currentIndex) === choice.modelData.profile) panel.manager.moveTab(panel.tabs.currentIndex, choice.modelData.id)
                            else panel.manager.copyTab(panel.tabs.currentIndex, choice.modelData.id)
                            panel.close()
                        }
                    }
                }
            }
        }
    }
}
