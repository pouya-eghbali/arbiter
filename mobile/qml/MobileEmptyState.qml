import QtQuick
import QtQuick.Layouts
import Arbiter

ColumnLayout {
    id: root
    property string iconName: "search"
    property string title: ""
    property string description: ""
    spacing: 12
    Item {
        Layout.preferredHeight: 28
    }
    UiIcon {
        name: root.iconName
        Layout.alignment: Qt.AlignHCenter
        Layout.preferredWidth: 36
        Layout.preferredHeight: 36
        color: Theme.mutedFor(parent)
    }
    Text {
        Layout.fillWidth: true
        text: root.title
        font.pixelSize: 19 * MobileMetrics.textScale
        font.weight: Font.Medium
        textFormat: Text.PlainText
        color: Theme.textFor(parent)
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.WordWrap
    }
    Text {
        visible: root.description.length > 0
        Layout.fillWidth: true
        text: root.description
        font.pixelSize: 14 * MobileMetrics.textScale
        lineHeight: 1.25
        textFormat: Text.PlainText
        color: Theme.mutedFor(parent)
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.WordWrap
    }
    Item {
        Layout.preferredHeight: 28
    }
}
