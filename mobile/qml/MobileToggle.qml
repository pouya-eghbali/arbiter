import QtQuick
import Arbiter

UiCheckBox {
    width: parent ? parent.width : implicitWidth
    implicitHeight: Math.max(48, implicitContentHeight + 16)
    font.pixelSize: 15 * MobileMetrics.textScale
}
