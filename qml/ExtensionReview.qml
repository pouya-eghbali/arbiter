pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic as Basic
import Arbiter

Basic.Popup {
    popupType: Basic.Popup.Item
    id: panel
    objectName: "extensionPermissionReview"
    required property var catalog
    property bool allowed: true
    property var reviewOwner: null
    property var details: ({})
    property bool internalClose: false
    function synchronize() {
        if (!allowed || !catalog || !catalog.review.id) {
            if (visible) { internalClose = true; close() }
            return
        }
        if (visible) {
            if (reviewOwner !== catalog || details.id !== catalog.review.id) {
                internalClose = true
                close()
            }
            return
        }
        reviewOwner = catalog
        details = catalog.review
        internalClose = false
        open()
    }
    function decline() {
        if (reviewOwner && reviewOwner.review.id === details.id) reviewOwner.cancelReview()
        close()
    }
    onCatalogChanged: Qt.callLater(synchronize)
    onAllowedChanged: Qt.callLater(synchronize)
    Component.onCompleted: Qt.callLater(synchronize)
    Connections {
        target: panel.catalog
        function onChanged() { Qt.callLater(panel.synchronize) }
    }
    parent: Basic.Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(560, parent.width - 32)
    height: Math.min(parent.height - 48, reviewContent.implicitHeight + topPadding + bottomPadding)
    padding: 24
    modal: true
    dim: true
    focus: true
    closePolicy: Basic.Popup.CloseOnEscape
    background: GlassSurface { dense: true; radius: Theme.panelRadius }
    Basic.Overlay.modal: Rectangle { color: Theme.scrim }
    onOpened: declineButton.forceActiveFocus()
    onClosed: {
        const owner = reviewOwner
        const id = details.id
        reviewOwner = null
        details = ({})
        if (!internalClose && owner && owner.review.id === id) owner.cancelReview()
        internalClose = false
        Qt.callLater(synchronize)
    }
    contentItem: ColumnLayout {
        id: reviewContent
        spacing: 16
        Basic.Label {
            Layout.fillWidth: true
            text: qsTr("Add %1?").arg(panel.details.name || "")
            textFormat: Text.PlainText
            font.pixelSize: 24
            font.weight: Font.DemiBold
            color: Theme.text
            wrapMode: Text.Wrap
            maximumLineCount: 3
            elide: Text.ElideRight
        }
        Basic.Label {
            Layout.fillWidth: true
            text: qsTr("Review this extension’s requested permissions and website access. Add extension approves these permissions and enables it in this browser profile.")
            color: Theme.muted
            wrapMode: Text.Wrap
        }
        Basic.ScrollView {
            id: permissionsScroll
            objectName: "extensionReviewPermissions"
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredHeight: Math.min(240, permissionsText.implicitHeight)
            Layout.minimumHeight: 40
            clip: true
            contentWidth: availableWidth
            Basic.Label {
                id: permissionsText
                width: permissionsScroll.availableWidth
                text: panel.details.permissions && panel.details.permissions.length
                    ? panel.details.permissions.join("\n") : qsTr("No permissions or website access declared.")
                textFormat: Text.PlainText
                color: Theme.text
                wrapMode: Text.WrapAnywhere
            }
        }
        Basic.Label {
            Layout.fillWidth: true
            visible: panel.reviewOwner && panel.reviewOwner.review.id === panel.details.id && panel.reviewOwner.status.length > 0
            text: panel.reviewOwner ? panel.reviewOwner.status : ""
            textFormat: Text.PlainText
            color: Theme.muted
            wrapMode: Text.Wrap
        }
        RowLayout {
            Layout.fillWidth: true
            UiButton { id: declineButton; objectName: "declineExtensionButton"; text: qsTr("Keep disabled"); onClicked: panel.decline() }
            Item { Layout.fillWidth: true }
            UiButton {
                objectName: "approveExtensionButton"
                text: qsTr("Add extension")
                primary: true
                enabled: panel.allowed && panel.reviewOwner && panel.reviewOwner === panel.catalog
                    && panel.reviewOwner.review.id === panel.details.id
                onClicked: panel.reviewOwner.approve()
            }
        }
    }
}
