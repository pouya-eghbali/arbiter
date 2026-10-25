pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic as Basic
import Arbiter

UiPopup {
    id: panel
    required property var manager
    required property var tabs
    property var browserPreferences: null
    property bool privateMode: false
    property var tabSnapshot: []
    readonly property int visibleColumns: Math.max(1, Math.min(manager.count, 3, Math.floor((width - padding * 2) / 260)))
    readonly property real columnWidth: (width - padding * 2) / visibleColumns
    readonly property int previewRows: Math.min(3, Math.max(1, ...manager.choices.map(space => space.tabs)))
    readonly property bool hasOtherTabs: manager.choices.some(space => space.tabs > 3)
    signal tabRequested(int index)
    signal moreTabsRequested()
    function showWorkspaceTabs(id: string) {
        manager.activate(id)
        close()
        moreTabsRequested()
    }
    function refreshTabs() {
        if (visible) tabSnapshot = tabs.choices()
    }
    function tabsForWorkspace(id: string): var {
        const entries = tabSnapshot.filter(tab => tab.workspace === id)
        return entries.filter(tab => tab.pinned).concat(entries.filter(tab => !tab.pinned))
    }
    function requestTab(spaceId: string, tabId: int) {
        manager.activate(spaceId)
        // Resolve stable IDs after activation; rows may change as pages update.
        for (let row = 0; row < tabs.count; ++row) {
            if (tabs.idAt(row) === tabId) {
                tabRequested(row)
                close()
                return
            }
        }
    }
    function tabTitle(tab: var): string {
        return tab.url === "about:blank" ? qsTranslate("Main", "New tab") : (tab.title || tab.url)
    }
    function tabSite(address: string): string {
        if (!address || address === "about:blank") return qsTranslate("Main", "Start page")
        const authority = address.match(/^[a-zA-Z][a-zA-Z0-9+.-]*:\/\/([^/?#]+)/)
        return authority ? authority[1].replace(/^www\./, "") : address.replace(/[?#].*$/, "")
    }
    property string editingId: ""
    property string deletingId: ""
    property string deletingName: ""
    property bool editing: false
    property color chosenColor: Theme.accent
    property var actionSpace: null
    property int actionIndex: -1
    function focusCurrentWorkspace() {
        choices.currentIndex = Math.max(0, manager.choices.findIndex(space => space.id === manager.currentId))
        choices.positionViewAtIndex(choices.currentIndex, GridView.Contain)
        choices.forceActiveFocus()
    }
    function focusSearch() { if (editing) nameField.forceActiveFocus(); else focusCurrentWorkspace() }
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
            choices.positionViewAtIndex(choices.currentIndex, GridView.Contain)
        })
    }
    parent: Basic.Overlay.overlay
    anchors.centerIn: parent
    width: Math.max(0, Math.min(editing ? 580 : 52 + Math.min(manager.count, 3) * 300, parent.width - 32))
    height: Math.max(0, Math.min(parent.height - 48, editing ? 480
        : 196 + previewRows * 54 + (hasOtherTabs ? 40 : 0) + (deletingId.length ? 132 : 0) + (manager.error.length ? 48 : 0)))
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
        refreshTabs()
        focusCurrentWorkspace()
    }
    onClosed: workspaceActions.close()
    onEditingChanged: if (!editing && opened) Qt.callLater(panel.focusCurrentWorkspace)
    onDeletingIdChanged: if (!deletingId.length && opened && !editing) Qt.callLater(panel.focusCurrentWorkspace)
    Connections {
        target: panel.tabs
        function onSessionChanged() { panel.refreshTabs() }
    }
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
                    font.pixelSize: panel.editing ? 22 : 15
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
        GridView {
            id: choices
            objectName: "workspaceChoices"
            visible: !panel.editing
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            cellWidth: panel.columnWidth
            cellHeight: height
            flickableDirection: Flickable.VerticalFlick
            boundsBehavior: Flickable.StopAtBounds
            keyNavigationEnabled: true
            highlightMoveDuration: 0
            model: panel.manager.choices
            Basic.ScrollBar.vertical: Basic.ScrollBar { }
            function chooseSelected() {
                if (currentIndex >= 0 && currentIndex < panel.manager.choices.length)
                    panel.activateWorkspace(panel.manager.choices[currentIndex].id)
            }
            function moveSelection(direction: int) {
                currentIndex = Math.max(0, Math.min(count - 1, currentIndex + direction))
                positionViewAtIndex(currentIndex, GridView.Contain)
            }
            Keys.onLeftPressed: moveSelection(-1)
            Keys.onRightPressed: moveSelection(1)
            Keys.onDownPressed: moveSelection(panel.visibleColumns)
            Keys.onUpPressed: moveSelection(-panel.visibleColumns)
            Keys.onReturnPressed: chooseSelected()
            Keys.onEnterPressed: chooseSelected()
            Keys.onSpacePressed: chooseSelected()
            // The outer view may focus a column when its index changes. Keep
            // the focused header/tab inside that column active as it does so.
            delegate: FocusScope {
                id: choice
                required property var modelData
                required property int index
                readonly property bool current: modelData.id === panel.manager.currentId
                readonly property color workspaceColor: modelData.color
                readonly property string tabSummary: modelData.tabs === 1 ? qsTr("1 tab") : qsTr("%1 tabs").arg(modelData.tabs)
                width: panel.columnWidth
                height: choices.cellHeight - 8
                Rectangle {
                    visible: choice.index % panel.visibleColumns > 0
                    width: 1
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    anchors.topMargin: 8
                    anchors.bottomMargin: 8
                    color: Theme.field
                }
                ColumnLayout {
                    anchors.fill: parent
                    anchors.leftMargin: choice.index % panel.visibleColumns > 0 ? 14 : 0
                    anchors.rightMargin: 14
                    spacing: 8
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 3
                        Basic.ItemDelegate {
                            id: workspaceHeading
                            objectName: "workspaceHeading" + choice.index
                            Layout.fillWidth: true
                            implicitHeight: 34
                            padding: 6
                            hoverEnabled: true
                            text: choice.modelData.name
                            Accessible.name: qsTr("%1, %2, %3%4").arg(choice.modelData.name).arg(choice.tabSummary)
                                .arg(choice.modelData.isolated ? qsTr("Separate session") : qsTr("Shared session"))
                                .arg(choice.current ? qsTr(", current workspace") : "")
                            Accessible.role: Accessible.ListItem
                            Accessible.selected: choice.current
                            onClicked: panel.activateWorkspace(choice.modelData.id)
                            onActiveFocusChanged: if (activeFocus) choices.currentIndex = choice.index
                            background: Rectangle {
                                radius: 9
                                color: workspaceHeading.down ? Theme.pressed : workspaceHeading.hovered || choices.activeFocus && choices.currentIndex === choice.index ? Theme.hover : "transparent"
                                border.width: workspaceHeading.activeFocus ? 1 : 0
                                border.color: Theme.accent
                            }
                            contentItem: RowLayout {
                                spacing: 9
                                Rectangle { Layout.preferredWidth: 10; Layout.preferredHeight: 10; radius: 5; color: choice.workspaceColor }
                                Basic.Label { Layout.fillWidth: true; text: choice.modelData.name; textFormat: Text.PlainText; elide: Text.ElideRight; font.pixelSize: 14; font.weight: Font.Medium; color: Theme.text }
                                UiIcon { name: choice.current ? "check" : "chevron-right"; color: choice.current ? Theme.text : Theme.muted; Layout.preferredWidth: 15; Layout.preferredHeight: 15 }
                            }
                        }
                        UiIconButton {
                            id: actionsButton
                            objectName: "workspaceActionsButton" + choice.index
                            label: qsTr("Actions for %1").arg(choice.modelData.name)
                            iconName: "ellipsis"
                            onClicked: panel.showActions(choice.modelData, choice.index, actionsButton)
                        }
                    }
                    Basic.Label {
                        Layout.fillWidth: true
                        Layout.leftMargin: 6
                        text: choice.tabSummary + " · " + (choice.modelData.isolated ? qsTr("Separate session") : qsTr("Shared session"))
                        font.pixelSize: 11
                        color: Theme.muted
                        elide: Text.ElideRight
                    }
                    ListView {
                        id: workspaceTabs
                        objectName: "workspaceTabs" + choice.index
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.topMargin: 6
                        clip: true
                        spacing: 4
                        boundsBehavior: Flickable.StopAtBounds
                        keyNavigationEnabled: true
                        interactive: contentHeight > height
                        model: panel.tabsForWorkspace(choice.modelData.id).slice(0, 3)
                        currentIndex: -1
                        Basic.ScrollBar.vertical: Basic.ScrollBar { }
                        function moveSelection(direction: int) {
                            currentIndex = Math.max(0, Math.min(count - 1, currentIndex + direction))
                            positionViewAtIndex(currentIndex, ListView.Contain)
                            Qt.callLater(function() { if (workspaceTabs.currentItem) workspaceTabs.currentItem.forceActiveFocus() })
                        }
                        delegate: Basic.ItemDelegate {
                            id: tabChoice
                            required property var modelData
                            required property int index
                            readonly property bool current: panel.tabs.idAt(panel.tabs.currentIndex) === modelData.id
                            readonly property string title: panel.tabTitle(modelData)
                            readonly property string site: panel.tabSite(modelData.url)
                            objectName: "workspaceTab" + modelData.id
                            width: workspaceTabs.width - (workspaceTabs.contentHeight > workspaceTabs.height ? 10 : 0)
                            height: 50
                            padding: 7
                            hoverEnabled: true
                            text: title
                            Accessible.name: title + ", " + site
                            Accessible.selected: current
                            onClicked: panel.requestTab(choice.modelData.id, modelData.id)
                            Keys.onReturnPressed: panel.requestTab(choice.modelData.id, modelData.id)
                            Keys.onEnterPressed: panel.requestTab(choice.modelData.id, modelData.id)
                            Keys.onDownPressed: workspaceTabs.moveSelection(1)
                            Keys.onUpPressed: workspaceTabs.moveSelection(-1)
                            Keys.onLeftPressed: { choices.moveSelection(-1); choices.forceActiveFocus() }
                            Keys.onRightPressed: { choices.moveSelection(1); choices.forceActiveFocus() }
                            onActiveFocusChanged: if (activeFocus) {
                                workspaceTabs.currentIndex = index
                                choices.currentIndex = choice.index
                                choices.positionViewAtIndex(choice.index, GridView.Contain)
                                workspaceTabs.positionViewAtIndex(index, ListView.Contain)
                            }
                            background: Rectangle {
                                radius: 10
                                color: tabChoice.down ? Theme.pressed : tabChoice.hovered || tabChoice.activeFocus ? Theme.hover : tabChoice.current ? Theme.field : "transparent"
                                border.width: tabChoice.current || tabChoice.activeFocus ? 1 : 0
                                border.color: tabChoice.activeFocus ? Theme.accent : Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.6)
                            }
                            contentItem: RowLayout {
                                spacing: 10
                                Rectangle {
                                    Layout.preferredWidth: 34
                                    Layout.preferredHeight: 34
                                    radius: 7
                                    color: Theme.field
                                    Image {
                                        id: favicon
                                        anchors.centerIn: parent
                                        width: 26; height: 26
                                        source: panel.browserPreferences && !panel.privateMode && panel.visible
                                            ? panel.browserPreferences.favicon(tabChoice.modelData.url, false) : ""
                                        fillMode: Image.PreserveAspectFit
                                        visible: status === Image.Ready
                                        asynchronous: true
                                    }
                                    Basic.Label {
                                        anchors.centerIn: parent
                                        visible: !favicon.visible
                                        text: (Array.from(tabChoice.title.trim())[0] || "A").toUpperCase()
                                        color: Theme.text
                                        font.pixelSize: 19
                                        font.weight: Font.Medium
                                    }
                                }
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 3
                                    Basic.Label { Layout.fillWidth: true; text: tabChoice.title; textFormat: Text.PlainText; color: Theme.text; font.pixelSize: 12; elide: Text.ElideRight }
                                    Basic.Label { Layout.fillWidth: true; text: tabChoice.site; textFormat: Text.PlainText; color: Theme.muted; font.pixelSize: 10; elide: Text.ElideMiddle }
                                }
                                UiIcon { visible: tabChoice.modelData.pinned; name: "pin"; color: Theme.muted; Layout.preferredWidth: 12; Layout.preferredHeight: 12 }
                            }
                        }
                    }
                    UiButton {
                        objectName: "workspaceMoreTabs" + choice.index
                        visible: choice.modelData.tabs > 3
                        Layout.fillWidth: true
                        implicitHeight: 32
                        padding: 7
                        horizontalPadding: 7
                        font.pixelSize: 11
                        flat: true
                        textAlignment: Text.AlignLeft
                        text: choice.modelData.tabs === 4 ? qsTr("+ 1 other tab") : qsTr("+ %1 other tabs").arg(choice.modelData.tabs - 3)
                        onClicked: panel.showWorkspaceTabs(choice.modelData.id)
                        onActiveFocusChanged: if (activeFocus) {
                            choices.currentIndex = choice.index
                            choices.positionViewAtIndex(choice.index, GridView.Contain)
                        }
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
        RowLayout {
            visible: !panel.editing
            Layout.fillWidth: true
            spacing: 8
            Rectangle { width: 6; height: 6; radius: 3; color: panel.manager.currentColor }
            Basic.Label { text: panel.manager.currentName; textFormat: Text.PlainText; elide: Text.ElideRight; Layout.fillWidth: true; color: Theme.muted; font.pixelSize: 11 }
            Basic.Label { text: "← → " + qsTr("↑ ↓ Navigate    ↵ Switch    Esc Close"); color: Theme.muted; font.pixelSize: 10; elide: Text.ElideRight; Layout.maximumWidth: panel.availableWidth * 0.72 }
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
