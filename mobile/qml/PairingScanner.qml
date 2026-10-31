import QtQuick
import QtQuick.Layouts
import QtMultimedia
import Arbiter

ColumnLayout {
    id: root
    signal invitation(string value)
    spacing: 16
    Text {
        textFormat: Text.PlainText
        text: qsTr("Scan the pairing QR code from desktop Settings → Sync.")
        color: Theme.muted
        wrapMode: Text.WordWrap
        Layout.fillWidth: true
    }
    QRScanner {
        id: decoder
        videoSink: preview.videoSink
        onScanned: value => root.invitation(value)
        Component.onCompleted: requestPermission()
    }
    Camera {
        id: camera
        active: decoder.allowed
    }
    CaptureSession {
        camera: camera
        videoOutput: preview
    }
    VideoOutput {
        id: preview
        Layout.fillWidth: true
        Layout.preferredHeight: Math.min(300, root.width)
        fillMode: VideoOutput.PreserveAspectCrop
    }
    Text {
        textFormat: Text.PlainText
        visible: !decoder.allowed
        text: qsTr("Allow camera access to scan a pairing code.")
        color: Theme.muted
        wrapMode: Text.WordWrap
        Layout.fillWidth: true
    }
    MobileButton {
        Layout.fillWidth: true
        visible: !decoder.allowed
        text: qsTr("Allow camera")
        onClicked: decoder.requestPermission()
    }
}
