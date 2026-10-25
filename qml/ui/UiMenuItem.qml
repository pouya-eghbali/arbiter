pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic as Basic
import Arbiter

Basic.MenuItem {
    id: control
    property string iconName: ""
    property string shortcutText: ""
    property bool reserveIconSpace: false
    implicitHeight: visible ? 36 : 0
    font.pixelSize: 13
    opacity: enabled ? 1 : 0.45
    horizontalPadding: 12
    indicator: Item { }
    contentItem: RowLayout {
        spacing: 10
        UiIcon {
            visible: control.reserveIconSpace || control.iconName.length > 0 || control.checkable
            name: control.checkable && control.checked ? "check" : control.iconName
            opacity: name.length ? 1 : 0
            Layout.preferredWidth: 18
            Layout.preferredHeight: 18
            color: Theme.text
        }
        Basic.Label {
            Layout.fillWidth: true
            text: control.text
            textFormat: Text.PlainText
            color: Theme.text
            font: control.font
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
        Basic.Label {
            visible: control.shortcutText.length > 0
            text: control.shortcutText
            textFormat: Text.PlainText
            color: Theme.muted
            font.pixelSize: 11
            horizontalAlignment: Text.AlignRight
        }
    }
    background: Rectangle { radius: 10; color: control.highlighted ? Theme.selection : control.hovered ? Theme.hover : "transparent" }
}
