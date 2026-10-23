pragma ComponentBehavior: Bound
import QtQuick
import Arbiter
import QtQuick.Controls.Basic as Basic

Basic.ToolTip {
    id: tip
    objectName: "roundedToolTip"
    popupType: Basic.Popup.Item
    delay: 650
    timeout: 4000
    padding: 9
    horizontalPadding: 12
    font.pixelSize: 12
    contentItem: Basic.Label {
        text: tip.text
        textFormat: Text.PlainText
        color: Theme.text
        font: tip.font
        wrapMode: Text.Wrap
    }
    background: Rectangle {
        radius: 10
        color: Theme.popupSolid
        border.color: Theme.border
    }
}
