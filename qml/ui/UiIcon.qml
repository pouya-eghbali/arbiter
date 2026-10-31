import QtQuick
import Arbiter
import "LucideIcons.js" as Lucide

Item {
    id: icon
    property string name: "user"
    property color color: Theme.text
    // Keep existing semantic names stable across desktop and mobile controls.
    readonly property string assetName: ({user: "user-round", phone: "smartphone", key: "key-round", fingerprint: "fingerprint-pattern", "trash-2": "trash", "share-2": "share", history: "clock-arrow-up"})[name] || name
    readonly property int status: asset.status
    implicitWidth: 24
    implicitHeight: 24
    Accessible.ignored: true
    Image {
        id: asset
        anchors.fill: parent
        fillMode: Image.PreserveAspectFit
        sourceSize: Qt.size(Math.max(1, width), Math.max(1, height))
        cache: true
        smooth: true
        // SVG source is bundled, never fetched. Tint before rasterization so icons
        // work with both GPU and software rendering without effects or icon fonts.
        source: Lucide.icons[icon.assetName]
            ? "data:image/svg+xml," + encodeURIComponent(Lucide.icons[icon.assetName].replace(/currentColor/g, icon.color.toString())) : ""
    }
}
