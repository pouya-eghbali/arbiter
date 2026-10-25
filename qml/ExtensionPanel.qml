pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Effects
import QtQuick.Layouts
import QtQuick.Controls.Basic as Basic
import QtWebEngine
import Arbiter

UiPopup {
    popupType: Basic.Popup.Item
    id: panel
    required property var controller
    required property var browserProfile
    property string extensionId: ""
    property url extensionUrl: ""
    property bool optionsPage: false
    property size popupContentSize: Qt.size(360, 420)
    readonly property int popupFrameHeight: padding * 2 + 28 + 6
    function fitContent(view) {
        if (!opened || optionsPage || !extensionId.length || view.loading) return
        const expectedUrl = extensionUrl.toString()
        view.mainFrame.runJavaScript(`(() => {
            const body = document.body;
            if (!body) return null;
            const bounds = body.getBoundingClientRect();
            const style = getComputedStyle(body);
            const number = value => parseFloat(value) || 0;
            return [Math.ceil(Math.max(bounds.width, body.scrollWidth) + number(style.marginLeft) + number(style.marginRight)),
                    Math.ceil(Math.max(bounds.height, body.scrollHeight) + number(style.marginTop) + number(style.marginBottom))];
        })()`, 0, function(size) {
            if (!panel.opened || panel.optionsPage || panel.extensionUrl.toString() !== expectedUrl || !size || size.length !== 2) return
            if (Number.isFinite(size[0]) && Number.isFinite(size[1]) && size[0] > 0 && size[1] > 0)
                panel.popupContentSize = Qt.size(Math.max(160, Math.min(800, size[0])), Math.max(80, Math.min(800, size[1])))
        })
    }
    function place() {
        x = extensionId.length ? Math.max(16, parent.width - width - 20) : (parent.width - width) / 2
        y = Math.max(16, (parent.height - height) / 2)
    }
    function keepInBounds() {
        x = Math.max(0, Math.min(x, parent.width - width))
        y = Math.max(0, Math.min(y, parent.height - height))
    }
    signal invoked(string id)
    signal manageRequested()
    signal newWindowRequested(var request)
    signal visitRequested(string url)
    function showExtension(id: string, options: bool) {
        const url = options ? controller.optionsUrl(id) : controller.popupUrl(id)
        if (!url.toString().length) return
        if (!options) invoked(id)
        popupContentSize = Qt.size(360, 420)
        extensionId = id
        extensionUrl = url
        optionsPage = options
        place()
        open()
    }
    function showList() { extensionId = ""; extensionUrl = ""; optionsPage = false; place(); open() }
    parent: Basic.Overlay.overlay
    width: Math.max(1, Math.min(extensionId.length ? (optionsPage ? 680 : popupContentSize.width + padding * 2) : 460, parent.width - 32))
    height: Math.max(1, Math.min(extensionId.length ? (optionsPage ? 600 : popupContentSize.height + popupFrameHeight) : 440, parent.height - 48))
    padding: extensionId.length ? 8 : 16
    modal: false
    dim: false
    focus: true
    // Keep the extension document alive while the user interacts with its page.
    closePolicy: Basic.Popup.CloseOnEscape
    background: GlassSurface { dense: true; radius: Theme.panelRadius }
    onWidthChanged: if (visible) keepInBounds()
    onHeightChanged: if (visible) keepInBounds()
    Connections {
        target: panel.parent
        function onWidthChanged() { panel.keepInBounds() }
        function onHeightChanged() { panel.keepInBounds() }
    }
    onClosed: { extensionId = ""; extensionUrl = "" }
    onControllerChanged: close()
    Connections {
        target: panel.controller
        function onChanged() { if (panel.extensionId.length && !panel.controller.popupUrl(panel.extensionId).toString().length && !panel.controller.optionsUrl(panel.extensionId).toString().length) panel.close() }
    }
    contentItem: ColumnLayout {
        spacing: panel.extensionId.length ? 6 : 12
        RowLayout {
            visible: !panel.extensionId.length
            Layout.fillWidth: true
            Basic.Label { Layout.fillWidth: true; text: qsTr("Extensions"); textFormat: Text.PlainText; color: Theme.text; font.pixelSize: 20; elide: Text.ElideRight }
            UiIconButton { label: qsTr("Close"); iconName: "x"; onClicked: panel.close() }
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
                    backgroundColor: "white"
                    onLoadingChanged: function(request) {
                        if (request.status === WebEngineView.LoadSucceededStatus) panel.fitContent(extensionView)
                    }
                    // DOM changes can shrink below the current viewport without
                    // changing contentsSize, so remeasure only while a popup is open.
                    Timer {
                        interval: 500
                        repeat: true
                        running: panel.opened && !panel.optionsPage && !extensionView.loading
                        onTriggered: panel.fitContent(extensionView)
                    }
                    layer.enabled: true
                    layer.effect: MultiEffect { maskEnabled: true; maskSource: popupMask }
                    Rectangle {
                        id: popupMask
                        anchors.fill: parent
                        radius: Math.max(8, Theme.panelRadius - panel.padding)
                        color: "white"
                        layer.enabled: true
                        visible: false
                    }
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
        RowLayout {
            visible: panel.extensionId.length > 0
            Layout.fillWidth: true
            spacing: 4
            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: 28
                Accessible.role: Accessible.Grip
                Accessible.name: qsTr("Extension")
                Rectangle {
                    anchors.centerIn: parent
                    width: 36; height: 4; radius: 2
                    color: dragHandler.active ? Theme.text : Theme.muted
                    opacity: 0.6
                }
                HoverHandler { cursorShape: dragHandler.active ? Qt.ClosedHandCursor : Qt.OpenHandCursor }
                DragHandler {
                    id: dragHandler
                    target: null
                    property point startPosition
                    onActiveChanged: if (active) startPosition = Qt.point(panel.x, panel.y)
                    onTranslationChanged: {
                        panel.x = startPosition.x + translation.x
                        panel.y = startPosition.y + translation.y
                        panel.keepInBounds()
                    }
                }
            }
            UiIconButton { label: qsTr("Close"); iconName: "x"; implicitWidth: 28; implicitHeight: 28; onClicked: panel.close() }
        }
        UiButton { visible: !panel.extensionId.length; Layout.fillWidth: true; text: qsTr("Manage extensions…"); onClicked: { panel.close(); panel.manageRequested() } }
    }
}
