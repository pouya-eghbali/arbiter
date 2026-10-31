pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import Arbiter

UiButton {
    id: control
    implicitHeight: Math.max(48, implicitContentHeight + 24)
    horizontalPadding: 16
    font.pixelSize: 15
    contentItem: Item {
        implicitWidth: labelRow.implicitWidth
        implicitHeight: labelRow.implicitHeight
        RowLayout {
            id: labelRow
            width: Math.min(implicitWidth, parent.width)
            x: control.textAlignment === Text.AlignLeft ? 0 : (parent.width - width) / 2
            anchors.verticalCenter: parent.verticalCenter
            spacing: 8
            UiIcon {
                visible: control.iconName.length > 0
                name: control.iconName
                color: control.iconColor
                Layout.preferredWidth: control.iconSize
                Layout.preferredHeight: control.iconSize
            }
            Text {
                Layout.fillWidth: true
                text: control.text
                textFormat: Text.PlainText
                font: control.font
                color: control.foregroundColor
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
            }
        }
    }
}
