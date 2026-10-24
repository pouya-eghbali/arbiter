pragma ComponentBehavior: Bound
import QtQuick

Rectangle {
    id: background
    property url imageSource
    readonly property bool ready: artwork.status === Image.Ready
    color: Theme.window
    Image {
        id: artwork
        objectName: "backgroundArtwork"
        anchors.fill: parent
        source: background.imageSource
        asynchronous: true
        visible: status === Image.Ready
        fillMode: Image.PreserveAspectCrop
        // Decode in coarse size steps so dragging a resize handle does not
        // continuously re-decode a large photo.
        sourceSize: Qt.size(Math.min(5120, Math.max(512, Math.ceil(width * Screen.devicePixelRatio / 512) * 512)),
                            Math.min(5120, Math.max(512, Math.ceil(height * Screen.devicePixelRatio / 512) * 512)))
    }
}
