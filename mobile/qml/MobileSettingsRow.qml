pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic as Controls
import QtQuick.Layouts
import Arbiter

Controls.AbstractButton {
    id: control
    property string detail: ""
    property string iconName: ""
    implicitHeight: Math.max(60, contentItem.implicitHeight + 24)
    padding: 14
    Accessible.name: text
    Accessible.description: detail
    background: Rectangle { radius: 16; color: control.down ? Theme.hover : Theme.field }
    contentItem: RowLayout {
        spacing: 12
        UiIcon { name: control.iconName; color: Theme.accent; Layout.preferredWidth: 21; Layout.preferredHeight: 21 }
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 3
            Text { text: control.text; textFormat: Text.PlainText; Layout.fillWidth: true; color: Theme.text; font.pixelSize: 15; wrapMode: Text.WordWrap }
            Text { visible: control.detail.length > 0; text: control.detail; textFormat: Text.PlainText; Layout.fillWidth: true; color: Theme.muted; font.pixelSize: 12; wrapMode: Text.WordWrap }
        }
        UiIcon { name: "chevron-right"; color: Theme.muted; Layout.preferredWidth: 17; Layout.preferredHeight: 17 }
    }
}
