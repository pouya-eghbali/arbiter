pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic as Basic
import QtWebEngine
import Arbiter

Basic.Popup {
    id: panel
    required property var controller
    required property var browserProfile
    property string extensionId: ""
    property url extensionUrl: ""
    property string pageTitle: ""
    signal manageRequested()
    signal newWindowRequested(var request)
    signal visitRequested(string url)
    function showExtension(id: string, options: bool) {
        const url = options ? controller.optionsUrl(id) : controller.popupUrl(id)
        if (!url.toString().length) return
        extensionId = id
        extensionUrl = url
        pageTitle = qsTr("Extension")
        open()
    }
    function showList() { extensionId = ""; extensionUrl = ""; pageTitle = qsTr("Extensions"); open() }
    parent: Basic.Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(extensionId.length ? 680 : 460, parent.width - 32)
    height: Math.min(extensionId.length ? 600 : 440, parent.height - 48)
    padding: 16
    modal: true
    dim: true
    focus: true
    closePolicy: Basic.Popup.CloseOnEscape | Basic.Popup.CloseOnPressOutside
    background: GlassSurface { dense: true; radius: 24 }
    Basic.Overlay.modal: Rectangle { color: Theme.scrim }
    onClosed: { extensionId = ""; extensionUrl = "" }
    onControllerChanged: close()
    Connections {
        target: panel.controller
        function onChanged() { if (panel.extensionId.length && !panel.controller.popupUrl(panel.extensionId).toString().length && !panel.controller.optionsUrl(panel.extensionId).toString().length) panel.close() }
    }
    contentItem: ColumnLayout {
        spacing: 12
        RowLayout {
            Layout.fillWidth: true
            Basic.Label { Layout.fillWidth: true; text: panel.pageTitle; textFormat: Text.PlainText; color: Theme.text; font.pixelSize: 20; elide: Text.ElideRight }
            UiButton { text: qsTr("Close"); onClicked: panel.close() }
        }
        Basic.ScrollView {
            visible: !panel.extensionId.length
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentWidth: availableWidth
            ColumnLayout {
                width: parent.width
                spacing: 6
                Basic.Label {
                    Layout.fillWidth: true
                    visible: !panel.controller || !panel.controller.items.length
                    text: qsTr("Install extensions in Settings to get started.")
                    color: Theme.muted
                    wrapMode: Text.Wrap
                }
                Repeater {
                    model: panel.controller ? panel.controller.items : []
                    delegate: UiButton {
                        required property var modelData
                        Layout.fillWidth: true
                        text: modelData.name + (modelData.enabled ? "" : qsTr(" · Disabled"))
                        textAlignment: Text.AlignLeft
                        enabled: modelData.enabled && (modelData.hasPopup || modelData.hasOptions)
                        onClicked: panel.showExtension(modelData.id, !modelData.hasPopup)
                    }
                }
                Basic.Label { Layout.fillWidth: true; text: qsTr("Background extensions run without a popup. Toolbar actions without a popup are not supported yet."); color: Theme.muted; wrapMode: Text.Wrap }
            }
        }
        Loader {
            id: extensionLoader
            Layout.fillWidth: true
            Layout.fillHeight: true
            active: panel.opened && panel.extensionId.length > 0
            visible: active
            sourceComponent: Component {
                WebEngineView {
                    id: extensionView
                    objectName: "extensionPopupView"
                    profile: panel.browserProfile
                    url: panel.extensionUrl
                    backgroundColor: Theme.window
                    onTitleChanged: panel.pageTitle = title || qsTr("Extension")
                    onWindowCloseRequested: panel.close()
                    onNewWindowRequested: function(request) { panel.newWindowRequested(request) }
                    onPermissionRequested: function(permission) { permission.deny() }
                    onDesktopMediaRequested: function(request) { request.cancel() }
                    onWebAuthUxRequested: function(request) { request.cancel() }
                    onNavigationRequested: function(request) {
                        const destination = request.url.toString()
                        const prefix = "chrome-extension://" + panel.extensionId + "/"
                        if (request.isMainFrame && !destination.startsWith(prefix) && destination !== "about:blank") {
                            request.reject()
                            if (/^https?:\/\//i.test(destination)) panel.visitRequested(destination)
                        }
                    }
                    Basic.BusyIndicator { anchors.centerIn: parent; visible: extensionView.loading; running: visible }
                }
            }
        }
        UiButton { visible: !panel.extensionId.length; Layout.fillWidth: true; text: qsTr("Manage extensions…"); onClicked: { panel.close(); panel.manageRequested() } }
    }
}
