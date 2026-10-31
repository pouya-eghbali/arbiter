import QtQuick
import QtQuick.Controls.Basic as Controls
import Arbiter

UiComboBox {
    id: control
    implicitHeight: 48
    font.pixelSize: 15
    delegate: Controls.ItemDelegate {
        id: option
        required property int index
        required property var model
        width: control.width - 12
        implicitHeight: 48
        highlighted: control.highlightedIndex === index
        contentItem: Text {
            text: option.model.modelData
            font: control.font
            textFormat: Text.PlainText
            color: Theme.text
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
        background: Rectangle {
            radius: 9
            color: option.highlighted ? Theme.selection : option.down ? Theme.hover : "transparent"
        }
    }
}
