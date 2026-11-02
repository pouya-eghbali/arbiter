import QtQuick
import QtQuick.Layouts
import QtMultimedia
import Arbiter

Item {
    id: root
    objectName: "pairingScanner"
    property bool cameraEnabled: true
    property real topInset: 0
    property real bottomInset: 0
    signal invitation(string value)
    signal cancel()
    Rectangle { anchors.fill: parent; color: "#080b10" }
    QRScanner {
        id: decoder
        videoSink: preview.videoSink
        onScanned: value => root.invitation(value)
    }
    Component.onCompleted: Qt.callLater(function() { if (root.cameraEnabled) decoder.requestPermission() })
    Loader {
        active: root.cameraEnabled
        sourceComponent: Component {
            CaptureSession {
                camera: Camera { active: decoder.allowed && root.visible && Application.state === Qt.ApplicationActive }
                videoOutput: preview
            }
        }
    }
    VideoOutput { id: preview; anchors.fill: parent; fillMode: VideoOutput.PreserveAspectCrop }
    Item {
        id: guide
        width: Math.max(100, Math.min(root.width - 64, 300, root.height - root.topInset - root.bottomInset - 260))
        height: width
        anchors.horizontalCenter: parent.horizontalCenter
        y: Math.max(root.topInset + 100, (root.height - height) / 2 - 24)
        Rectangle { anchors.fill: parent; radius: 24; color: "transparent"; border.width: 2; border.color: "#ddffffff" }
    }
    Rectangle { x: 0; y: 0; width: root.width; height: guide.y; color: "#99000000" }
    Rectangle { x: 0; y: guide.y + guide.height; width: root.width; height: Math.max(0, root.height - y); color: "#99000000" }
    Rectangle { x: 0; y: guide.y; width: guide.x; height: guide.height; color: "#99000000" }
    Rectangle { x: guide.x + guide.width; y: guide.y; width: guide.x; height: guide.height; color: "#99000000" }
    RowLayout {
        anchors { left: parent.left; right: parent.right; top: parent.top; margins: 20; topMargin: root.topInset + 12 }
        Text { text: qsTr("Scan QR code"); color: "white"; font.pixelSize: 24; font.weight: Font.DemiBold; Layout.fillWidth: true }
        MobileIconButton { iconName: "x"; iconColor: "white"; label: qsTr("Close"); onClicked: root.cancel() }
    }
    ColumnLayout {
        anchors { top: guide.bottom; topMargin: 32; horizontalCenter: parent.horizontalCenter }
        width: Math.min(root.width - 48, 360); spacing: 16
        Text {
            text: (!root.cameraEnabled || decoder.allowed) ? qsTr("Position the desktop pairing code inside the frame.") : qsTr("Allow camera access to scan a pairing code.")
            textFormat: Text.PlainText; color: "white"; font.pixelSize: 16; wrapMode: Text.WordWrap; horizontalAlignment: Text.AlignHCenter; Layout.fillWidth: true
        }
        MobileButton { visible: root.cameraEnabled && !decoder.allowed; Layout.alignment: Qt.AlignHCenter; text: qsTr("Allow camera"); onClicked: decoder.requestPermission() }
        Text { text: qsTr("You can also scan with your phone’s Camera app to open Arbiter."); textFormat: Text.PlainText; color: "#bfffffff"; font.pixelSize: 13; wrapMode: Text.WordWrap; horizontalAlignment: Text.AlignHCenter; Layout.fillWidth: true }
    }
}
