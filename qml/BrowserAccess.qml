pragma ComponentBehavior: Bound
import QtQuick
import "ui"

UiButton {
    id: access
    required property var importer
    signal scanned()
    objectName: "allowBrowserAccess"
    text: importer.requestingAccess ? qsTr("Waiting for access…")
        : importer.selectedBrowser.length > 0 ? qsTr("Allow %1 access").arg(importer.selectedBrowser) : qsTr("Allow browser access")
    enabled: !importer.busy
    onClicked: importer.requestBrowserAccess(access.Window.window)
    Connections {
        target: access.importer
        function onAccessFinished(allowed: bool) { if (allowed) access.scanned() }
    }
}
