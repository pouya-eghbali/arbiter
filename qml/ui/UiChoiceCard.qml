pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic as Basic
import Arbiter

Basic.Button {
    id: card
    property string subtitle: ""
    property string iconName: "user"
    property bool prominent: false
    implicitHeight: 82
    implicitWidth: 360
    padding: 18
    hoverEnabled: true
    Accessible.name: text + ". " + subtitle
    background: Rectangle {
        radius: 18
        color: card.prominent || card.down ? Theme.selection : card.hovered || card.activeFocus ? Theme.hover : "transparent"
        border.width: card.activeFocus ? 1 : 0
        border.color: Theme.accent
    }
    contentItem: RowLayout {
        spacing: 18
        UiIcon { name: card.iconName; color: card.prominent ? Theme.accent : Theme.text; Layout.preferredWidth: 28; Layout.preferredHeight: 28 }
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 5
            Text { text: card.text; textFormat: Text.PlainText; color: Theme.text; font.pixelSize: 18; Layout.fillWidth: true; elide: Text.ElideRight }
            Text { text: card.subtitle; textFormat: Text.PlainText; color: Theme.muted; font.pixelSize: 14; Layout.fillWidth: true; wrapMode: Text.Wrap }
        }
        UiIcon { name: "chevron-right"; color: card.prominent ? Theme.accent : Theme.muted; Layout.preferredWidth: 22; Layout.preferredHeight: 22 }
    }
}
