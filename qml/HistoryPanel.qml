pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic as Basic
import Arbiter

UiPopup {
    id: panel
    property bool privateMode: false
    required property var manager
    property var browserPreferences: null
    property string deleteScope: ""
    property var deleteIds: []
    property bool dateValid: true
    signal visitRequested(string url, bool newTab)
    function focusSearch() { search.forceActiveFocus() }
    function resetFilters() {
        search.clear(); websiteFilter.clear()
        dateRange.currentIndex = 0
        fromField.clear(); throughField.clear()
        manager.resetFilters()
        dateValid = true
    }
    function applyDatePreset() {
        deleteScope = ""
        if (dateRange.currentIndex === 5) { applyCustomDates(); return }
        const today = new Date()
        const from = new Date(today.getFullYear(), today.getMonth(), today.getDate())
        const through = new Date(from)
        if (dateRange.currentIndex === 2) { from.setDate(from.getDate() - 1); through.setDate(through.getDate() - 1) }
        else if (dateRange.currentIndex === 3) from.setDate(from.getDate() - 6)
        else if (dateRange.currentIndex === 4) from.setDate(from.getDate() - 29)
        dateValid = manager.setDateRange(dateRange.currentIndex === 0 ? "" : Qt.formatDate(from, "yyyy-MM-dd"),
            dateRange.currentIndex === 0 ? "" : Qt.formatDate(through, "yyyy-MM-dd"))
    }
    function applyCustomDates() {
        deleteScope = ""
        dateValid = manager.setDateRange(fromField.text.trim(), throughField.text.trim())
    }
    function confirmDelete(scope: string) {
        deleteIds = scope === "matching" ? manager.matchingIds : []
        deleteScope = scope
    }
    parent: Basic.Overlay.overlay
    anchors.centerIn: parent
    width: Math.max(0, Math.min(920, parent.width - 32))
    height: Math.max(0, Math.min(760, parent.height - 48))
    padding: 24
    modal: true
    dim: true
    focus: true
    closePolicy: Basic.Popup.CloseOnEscape | Basic.Popup.CloseOnPressOutside
    background: GlassSurface { dense: true; radius: Theme.panelRadius }
    Basic.Overlay.modal: Rectangle { color: Theme.scrim }
    onOpened: { deleteScope = ""; resetFilters(); search.forceActiveFocus() }
    onDeleteScopeChanged: { if (deleteScope.length) deleteConfirmation.open(); else deleteConfirmation.close() }
    onClosed: { deleteScope = ""; deleteIds = []; deleteMenu.close() }
    Connections { target: panel.manager; function onFiltersChanged() { panel.deleteScope = ""; visits.currentIndex = -1; visits.positionViewAtBeginning() } }
    contentItem: ColumnLayout {
        spacing: 12
        RowLayout {
            Layout.fillWidth: true
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 3
                Basic.Label { Layout.fillWidth: true; text: qsTr("History"); font.pixelSize: 24; font.weight: Font.DemiBold; color: Theme.text }
                Basic.Label { Layout.fillWidth: true; elide: Text.ElideRight; text: qsTr("Your browsing timeline, newest first."); color: Theme.muted; font.pixelSize: 12 }
            }
            UiButton {
                id: deleteButton
                objectName: "historyDeleteMenuButton"
                text: qsTr("Delete…"); iconName: "trash"; flat: true
                enabled: panel.manager.totalCount > 0
                onClicked: deleteMenu.popup(deleteButton, deleteButton.width - deleteMenu.width, deleteButton.height + 4)
            }
            UiIconButton { label: qsTr("Close history"); iconName: "x"; onClicked: panel.close() }
        }
        UiTextField {
            id: search
            objectName: "historySearch"
            Layout.fillWidth: true
            placeholderText: qsTr("Search titles and addresses")
            Accessible.name: placeholderText
            onTextChanged: panel.manager.search = text
            onAccepted: if (visits.count > 0 && panel.dateValid) { visits.currentIndex = 0; visits.forceActiveFocus() }
            Keys.onDownPressed: if (visits.count > 0 && panel.dateValid) { visits.currentIndex = 0; visits.forceActiveFocus() }
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: 10
            UiComboBox {
                id: dateRange
                objectName: "historyDateRange"
                Layout.preferredWidth: 170
                Accessible.name: qsTr("Filter by date")
                model: [qsTr("All dates"), qsTr("Today"), qsTr("Yesterday"), qsTr("Last 7 days"), qsTr("Last 30 days"), qsTr("Custom dates…")]
                onActivated: panel.applyDatePreset()
            }
            UiTextField {
                id: websiteFilter
                objectName: "historyWebsiteFilter"
                Layout.fillWidth: true
                placeholderText: qsTr("Website, e.g. example.com")
                Accessible.name: qsTr("Filter by website, including subdomains")
                onTextChanged: panel.manager.website = text
            }
            UiIconButton {
                label: qsTr("Reset all filters"); iconName: "rotate-cw"
                enabled: panel.manager.hasFilters || dateRange.currentIndex !== 0 || !panel.dateValid
                onClicked: panel.resetFilters()
            }
        }
        RowLayout {
            visible: dateRange.currentIndex === 5
            Layout.fillWidth: true
            spacing: 10
            Basic.Label { text: qsTr("From"); color: Theme.muted }
            UiDateField { id: fromField; pickerTitle: qsTr("First date"); objectName: "historyFromDate"; Layout.fillWidth: true; placeholderText: "YYYY-MM-DD"; maximumLength: 10; Accessible.name: qsTr("First date, YYYY-MM-DD; blank for no lower limit"); onTextChanged: if (dateRange.currentIndex === 5) panel.applyCustomDates() }
            Basic.Label { text: qsTr("Through"); color: Theme.muted }
            UiDateField { id: throughField; pickerTitle: qsTr("Last date"); objectName: "historyThroughDate"; Layout.fillWidth: true; placeholderText: "YYYY-MM-DD"; maximumLength: 10; Accessible.name: qsTr("Last date, YYYY-MM-DD; blank for no upper limit"); onTextChanged: if (dateRange.currentIndex === 5) panel.applyCustomDates() }
        }
        Basic.Label {
            visible: !panel.dateValid
            text: qsTr("Use YYYY-MM-DD, with the first date before or equal to the last. Either date can be left blank.")
            color: Theme.muted; font.pixelSize: 12; Layout.fillWidth: true; wrapMode: Text.Wrap
        }
        Basic.Label {
            visible: panel.privateMode || panel.manager.error.length > 0
            text: panel.manager.error || qsTr("Private browsing history is never saved.")
            textFormat: Text.PlainText
            color: Theme.muted; Layout.fillWidth: true; wrapMode: Text.Wrap
        }
        ListView {
            id: visits
            objectName: "historyList"
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: panel.dateValid
            clip: true
            spacing: 0
            model: panel.manager
            currentIndex: -1
            keyNavigationEnabled: true
            boundsBehavior: Flickable.StopAtBounds
            Basic.ScrollBar.vertical: Basic.ScrollBar { }
            section.property: "day"
            section.criteria: ViewSection.FullString
            section.delegate: RowLayout {
                id: dayHeading
                required property string section
                width: visits.width - 14
                height: 46
                spacing: 12
                Basic.Label { text: dayHeading.section; textFormat: Text.PlainText; color: Theme.text; font.pixelSize: 13; font.weight: Font.DemiBold }
                Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Theme.border }
            }
            delegate: HistoryEntry { }
            component HistoryEntry: RowLayout {
                id: entry
                required property int index
                required property string visitId
                required property string pageTitle
                required property string pageUrl
                required property string website
                required property date visitedAt
                width: visits.width - 14
                height: 62
                spacing: 8
                Basic.Label {
                    Layout.preferredWidth: 44
                    text: Qt.formatTime(entry.visitedAt, "hh:mm")
                    color: Theme.muted; font.pixelSize: 11
                    horizontalAlignment: Text.AlignRight
                    elide: Text.ElideRight
                }
                Item {
                    Layout.preferredWidth: 12; Layout.fillHeight: true
                    Rectangle { anchors.horizontalCenter: parent.horizontalCenter; width: 1; height: parent.height; color: Theme.border }
                    Rectangle { anchors.centerIn: parent; width: 5; height: 5; radius: 2.5; color: Theme.muted }
                }
                UiButton {
                    Layout.fillWidth: true; Layout.fillHeight: true
                    flat: true; padding: 8; horizontalPadding: 8
                    highlighted: entry.ListView.isCurrentItem && visits.activeFocus
                    Accessible.name: qsTr("Open %1, %2, %3").arg(entry.pageTitle).arg(entry.pageUrl).arg(Qt.formatTime(entry.visitedAt))
                    contentItem: RowLayout {
                        spacing: 10
                        Rectangle {
                            Layout.preferredWidth: 30; Layout.preferredHeight: 30
                            radius: 8; color: Theme.field
                            Image {
                                id: favicon
                                anchors.centerIn: parent; width: 18; height: 18
                                sourceSize: Qt.size(32, 32)
                                // Never contact old websites just to display history.
                                source: panel.browserPreferences && !panel.privateMode ? panel.browserPreferences.favicon(entry.pageUrl, false) : ""
                                asynchronous: true; cache: true
                            }
                            Basic.Label { anchors.centerIn: parent; visible: favicon.status !== Image.Ready; text: entry.website.charAt(0).toUpperCase(); color: Theme.muted; font.pixelSize: 13; Accessible.ignored: true }
                        }
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 4
                            Basic.Label { text: entry.pageTitle; textFormat: Text.PlainText; color: Theme.text; elide: Text.ElideRight; Layout.fillWidth: true; font.pixelSize: 13; font.weight: Font.Medium }
                            Basic.Label { text: entry.pageUrl; textFormat: Text.PlainText; color: Theme.muted; elide: Text.ElideMiddle; Layout.fillWidth: true; font.pixelSize: 11 }
                        }
                    }
                    onClicked: panel.visitRequested(entry.pageUrl, false)
                }
                UiIconButton { label: qsTr("Show visits to %1").arg(entry.website); iconName: "search"; onClicked: websiteFilter.text = entry.website }
                UiIconButton { label: qsTr("Open %1 in a new tab").arg(entry.pageTitle); iconName: "plus"; onClicked: panel.visitRequested(entry.pageUrl, true) }
                UiIconButton { label: qsTr("Remove %1 from history").arg(entry.pageTitle); iconName: "x"; onClicked: panel.manager.remove(entry.visitId) }
            }
            function openSelected() {
                const selected = currentItem as HistoryEntry
                if (selected) panel.visitRequested(selected.pageUrl, false)
            }
            Keys.onReturnPressed: openSelected()
            Keys.onEnterPressed: openSelected()
            Keys.onDeletePressed: { const selected = currentItem as HistoryEntry; if (selected) panel.manager.remove(selected.visitId) }
            Basic.Label {
                anchors.centerIn: parent
                width: parent.width - 32
                horizontalAlignment: Text.AlignHCenter; wrapMode: Text.Wrap
                visible: visits.count === 0
                text: panel.manager.totalCount === 0 ? qsTr("Your browsing timeline starts here.") : qsTr("No visits match these filters. Try another date or website.")
                color: Theme.muted
            }
        }
        Item { visible: !panel.dateValid; Layout.fillWidth: true; Layout.fillHeight: true }
        RowLayout {
            Layout.fillWidth: true
            Basic.Label {
                text: panel.dateValid ? qsTr("%1 of %2 visits · Up to 10,000 saved locally").arg(panel.manager.count).arg(panel.manager.totalCount) : qsTr("Enter a valid date range to see matching visits.")
                color: Theme.muted; font.pixelSize: 11; Layout.fillWidth: true; wrapMode: Text.Wrap
            }
            UiCheckBox {
                visible: !panel.privateMode
                text: qsTr("Save history")
                checked: panel.manager.enabled
                onToggled: panel.manager.enabled = checked
            }
        }
    }
    UiPopup {
        id: deleteConfirmation
        objectName: "historyDeleteConfirmation"
        parent: Basic.Overlay.overlay
        anchors.centerIn: parent
        width: Math.max(0, Math.min(480, parent.width - 32))
        padding: 24
        modal: true
        dim: true
        focus: true
        closePolicy: Basic.Popup.CloseOnEscape
        background: GlassSurface { dense: true; radius: Theme.panelRadius }
        Basic.Overlay.modal: Rectangle { color: Theme.scrim }
        onClosed: { panel.deleteScope = ""; panel.deleteIds = []; if (panel.opened) search.forceActiveFocus() }
        contentItem: ColumnLayout {
            spacing: 14
            Basic.Label {
                text: panel.deleteScope === "matching" ? qsTr("Delete %1 matching visits?").arg(panel.deleteIds.length) : qsTr("Delete all browsing history?")
                color: Theme.text; font.weight: Font.DemiBold; Layout.fillWidth: true; wrapMode: Text.Wrap
            }
            Basic.Label {
                text: (panel.deleteScope === "matching" ? qsTr("Only the results selected by your filters will be removed. Other visits are kept.")
                    : qsTr("Every saved visit will be removed, including visits hidden by your filters."))
                    + " " + qsTr("This cannot be undone. Open tabs and their Back/Forward lists stay intact.")
                color: Theme.muted; font.pixelSize: 12; Layout.fillWidth: true; wrapMode: Text.Wrap
            }
            Basic.Label { visible: panel.manager.error.length > 0; text: panel.manager.error; textFormat: Text.PlainText; color: Theme.text; Layout.fillWidth: true; wrapMode: Text.Wrap }
            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                UiButton { text: qsTr("Cancel"); flat: true; onClicked: panel.deleteScope = "" }
                UiButton {
                    objectName: "historyConfirmDelete"
                    text: panel.deleteScope === "matching" ? qsTr("Delete matching visits") : qsTr("Delete all history")
                    iconName: "trash"
                    onClicked: {
                        const success = panel.deleteScope === "matching" ? panel.manager.removeVisits(panel.deleteIds) : panel.manager.clear()
                        if (success) { panel.deleteScope = ""; panel.deleteIds = [] }
                    }
                }
            }
        }
    }
    UiMenu {
        id: deleteMenu
        objectName: "historyDeleteMenu"
        parent: panel.contentItem
        preferredWidth: 260
        UiMenuItem { text: qsTr("Delete matching results…"); enabled: panel.manager.hasFilters && panel.manager.count > 0 && panel.dateValid; onTriggered: panel.confirmDelete("matching") }
        UiMenuItem { text: qsTr("Delete all history…"); enabled: panel.manager.totalCount > 0; onTriggered: panel.confirmDelete("all") }
    }
}
