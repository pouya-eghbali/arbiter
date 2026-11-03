pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic as Basic
import Arbiter

Basic.MenuItem {
    id: strip
    objectName: "splitMenuActions"
    required property var ownerMenu
    property var shortcutHints: ({})
    property int selectedAction: 0
    readonly property var actions: [
        {icon: "columns-2", label: qsTr("Split left / right"), key: "splitHorizontal"},
        {icon: "rows-2", label: qsTr("Split top / bottom"), key: "splitVertical"},
        {icon: "panels-top-left", label: qsTr("Split with…"), key: "splitWith"}
    ]
    signal splitRequested(int arrangement)
    text: qsTr("Split view")
    Accessible.role: Accessible.ToolBar
    implicitHeight: 72
    horizontalPadding: 4
    verticalPadding: 4
    function activate(index: int) {
        if (!enabled || index < 0 || index >= actions.length) return
        ownerMenu.close()
        splitRequested(index)
    }
    Keys.onLeftPressed: selectedAction = (selectedAction + 2) % 3
    Keys.onRightPressed: selectedAction = (selectedAction + 1) % 3
    Keys.onReturnPressed: activate(selectedAction)
    Keys.onEnterPressed: activate(selectedAction)
    Keys.onSpacePressed: activate(selectedAction)
    background: Rectangle { radius: 12; color: Theme.fieldFor(parent) }
    contentItem: RowLayout {
        spacing: 4
        Repeater {
            model: strip.actions
            delegate: UiButton {
                id: button
                required property var modelData
                required property int index
                objectName: "menuSplit-" + modelData.key
                Layout.fillWidth: true
                Layout.preferredWidth: 0
                Layout.fillHeight: true
                flat: true
                padding: 5
                highlighted: strip.highlighted && strip.selectedAction === index
                Accessible.name: modelData.label
                onHoveredChanged: if (hovered) strip.selectedAction = index
                onClicked: strip.activate(index)
                contentItem: ColumnLayout {
                    spacing: 4
                    UiIcon { name: button.modelData.icon; color: Theme.textFor(parent); Layout.preferredWidth: 20; Layout.preferredHeight: 20; Layout.alignment: Qt.AlignHCenter }
                    Basic.Label {
                        text: button.modelData.label
                        textFormat: Text.PlainText
                        color: Theme.textFor(parent)
                        font.pixelSize: 11
                        Layout.fillWidth: true
                        Layout.preferredHeight: 28
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        wrapMode: Text.WordWrap
                        maximumLineCount: 2
                        elide: Text.ElideRight
                    }
                }
                UiToolTip {
                    visible: button.hovered
                    text: button.modelData.label + (strip.shortcutHints[button.modelData.key] ? " (" + strip.shortcutHints[button.modelData.key] + ")" : "")
                }
            }
        }
    }
}
