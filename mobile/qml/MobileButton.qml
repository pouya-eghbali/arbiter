pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import Arbiter

UiButton {
    id: control
    implicitHeight: Math.max(48, implicitContentHeight + 24)
    horizontalPadding: 16
    font.pixelSize: 15 * MobileMetrics.textScale
    opacity: nativeMaterial ? (enabled ? 1 : .45) : (enabled || primary ? 1 : .45)
    textColor: nativeMaterial ? Theme.textFor(control) : foregroundColor
    iconColor: textColor
    background: Rectangle {
        radius: control.nativeMaterial ? height / 2 : 12
        color: control.nativeMaterial ? Qt.alpha(control.textColor,
            control.down ? .20 : control.primary || control.checked || control.highlighted ? .14 : control.hovered || control.activeFocus ? .10 : control.flat ? 0 : .07)
            : control.primary ? control.primaryFill : control.down ? Theme.pressedFor(control)
            : control.checked || control.highlighted ? Theme.selection : control.hovered || control.activeFocus ? Theme.hoverFor(control) : control.flat ? "transparent" : Theme.fieldFor(control)
        border.width: control.activeFocus ? 1 : 0
        border.color: control.nativeMaterial ? Qt.alpha(control.textColor, .24) : Theme.accent
    }
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
                color: control.textColor
                verticalAlignment: Text.AlignVCenter
                wrapMode: Text.WordWrap
            }
        }
    }
}
