import QtQuick
import Arbiter

Item {
    id: root
    implicitWidth: 32
    implicitHeight: 32
    Image {
        anchors.fill: parent
        source: "image://brand/" + encodeURIComponent(Theme.text.toString())
        sourceSize: Qt.size(root.width, root.height)
        fillMode: Image.PreserveAspectFit
        cache: true
    }
}
