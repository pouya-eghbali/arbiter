import QtQuick
import Arbiter
import "LucideIcons.js" as Lucide

Item {
    id: icon
    property string name: "user"
    property color color: Theme.text
    // Keep existing semantic names stable across desktop and mobile controls.
    readonly property string assetName: ({user: "user-round", phone: "smartphone", key: "key-round", fingerprint: "fingerprint-pattern", "trash-2": "trash", "share-2": "share", history: "clock-arrow-up", "refresh-cw": "rotate-cw"})[name] || name
    readonly property int status: asset.status
    implicitWidth: 24
    implicitHeight: 24
    Accessible.ignored: true
    Image {
        id: asset
        anchors.fill: parent
        fillMode: Image.PreserveAspectFit
        // Qt 6.12 does not recognize data: SVGs as scalable for automatic
        // DPR handling. Request physical pixels explicitly; otherwise a 21pt
        // icon is enlarged from 21px to 63px on a Retina phone.
        sourceSize: Qt.size(Math.max(1, Math.ceil(width * icon.Screen.devicePixelRatio)),
                            Math.max(1, Math.ceil(height * icon.Screen.devicePixelRatio)))
        cache: true
        smooth: true
        // SVG source is bundled, never fetched. Tint before rasterization so icons
        // work with both GPU and software rendering without effects or icon fonts.
        source: Lucide.icons[icon.assetName]
            ? "data:image/svg+xml," + encodeURIComponent(Lucide.icons[icon.assetName].replace(/currentColor/g, icon.color.toString())) : ""
    }
}
