pragma ComponentBehavior: Bound
import QtQuick
import Arbiter
import QtQuick.Controls.Basic as Basic

Basic.ComboBox {
    id: control
    implicitWidth: 180
    implicitHeight: 42
    padding: 12
    rightPadding: 36
    hoverEnabled: true
    background: Rectangle {
        radius: 12
        color: control.hovered ? Theme.hoverFor(parent) : Theme.fieldFor(parent)
        border.color: control.activeFocus ? Theme.accent : Theme.borderFor(parent)
    }
    contentItem: Basic.Label {
        text: control.displayText
        textFormat: Text.PlainText
        color: Theme.textFor(parent)
        font: control.font
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
    indicator: UiIcon {
        name: "chevron-down"
        color: Theme.mutedFor(parent)
        width: 20
        height: 20
        x: control.width - width - 14
        y: (control.height - height) / 2
    }
    delegate: Basic.ItemDelegate {
        id: option
        required property int index
        required property var model
        width: control.width - 12
        implicitHeight: 38
        padding: 10
        highlighted: control.highlightedIndex === index
        contentItem: Basic.Label {
            text: {
                const entry = option.model.modelData
                return control.textRole ? (entry && typeof entry === "object" ? entry[control.textRole] : option.model[control.textRole]) : entry
            }
            textFormat: Text.PlainText
            color: Theme.textFor(parent)
            elide: Text.ElideRight
            verticalAlignment: Text.AlignVCenter
        }
        background: Rectangle { radius: 9; color: option.highlighted ? Theme.selection : option.hovered ? Theme.hoverFor(parent) : "transparent" }
    }
    popup: UiPopup {
        popupType: Theme.nativePopups ? Basic.Popup.Window : Basic.Popup.Item
        y: control.height + 6
        width: control.width
        padding: 6
        implicitHeight: Math.min(260, options.contentHeight + 12)
        background: GlassSurface { radius: Theme.menuRadius; dense: true }
        contentItem: ListView {
            id: options
            clip: true
            implicitHeight: contentHeight
            model: control.popup.visible ? control.delegateModel : null
            currentIndex: control.highlightedIndex
            boundsBehavior: Flickable.StopAtBounds
            Basic.ScrollIndicator.vertical: Basic.ScrollIndicator { }
        }
    }
}
