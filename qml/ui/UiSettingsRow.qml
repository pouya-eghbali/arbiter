pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic as Basic

Item {
    id: root
    property string title
    property string description
    readonly property bool compact: width < 560
    default property alias controls: actions.data
    implicitHeight: layout.implicitHeight + 32
    GridLayout {
        id: layout
        anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter }
        columns: root.compact ? 1 : 2
        columnSpacing: 28; rowSpacing: 12
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 5
            Basic.Label { text: root.title; textFormat: Text.PlainText; color: Theme.textFor(parent); Layout.fillWidth: true; wrapMode: Text.Wrap; font.weight: Font.Medium }
            Basic.Label { visible: text.length > 0; text: root.description; textFormat: Text.PlainText; color: Theme.mutedFor(parent); font.pixelSize: 13; Layout.fillWidth: true; wrapMode: Text.Wrap }
        }
        RowLayout { id: actions; Layout.alignment: root.compact ? Qt.AlignLeft : Qt.AlignRight | Qt.AlignVCenter; spacing: 8 }
    }
    Rectangle { anchors { left: parent.left; right: parent.right; bottom: parent.bottom } height: 1; color: Theme.borderFor(parent); opacity: 0.55 }
}
