pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic as Basic

Item {
    id: panel
    property var owner: null
    property var request: null
    property string tabOrigin: ""
    function show(view: var, mediaRequest: var, origin: string) {
        cancel()
        owner = view
        request = mediaRequest
        tabOrigin = origin
        sourcePicker.currentIndex = 0
        sharePopup.open()
    }
    function cancelFor(view: var) { if (owner === view) cancel() }
    function cancel() {
        const pending = request
        if (pending) pending.cancel()
        request = null
        owner = null
        sharePopup.close()
    }
    function select(index: int) {
        if (!request) return
        const pending = request
        const screen = sourcePicker.currentIndex === 0
        const model = screen ? pending.screensModel : pending.windowsModel
        if (index < 0 || index >= model.rowCount()) return
        const modelIndex = model.index(index, 0)
        // Resolve while the QML gadget still belongs to the request property.
        if (screen) pending.selectScreen(modelIndex)
        else pending.selectWindow(modelIndex)
        request = null
        owner = null
        sharePopup.close()
    }
    Basic.Popup {
        id: sharePopup
        objectName: "desktopSharePopup"
        parent: Basic.Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(520, panel.width - 32)
        padding: 24
        modal: true
        dim: true
        focus: true
        closePolicy: Basic.Popup.CloseOnEscape
        Basic.Overlay.modal: Rectangle { color: Theme.scrim }
        background: GlassSurface { radius: 26 }
        onClosed: if (panel.request) panel.cancel()
        height: Math.min(shareContent.implicitHeight + topPadding + bottomPadding, panel.height - 32)
        contentItem: Basic.ScrollView {
            id: shareScroll
            contentWidth: availableWidth
            clip: true
            ColumnLayout {
            id: shareContent
            width: shareScroll.availableWidth
            spacing: 16
            Basic.Label {
                Layout.fillWidth: true
                text: qsTr("Choose what to share")
                wrapMode: Text.Wrap
                font.pixelSize: 24
                color: Theme.text
                font.weight: Font.DemiBold
            }
            Basic.Label {
                Layout.fillWidth: true
                text: qsTr("Tab at %1").arg(panel.tabOrigin)
                textFormat: Text.PlainText
                wrapMode: Text.WrapAnywhere
                color: Theme.muted
            }
            UiComboBox {
                id: sourcePicker
                model: [qsTr("Screens"), qsTr("Windows")]
                Layout.fillWidth: true
                leftPadding: 16
            }
            ListView {
                id: sources
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(260, Math.max(48, contentHeight))
                clip: true
                spacing: 6
                boundsBehavior: Flickable.StopAtBounds
                model: panel.request ? sourcePicker.currentIndex === 0 ? panel.request.screensModel : panel.request.windowsModel : null
                Basic.ScrollBar.vertical: Basic.ScrollBar { }
                delegate: UiButton {
                    required property int index
                    required property var model
                    width: sources.width
                    textAlignment: Text.AlignLeft
                    rightPadding: sources.contentHeight > sources.height ? 28 : 16
                    text: model.display
                    onClicked: panel.select(index)
                }
                Basic.Label { anchors.centerIn: parent; visible: sources.count === 0; text: qsTr("No sources available"); color: Theme.muted }
            }
            UiButton {
                Layout.alignment: Qt.AlignRight
                text: qsTr("Cancel")
                onClicked: panel.cancel()
            }
            }
        }
    }
}
