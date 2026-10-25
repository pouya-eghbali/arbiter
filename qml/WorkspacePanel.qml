pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic as Basic
import Arbiter

UiPopup {
    id: panel
    required property var manager
    required property var tabs
    property string editingId: ""
    property string deletingId: ""
    property string deletingName: ""
    property bool editing: false
    property color chosenColor: Theme.accent
    property var actionSpace: null
    property int actionIndex: -1
    function focusSearch() { if (editing) nameField.forceActiveFocus(); else choices.forceActiveFocus() }
    function editWorkspace(space: var) {
        editingId = space ? space.id : ""
        nameField.text = space ? space.name : ""
        chosenColor = space ? space.color : Theme.accent
        isolated.checked = space ? space.isolated : false
        deletingId = ""
        editing = true
        Qt.callLater(function() { nameField.forceActiveFocus(); nameField.selectAll() })
    }
    function activateWorkspace(id: string) {
        manager.activate(id)
        if (manager.currentId === id) close()
    }
    function showActions(space: var, index: int, anchor: Item) {
        // Keep the menu outside delegates: reordering recreates the list rows.
        actionSpace = space
        actionIndex = index
        const at = anchor.mapToItem(panel.contentItem, anchor.width, anchor.height + 4)
        workspaceActions.popup(at.x - workspaceActions.width, at.y)
    }
    function reorderWorkspace(direction: int) {
        const id = actionSpace.id
        manager.reorder(id, direction)
        Qt.callLater(function() {
            choices.currentIndex = panel.manager.choices.findIndex(space => space.id === id)
            choices.positionViewAtIndex(choices.currentIndex, ListView.Contain)
        })
    }
    parent: Basic.Overlay.overlay
    anchors.centerIn: parent
    width: Math.max(0, Math.min(580, parent.width - 32))
    height: Math.max(0, Math.min(parent.height - 48, editing ? 480
        : 148 + Math.min(manager.count, 5) * 78 + (deletingId.length ? 132 : 0) + (manager.error.length ? 48 : 0)))
    padding: 20
    modal: true
    dim: true
    focus: true
    closePolicy: Basic.Popup.CloseOnEscape | Basic.Popup.CloseOnPressOutside
    background: GlassSurface { dense: true; radius: Theme.panelRadius }
    Basic.Overlay.modal: Rectangle { color: Theme.scrim }
    onOpened: {
        editing = false
        deletingId = ""
        choices.currentIndex = Math.max(0, manager.choices.findIndex(space => space.id === manager.currentId))
        choices.forceActiveFocus()
    }
    onClosed: workspaceActions.close()
    onEditingChanged: if (!editing && opened) Qt.callLater(function() { choices.forceActiveFocus() })
    onDeletingIdChanged: if (!deletingId.length && opened && !editing) Qt.callLater(function() { choices.forceActiveFocus() })
    contentItem: ColumnLayout {
        spacing: 16
        RowLayout {
            Layout.fillWidth: true
            spacing: 10
            UiIconButton { visible: panel.editing; label: qsTr("Back to workspaces"); iconName: "arrow-left"; onClicked: panel.editing = false }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 3
                Basic.Label {
                    text: panel.editing ? (panel.editingId ? qsTr("Edit workspace") : qsTr("New workspace")) : qsTr("Workspaces")
                    color: Theme.text
                    font.pixelSize: 22
                    font.weight: Font.DemiBold
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                }
                Basic.Label {
                    text: panel.editing ? qsTr("Make a space your own.") : qsTr("Switch between your spaces.")
                    color: Theme.muted
                    font.pixelSize: 12
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                }
            }
            UiButton { visible: !panel.editing; text: qsTr("New"); iconName: "plus"; flat: true; onClicked: panel.editWorkspace(null) }
            UiIconButton { label: qsTr("Close workspaces"); iconName: "x"; onClicked: panel.close() }
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
            id: editor
            visible: panel.editing
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: availableWidth
            clip: true
            ColumnLayout {
                width: editor.availableWidth
                spacing: 12
                Basic.Label { text: qsTr("Name"); color: Theme.text; font.weight: Font.Medium }
                UiTextField { id: nameField; objectName: "workspaceName"; Layout.fillWidth: true; maximumLength: 48; placeholderText: qsTr("e.g. Work, Personal, Research"); Accessible.name: qsTr("Workspace name") }
                UiColorPicker {
                    Layout.fillWidth: true
                    title: qsTr("Color")
                    value: panel.chosenColor
                    defaultColor: Theme.accent
                    onColorSelected: color => panel.chosenColor = color
                }
                UiCheckBox { id: isolated; text: qsTr("Separate website sign-in session"); enabled: panel.editingId.length === 0 }
                Basic.Label {
                    text: panel.editingId
                        ? qsTr("Session type is fixed when a workspace is created. Saved passwords, passkeys, and history are shared across workspaces.")
                        : qsTr("Separate sessions have their own cookies and website accounts. Leave this off to share sign-ins with your other shared workspaces. Saved passwords, passkeys, and history stay shared.")
                    color: Theme.muted
                    font.pixelSize: 12
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                }
            }
        }
        Rectangle {
            visible: panel.deletingId.length > 0 && !panel.editing
            Layout.fillWidth: true
            implicitHeight: confirmation.implicitHeight + 24
            radius: 14
            color: Theme.field
            ColumnLayout {
                id: confirmation
                anchors.fill: parent
                anchors.margins: 12
                spacing: 8
                Basic.Label {
                    text: qsTr("Delete “%1”?").arg(panel.deletingName)
                    textFormat: Text.PlainText
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                    font.weight: Font.DemiBold
                    color: Theme.text
                }
                Basic.Label {
                    text: qsTr("All its tabs will close and unsaved changes will be lost. Website sign-in data stays on this device.")
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                    font.pixelSize: 12
                    color: Theme.muted
                }
                RowLayout {
                    Layout.fillWidth: true
                    Item { Layout.fillWidth: true }
                    UiButton { text: qsTr("Keep workspace"); flat: true; onClicked: panel.deletingId = "" }
                    UiButton { text: qsTr("Delete"); onClicked: if (panel.manager.remove(panel.deletingId)) panel.deletingId = "" }
                }
            }
        }
        ListView {
            id: choices
            objectName: "workspaceChoices"
            visible: !panel.editing
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 6
            keyNavigationEnabled: true
            model: panel.manager.choices
            Basic.ScrollBar.vertical: Basic.ScrollBar { }
            function chooseSelected() {
                if (currentIndex >= 0 && currentIndex < panel.manager.choices.length)
                    panel.activateWorkspace(panel.manager.choices[currentIndex].id)
            }
            Keys.onReturnPressed: chooseSelected()
            Keys.onEnterPressed: chooseSelected()
            Keys.onSpacePressed: chooseSelected()
            delegate: Basic.ItemDelegate {
                id: choice
                required property var modelData
                required property int index
                readonly property bool current: modelData.id === panel.manager.currentId
                readonly property color workspaceColor: modelData.color
                readonly property string tabSummary: modelData.tabs === 1 ? qsTr("1 tab") : qsTr("%1 tabs").arg(modelData.tabs)
                width: choices.width - (choices.contentHeight > choices.height ? 12 : 0)
                height: 72
                padding: 12
                hoverEnabled: true
                highlighted: choices.activeFocus && index === choices.currentIndex
                text: modelData.name
                Accessible.name: qsTr("%1, %2, %3%4").arg(modelData.name).arg(tabSummary)
                    .arg(modelData.isolated ? qsTr("Separate session") : qsTr("Shared session"))
                    .arg(current ? qsTr(", current workspace") : "")
                Accessible.role: Accessible.ListItem
                Accessible.selected: current
                onHoveredChanged: if (hovered) choices.currentIndex = index
                onClicked: panel.activateWorkspace(modelData.id)
                background: Rectangle {
                    radius: 14
                    color: choice.down ? Theme.pressed : choice.hovered ? Theme.hover : choice.current
                        ? Theme.mix(Theme.popupSolid, choice.workspaceColor, Theme.darkSurface ? 0.14 : 0.09) : "transparent"
                    border.width: choice.highlighted ? 1 : 0
                    border.color: Theme.accent
                    Behavior on color { ColorAnimation { duration: Theme.reducedMotion ? 0 : 100 } }
                }
                contentItem: RowLayout {
                    spacing: 12
                    Rectangle {
                        Layout.preferredWidth: 42
                        Layout.preferredHeight: 42
                        radius: 13
                        color: Qt.rgba(choice.workspaceColor.r, choice.workspaceColor.g, choice.workspaceColor.b, 0.18)
                        Basic.Label {
                            anchors.centerIn: parent
                            text: (Array.from(choice.modelData.name.trim())[0] || "W").toUpperCase()
                            font.pixelSize: 18
                            font.weight: Font.DemiBold
                            color: Theme.text
                            Accessible.ignored: true
                        }
                        Rectangle { anchors.right: parent.right; anchors.bottom: parent.bottom; anchors.margins: 3; width: 8; height: 8; radius: 4; color: choice.workspaceColor }
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 5
                        Basic.Label { Layout.fillWidth: true; text: choice.modelData.name; textFormat: Text.PlainText; font.pixelSize: 15; font.weight: Font.Medium; elide: Text.ElideRight; color: Theme.text }
                        Basic.Label { Layout.fillWidth: true; text: choice.tabSummary + " · " + (choice.modelData.isolated ? qsTr("Separate session") : qsTr("Shared session")); font.pixelSize: 12; elide: Text.ElideRight; color: Theme.muted }
                    }
                    UiIcon { visible: choice.current; name: "check"; color: Theme.text; Layout.preferredWidth: 16; Layout.preferredHeight: 16 }
                    UiIconButton {
                        id: actionsButton
                        objectName: "workspaceActionsButton" + choice.index
                        label: qsTr("Actions for %1").arg(choice.modelData.name)
                        iconName: "ellipsis"
                        onClicked: panel.showActions(choice.modelData, choice.index, actionsButton)
                    }
                }
            }
        }
        RowLayout {
            visible: panel.editing
            Layout.fillWidth: true
            UiButton { text: qsTr("Cancel"); flat: true; onClicked: panel.editing = false }
            Item { Layout.fillWidth: true }
            UiButton {
                objectName: "saveWorkspaceButton"
                text: panel.editingId ? qsTr("Save changes") : qsTr("Create workspace")
                primary: true
                enabled: nameField.text.trim().length > 0
                onClicked: {
                    if (panel.editingId) {
                        if (panel.manager.edit(panel.editingId, nameField.text, panel.chosenColor)) panel.editing = false
                    } else if (panel.manager.create(nameField.text, panel.chosenColor, isolated.checked)) panel.editing = false
                }
            }
        }
        Basic.Label {
            visible: !panel.editing
            Layout.fillWidth: true
            text: qsTr("↑ ↓ Navigate    ↵ Switch    Esc Close")
            font.pixelSize: 11
            color: Theme.muted
        }
    }
    UiMenu {
        id: workspaceActions
        objectName: "workspaceActionsMenu"
        parent: panel.contentItem
        preferredWidth: 260
        onClosed: if (panel.opened && !panel.editing) choices.forceActiveFocus()
        UiMenuItem { text: qsTr("Edit workspace…"); onTriggered: panel.editWorkspace(panel.actionSpace) }
        UiMenuItem { text: qsTr("Move up"); enabled: panel.actionIndex > 0; onTriggered: panel.reorderWorkspace(-1) }
        UiMenuItem { text: qsTr("Move down"); enabled: panel.actionIndex < panel.manager.count - 1; onTriggered: panel.reorderWorkspace(1) }
        Basic.MenuSeparator { }
        UiMenuItem {
            visible: panel.actionSpace && panel.actionSpace.id !== panel.manager.currentId
            height: visible ? implicitHeight : 0
            text: panel.actionSpace && panel.tabs.profileAt(panel.tabs.currentIndex) === panel.actionSpace.profile ? qsTr("Move current tab here") : qsTr("Open copy of current tab here")
            enabled: panel.tabs.currentIndex >= 0
            onTriggered: {
                const sameSession = panel.tabs.profileAt(panel.tabs.currentIndex) === panel.actionSpace.profile
                const success = sameSession ? panel.manager.moveTab(panel.tabs.currentIndex, panel.actionSpace.id)
                    : panel.manager.copyTab(panel.tabs.currentIndex, panel.actionSpace.id)
                if (success) panel.close()
            }
        }
        UiMenuItem {
            text: qsTr("Delete workspace…")
            enabled: panel.manager.count > 1
            onTriggered: { panel.deletingId = panel.actionSpace.id; panel.deletingName = panel.actionSpace.name }
        }
    }
}
