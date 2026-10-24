pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic as Basic

UiPopup {
    popupType: Basic.Popup.Item
    id: picker
    objectName: "splitWithPicker"
    required property var tabs
    required property var workspaces
    required property var splits
    property string owner: ""
    property int pane: 0
    property int contextTab: 0
    property var entries: []
    parent: Basic.Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(560, parent.width - 32)
    height: Math.min(parent.height - 32, 180 + Math.min(400, Math.max(60, list.contentHeight)))
    padding: 20
    modal: true
    dim: true
    focus: true
    Basic.Overlay.modal: Rectangle { color: Theme.scrim }
    background: GlassSurface { dense: true }
    closePolicy: Basic.Popup.CloseOnEscape | Basic.Popup.CloseOnPressOutside
    signal dismissed()
    function openFor() {
        owner = workspaces.currentId
        pane = splits.activePane
        contextTab = tabs.idAt(tabs.currentIndex)
        placement.currentIndex = 0
        search.text = ""
        rebuild()
        open()
    }
    function rebuild() {
        const previous = entries[list.currentIndex]?.id
        const query = search.text.trim().toLowerCase()
        const all = tabs.choices()
        const rows = []
        for (const workspace of workspaces.choices) {
            let heading = true
            for (const tab of all) {
                if (tab.workspace !== workspace.id || splits.geometry(tab.id).paneId !== undefined) continue
                const title = tab.url === "about:blank" ? qsTr("New tab") : (tab.title || tab.url)
                if (query && !(title + " " + tab.url + " " + workspace.name).toLowerCase().includes(query)) continue
                rows.push({id: tab.id, title: title, url: tab.url === "about:blank" ? qsTr("Start page") : tab.url,
                           workspace: workspace.name, color: workspace.color, heading: heading})
                heading = false
            }
        }
        entries = rows
        list.currentIndex = Math.max(0, rows.findIndex(row => row.id === previous))
    }
    function choose(index: int) {
        if (index < 0 || index >= entries.length || owner !== workspaces.currentId || pane !== splits.activePane
                || contextTab !== tabs.idAt(tabs.currentIndex)) return
        const tab = entries[index].id
        const direction = placement.currentIndex
        close()
        splits.splitWith(tab, direction === 0 || direction === 2, direction >= 2)
    }
    function moveSelection(delta: int) {
        list.currentIndex = Math.max(0, Math.min(entries.length - 1, list.currentIndex + delta))
        list.positionViewAtIndex(list.currentIndex, ListView.Contain)
    }
    onOpened: search.forceActiveFocus()
    onClosed: dismissed()
    Connections {
        target: picker.workspaces
        function onCurrentChanged() { picker.close() }
        function onChanged() { if (picker.visible) picker.rebuild() }
    }
    Connections {
        target: picker.tabs
        function onDataChanged() { if (picker.visible) picker.rebuild() }
        function onCountChanged() { if (picker.visible) picker.rebuild() }
        function onCurrentIndexChanged() { if (picker.visible && picker.tabs.idAt(picker.tabs.currentIndex) !== picker.contextTab) picker.close() }
    }
    Connections {
        target: picker.splits
        function onChanged() {
            if (!picker.visible) return
            if (picker.pane !== picker.splits.activePane) picker.close()
            else picker.rebuild()
        }
    }
    contentItem: ColumnLayout {
        spacing: 12
        RowLayout {
            Basic.Label { text: qsTr("Split with…"); color: Theme.text; font.pixelSize: 24; Layout.fillWidth: true }
            UiComboBox {
                id: placement
                objectName: "splitWithPlacement"
                model: [qsTr("Right"), qsTr("Below"), qsTr("Left"), qsTr("Above")]
                implicitWidth: 130
                Accessible.name: qsTr("Place the selected tab")
            }
        }
        UiTextField {
            id: search
            objectName: "splitWithSearch"
            Layout.fillWidth: true
            placeholderText: qsTr("Search tabs and workspaces")
            Accessible.name: placeholderText
            onTextChanged: { list.currentIndex = 0; picker.rebuild() }
            Keys.onDownPressed: picker.moveSelection(1)
            Keys.onUpPressed: picker.moveSelection(-1)
            onAccepted: picker.choose(list.currentIndex)
        }
        ListView {
            id: list
            objectName: "splitWithTabs"
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 4
            model: picker.entries
            Basic.ScrollBar.vertical: Basic.ScrollBar {}
            delegate: Item {
                id: entry
                required property var modelData
                required property int index
                width: list.width
                height: 54 + (modelData.heading ? 26 : 0)
                Basic.Label {
                    visible: entry.modelData.heading
                    text: entry.modelData.workspace
                    textFormat: Text.PlainText
                    color: Theme.muted
                    font.pixelSize: 12
                    leftPadding: 10
                    height: 24
                    width: parent.width
                    elide: Text.ElideRight
                }
                Basic.ItemDelegate {
                    width: parent.width
                    height: 54
                    y: entry.modelData.heading ? 26 : 0
                    text: entry.modelData.title
                    highlighted: list.currentIndex === entry.index
                    hoverEnabled: true
                    onHoveredChanged: if (hovered) list.currentIndex = entry.index
                    onClicked: picker.choose(entry.index)
                    contentItem: RowLayout {
                        spacing: 10
                        Rectangle { width: 8; height: 8; radius: 4; color: entry.modelData.color }
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 3
                            Basic.Label { text: entry.modelData.title; textFormat: Text.PlainText; color: Theme.text; Layout.fillWidth: true; elide: Text.ElideRight }
                            Basic.Label { text: entry.modelData.url; textFormat: Text.PlainText; color: Theme.muted; font.pixelSize: 12; Layout.fillWidth: true; elide: Text.ElideMiddle }
                        }
                    }
                    background: Rectangle { radius: 10; color: list.currentIndex === entry.index ? Theme.selection : "transparent" }
                }
            }
            Basic.Label {
                parent: list
                anchors.centerIn: parent
                visible: picker.entries.length === 0
                text: qsTr("No other matching tabs")
                color: Theme.muted
            }
        }
        Basic.Label { text: qsTr("↑ ↓ Choose a tab    ↵ Split    Esc Cancel"); font.pixelSize: 12; color: Theme.muted }
    }
}
