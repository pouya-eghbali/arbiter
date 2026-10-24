pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic as Basic
import Arbiter

Basic.Button {
    id: control
    property string iconName: ""
    property int iconSize: 20
    readonly property color primaryFill: enabled ? Theme.accent : Theme.mix(Theme.popupSolid, Theme.accent, 0.45)
    readonly property color foregroundColor: primary ? Theme.contrastingText(primaryFill) : Theme.text
    property color iconColor: foregroundColor
    property bool primary: false
    property int textAlignment: Text.AlignHCenter
    implicitWidth: Math.max(40, implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(40, implicitContentHeight + topPadding + bottomPadding)
    padding: 10
    horizontalPadding: 16
    hoverEnabled: true
    // Muting an entire primary button also fades its text and can reverse the
    // fill's contrast. Mute just the fill and retain a readable foreground.
    opacity: enabled || primary ? 1 : 0.45
    palette.buttonText: foregroundColor
    palette.highlightedText: foregroundColor
    contentItem: RowLayout {
        spacing: 8
        UiIcon {
            visible: control.iconName.length > 0
            name: control.iconName
            color: control.iconColor
            Layout.preferredWidth: control.iconSize
            Layout.preferredHeight: control.iconSize
            Layout.alignment: Qt.AlignCenter
        }
        Text {
            visible: control.text.length > 0
            Layout.fillWidth: true
            text: control.text
            textFormat: Text.PlainText
            color: control.foregroundColor
            font: control.font
            horizontalAlignment: control.textAlignment
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
    }
    background: Rectangle {
        radius: 12
        color: control.primary ? control.primaryFill
            : control.down ? Theme.pressed : control.checked || control.highlighted ? Theme.selection
            : control.hovered || control.activeFocus ? Theme.hover : control.flat ? "transparent" : Theme.field
        border.width: control.activeFocus || control.primary && (control.hovered || control.down) ? 1 : 0
        border.color: control.primary ? control.foregroundColor : Theme.accent
        Behavior on color { enabled: !control.primary; ColorAnimation { duration: 100 } }
    }
}
