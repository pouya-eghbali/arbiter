import QtQuick
import QtQuick.Layouts
import QtQuick.Templates as Templates
import Arbiter

ColumnLayout {
    id: root
    property string title: ""
    property string description: ""
    default property alias controls: body.data
    // A single control supplies its own surface. Only groups need a card.
    readonly property bool grouped: {
        const items = body.children.filter(item => item.visible);
        return items.length !== 1 || !(items[0] instanceof Templates.AbstractButton || items[0] instanceof Templates.ComboBox);
    }
    spacing: 12
    Text {
        visible: root.title.length > 0
        Layout.fillWidth: true
        text: root.title
        textFormat: Text.PlainText
        font.pixelSize: 14
        font.weight: Font.DemiBold
        color: Theme.muted
        wrapMode: Text.WordWrap
    }
    Rectangle {
        Layout.fillWidth: true
        implicitHeight: body.implicitHeight + (root.grouped ? 32 : 0)
        radius: 18
        color: root.grouped ? Theme.field : "transparent"
        ColumnLayout {
            id: body
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: root.grouped ? 16 : 0
            spacing: 12
        }
    }
    Text {
        visible: root.description.length > 0
        Layout.fillWidth: true
        text: root.description
        textFormat: Text.PlainText
        font.pixelSize: 13
        lineHeight: 1.2
        color: Theme.muted
        wrapMode: Text.WordWrap
    }
}
