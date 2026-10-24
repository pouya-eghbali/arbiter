pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic as Basic

Item {
    id: panel
    required property var controller
    property bool privateMode: false
    property int presentedId: 0
    property int closingId: 0
    Connections {
        target: panel.controller
        function onChanged() {
            if (panel.controller.active) {
                panel.presentedId = panel.controller.requestId
                permissionPopup.open()
            } else permissionPopup.close()
        }
    }
    Basic.Popup {
        popupType: Basic.Popup.Item
        id: permissionPopup
        objectName: "sitePermissionPopup"
        parent: Basic.Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(480, panel.width - 32)
        padding: 24
        modal: true
        dim: true
        focus: true
        closePolicy: Basic.Popup.CloseOnEscape
        Basic.Overlay.modal: Rectangle { color: Theme.scrim }
        background: GlassSurface { radius: Theme.panelRadius }
        onAboutToHide: panel.closingId = panel.presentedId
        onClosed: panel.controller.dismiss(panel.closingId)
        height: Math.min(permissionContent.implicitHeight + topPadding + bottomPadding, panel.height - 32)
        contentItem: Basic.ScrollView {
            id: permissionScroll
            contentWidth: availableWidth
            clip: true
            ColumnLayout {
            id: permissionContent
            width: permissionScroll.availableWidth
            spacing: 18
            RowLayout {
                Layout.fillWidth: true
                Basic.Label { text: qsTr("Site permission"); font.pixelSize: 24; font.weight: Font.DemiBold; color: Theme.text; Layout.fillWidth: true; wrapMode: Text.Wrap; Layout.alignment: Qt.AlignVCenter }
                UiButton { iconName: "x"; flat: true; Layout.alignment: Qt.AlignVCenter; Accessible.name: qsTr("Dismiss permission request"); onClicked: panel.controller.dismiss(panel.presentedId) }
            }
            Basic.Label {
                objectName: "sitePermissionOrigin"
                text: panel.controller.origin
                textFormat: Text.PlainText
                wrapMode: Text.WrapAnywhere
                Layout.fillWidth: true
                font.weight: Font.DemiBold
                color: Theme.accent
            }
            Basic.Label {
                text: qsTr("Allow access to %1?").arg(panel.controller.label)
                textFormat: Text.PlainText
                wrapMode: Text.Wrap
                color: Theme.text
                Layout.fillWidth: true
            }
            Basic.Label {
                visible: panel.controller.topOrigin !== panel.controller.origin
                text: qsTr("Requested from a page at %1").arg(panel.controller.topOrigin)
                textFormat: Text.PlainText
                wrapMode: Text.WrapAnywhere
                color: Theme.muted
                Layout.fillWidth: true
            }
            Basic.Label {
                text: panel.controller.persistent
                    ? panel.privateMode ? qsTr("This decision lasts for this private session.") : qsTr("This decision is saved for this site. Change it in Settings → Privacy.")
                    : qsTr("This permission is temporary. The site must ask again next time.")
                wrapMode: Text.Wrap
                color: Theme.muted
                Layout.fillWidth: true
            }
            RowLayout {
                Layout.fillWidth: true
                UiButton {
                    objectName: "blockSitePermission"
                    text: qsTr("Block")
                    property int pressedId: 0
                    onPressed: pressedId = panel.presentedId
                    onClicked: panel.controller.decide(pressedId, false)
                }
                Item { Layout.fillWidth: true }
                UiButton {
                    objectName: "allowSitePermission"
                    text: qsTr("Allow")
                    primary: true
                    property int pressedId: 0
                    onPressed: pressedId = panel.presentedId
                    onClicked: panel.controller.decide(pressedId, true)
                }
            }
            }
        }
    }
}
