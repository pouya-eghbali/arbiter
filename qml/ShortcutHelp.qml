pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic as Basic
import Arbiter

UiPopup {
    id: sheet
    objectName: "shortcutHelpSheet"
    required property var keyBindings
    signal allShortcutsRequested()
    parent: Basic.Overlay.overlay
    anchors.centerIn: parent
    width: Math.max(0, Math.min(880, parent.width - 32))
    height: Math.max(0, Math.min(540, parent.height - 48))
    padding: 22
    modal: true
    dim: true
    focus: true
    closePolicy: Basic.Popup.CloseOnEscape | Basic.Popup.CloseOnPressOutside
    background: GlassSurface { dense: true; radius: Theme.panelRadius }
    Basic.Overlay.modal: Rectangle { color: Theme.scrim }
    onOpened: {
        const flick = shortcutsScroll.contentItem as Flickable
        if (flick) flick.contentY = 0
        closeButton.forceActiveFocus()
    }
    contentItem: ColumnLayout {
        spacing: 16
        RowLayout {
            Layout.fillWidth: true
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 4
                Basic.Label { text: qsTr("Keyboard shortcuts"); font.pixelSize: 24; font.weight: Font.DemiBold; color: Theme.text; Layout.fillWidth: true; wrapMode: Text.Wrap }
                Basic.Label { text: qsTr("Your current bindings. Customize them in Settings."); color: Theme.muted; Layout.fillWidth: true; wrapMode: Text.Wrap }
            }
            UiIconButton { id: closeButton; label: qsTr("Close"); iconName: "x"; onClicked: sheet.close() }
        }
        Basic.ScrollView {
            id: shortcutsScroll
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 0
            clip: true
            contentWidth: availableWidth
            GridLayout {
                width: shortcutsScroll.availableWidth
                columns: width >= 640 ? 2 : 1
                columnSpacing: 28
                rowSpacing: 8
                Repeater {
                    model: sheet.keyBindings.commonShortcuts
                    delegate: RowLayout {
                        id: shortcutRow
                        required property var modelData
                        objectName: "shortcutHelp_" + modelData.actionId
                        Layout.fillWidth: true
                        Layout.preferredWidth: 1
                        Layout.minimumHeight: 34
                        spacing: 12
                        Basic.Label {
                            text: shortcutRow.modelData.actionLabel
                            textFormat: Text.PlainText
                            color: Theme.text
                            Layout.fillWidth: true
                            wrapMode: Text.Wrap
                        }
                        Rectangle {
                            implicitWidth: keyLabel.implicitWidth + 16
                            implicitHeight: 28
                            radius: 7
                            color: Theme.field
                            Basic.Label {
                                id: keyLabel
                                objectName: "shortcutHelpKey_" + shortcutRow.modelData.actionId
                                anchors.centerIn: parent
                                text: shortcutRow.modelData.keyLabel
                                textFormat: Text.PlainText
                                font.pixelSize: 12
                                font.weight: Font.Medium
                                color: Theme.text
                            }
                        }
                    }
                }
            }
        }
        RowLayout {
            Layout.fillWidth: true
            Item { Layout.fillWidth: true }
            UiButton {
                objectName: "allShortcutsButton"
                text: qsTr("See all shortcuts")
                iconName: "arrow-right"
                onClicked: { sheet.close(); sheet.allShortcutsRequested() }
            }
        }
    }
}
