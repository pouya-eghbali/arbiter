pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic as Basic
import Arbiter

Basic.Popup {
    id: panel
    required property var manager
    property bool confirmingClear: false
    signal visitRequested(string url, bool newTab)
    function focusSearch() { search.forceActiveFocus() }
    parent: Basic.Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(880, parent.width - 32)
    height: Math.min(720, parent.height - 48)
    padding: 24
    modal: true
    dim: true
    focus: true
    closePolicy: Basic.Popup.CloseOnEscape | Basic.Popup.CloseOnPressOutside
    background: GlassSurface { dense: true; radius: 24 }
    Basic.Overlay.modal: Rectangle { color: Theme.scrim }
    onOpened: { confirmingClear = false; search.text = ""; search.forceActiveFocus() }
    onClosed: confirmingClear = false
    contentItem: ColumnLayout {
        spacing: 12
        RowLayout {
            Layout.fillWidth: true
            Basic.Label { text: qsTr("History"); font.pixelSize: 26; font.weight: Font.DemiBold; color: Theme.text; Layout.fillWidth: true }
            UiButton { text: qsTr("Clear all…"); enabled: panel.manager.totalCount > 0; onClicked: panel.confirmingClear = true }
            UiButton { text: qsTr("Done"); onClicked: panel.close() }
        }
        UiTextField {
            id: search
            objectName: "historySearch"
            Layout.fillWidth: true
            placeholderText: qsTr("Search titles and addresses")
            Accessible.name: placeholderText
            onTextChanged: panel.manager.search = text
            onAccepted: if (visits.count > 0) { visits.currentIndex = 0; visits.forceActiveFocus() }
            Keys.onDownPressed: if (visits.count > 0) { visits.currentIndex = 0; visits.forceActiveFocus() }
        }
        UiCheckBox {
            text: qsTr("Save browsing history")
            checked: panel.manager.enabled
            onToggled: panel.manager.enabled = checked
        }
        Basic.Label {
            visible: panel.manager.error.length > 0
            text: panel.manager.error
            textFormat: Text.PlainText
            color: Theme.text
            Layout.fillWidth: true
            wrapMode: Text.Wrap
        }
        Rectangle {
            visible: panel.confirmingClear
            Layout.fillWidth: true
            implicitHeight: confirmation.implicitHeight + 24
            radius: 12
            color: Theme.field
            ColumnLayout {
                id: confirmation
                anchors.fill: parent
                anchors.margins: 12
                Basic.Label {
                    text: qsTr("Delete all saved history? This cannot be undone. Open tabs and their Back/Forward lists will remain.")
                    color: Theme.text
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                }
                RowLayout {
                    UiButton { text: qsTr("Cancel"); onClicked: panel.confirmingClear = false }
                    UiButton { text: qsTr("Delete all history"); onClicked: if (panel.manager.clear()) panel.confirmingClear = false }
                }
            }
        }
        ListView {
            id: visits
            objectName: "historyList"
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 4
            model: panel.manager
            currentIndex: -1
            keyNavigationEnabled: true
            Basic.ScrollBar.vertical: Basic.ScrollBar { }
            section.property: "day"
            section.criteria: ViewSection.FullString
            section.delegate: Basic.Label {
                required property string section
                width: visits.width
                height: 36
                text: section
                textFormat: Text.PlainText
                color: Theme.muted
                font.weight: Font.Medium
                verticalAlignment: Text.AlignVCenter
            }
            delegate: HistoryEntry { }
            component HistoryEntry: RowLayout {
                id: entry
                required property int index
                required property string visitId
                required property string pageTitle
                required property string pageUrl
                required property date visitedAt
                width: visits.width - 14
                height: 76
                spacing: 6
                UiButton {
                    id: openButton
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    flat: true
                    highlighted: entry.ListView.isCurrentItem && visits.activeFocus
                    Accessible.name: qsTr("Open %1, %2").arg(entry.pageTitle).arg(entry.pageUrl)
                    contentItem: ColumnLayout {
                        spacing: 3
                        Basic.Label { text: entry.pageTitle; textFormat: Text.PlainText; color: Theme.text; elide: Text.ElideRight; Layout.fillWidth: true; font.weight: Font.Medium }
                        Basic.Label { text: entry.pageUrl; textFormat: Text.PlainText; color: Theme.muted; elide: Text.ElideMiddle; Layout.fillWidth: true; font.pixelSize: 12 }
                        Basic.Label { text: Qt.formatTime(entry.visitedAt, "hh:mm"); color: Theme.muted; font.pixelSize: 11 }
                    }
                    onClicked: panel.visitRequested(entry.pageUrl, false)
                }
                UiButton {
                    id: newTabButton
                    text: "+"
                    flat: true
                    Accessible.name: qsTr("Open %1 in a new tab").arg(entry.pageTitle)
                    UiToolTip { visible: newTabButton.hovered; text: qsTr("Open in new tab") }
                    onClicked: panel.visitRequested(entry.pageUrl, true)
                }
                UiButton {
                    id: removeButton
                    text: "×"
                    flat: true
                    Accessible.name: qsTr("Remove %1 from history").arg(entry.pageTitle)
                    UiToolTip { visible: removeButton.hovered; text: qsTr("Remove from history") }
                    onClicked: panel.manager.remove(entry.visitId)
                }
            }
            function openSelected() {
                const selected = currentItem as HistoryEntry
                if (selected) panel.visitRequested(selected.pageUrl, false)
            }
            Keys.onReturnPressed: openSelected()
            Keys.onEnterPressed: openSelected()
            Basic.Label {
                anchors.centerIn: parent
                width: parent.width - 32
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                visible: visits.count === 0
                text: panel.manager.totalCount === 0 ? qsTr("Your browsing history will appear here.") : qsTr("No matching history.")
                color: Theme.muted
            }
        }
        Basic.Label {
            text: qsTr("%1 of %2 visits · Up to 10,000 saved on this computer").arg(panel.manager.count).arg(panel.manager.totalCount)
            color: Theme.muted
            font.pixelSize: 12
            Layout.fillWidth: true
            wrapMode: Text.Wrap
        }
    }
}
