pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic as Basic
import QtWebEngine

Item {
    id: panel
    required property var controller
    required property var passkeys
    property var owner: null

    function show(view: var, request: var) {
        controller.handle(request)
        passkeys.handle(request)
        owner = controller.active ? view : null
    }
    function cancelFor(view: var) {
        if (owner === view) controller.cancel()
    }
    Connections {
        target: panel.controller
        function onChanged() {
            pinField.text = ""
            confirmationField.text = ""
            if (panel.controller.active) {
                authPopup.open()
                if (panel.controller.state === WebEngineWebAuthUxRequest.WebAuthUxState.CollectPin)
                    Qt.callLater(function() { pinField.forceActiveFocus() })
            } else {
                panel.passkeys.clear()
                panel.owner = null
                authPopup.close()
            }
        }
        function onPinSubmitted() { pinField.text = ""; confirmationField.text = "" }
    }

    Connections {
        target: panel.passkeys
        function onVerificationSubmitted() { vaultPhrase.text = "" }
        function onChanged() { if (!panel.passkeys.verifying) vaultPhrase.text = "" }
    }
    Basic.Popup {
        id: authPopup
        parent: Basic.Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(580, panel.width - 40)
        padding: 24
        modal: true
        dim: true
        focus: true
        closePolicy: Basic.Popup.CloseOnEscape
        onClosed: { vaultPhrase.text = ""; if (panel.controller.active) panel.controller.cancel() }
        background: Rectangle { radius: 12; color: "#191919"; border.color: "#444444" }
        contentItem: ColumnLayout {
            spacing: 14
            Basic.Label { text: qsTr("Website authentication"); font.pixelSize: 22 }
            Basic.Label {
                text: panel.controller.relyingParty
                textFormat: Text.PlainText
                Layout.fillWidth: true
                wrapMode: Text.WrapAnywhere
                font.weight: Font.DemiBold
            }
            Basic.Label {
                text: panel.controller.description
                textFormat: Text.PlainText
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                color: "#aaaaaa"
            }
            Basic.Label {
                visible: panel.passkeys.active
                text: panel.passkeys.origin
                textFormat: Text.PlainText
                wrapMode: Text.WrapAnywhere
                Layout.fillWidth: true
            }
            RowLayout {
                visible: panel.passkeys.active && !panel.passkeys.verifying
                    && panel.controller.state === WebEngineWebAuthUxRequest.WebAuthUxState.Discovery
                Basic.Button { text: qsTr("Use Arbiter passkey"); visible: panel.passkeys.localAvailable; onClicked: panel.passkeys.useLocal() }
                Basic.Button { text: qsTr("Use a phone"); visible: panel.passkeys.phoneAvailable; onClicked: panel.passkeys.usePhone() }
                Basic.Button { text: qsTr("Windows Hello / security key"); visible: panel.passkeys.systemAvailable; onClicked: panel.passkeys.useSystem() }
            }
            Basic.Label {
                visible: panel.passkeys.active && !panel.passkeys.configured
                text: qsTr("Set up the passkey vault in Settings to save passkeys on this computer.")
                textFormat: Text.PlainText
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }
            Image {
                visible: panel.passkeys.qrImage.toString().length > 0
                source: panel.passkeys.qrImage
                cache: false
                smooth: false
                Layout.alignment: Qt.AlignHCenter
                Layout.preferredWidth: 270
                Layout.preferredHeight: 270
                fillMode: Image.PreserveAspectFit
            }
            Basic.Label {
                visible: panel.passkeys.qrImage.toString().length > 0
                text: qsTr("Scan with your phone's camera. Keep the phone nearby with Bluetooth and internet enabled. Your passkey stays on the phone.")
                textFormat: Text.PlainText
                Layout.fillWidth: true
                wrapMode: Text.Wrap
            }
            Basic.Button {
                visible: panel.passkeys.phoneAvailable && panel.passkeys.bluetooth === 1
                text: qsTr("Turn on Bluetooth")
                onClicked: panel.passkeys.powerOnBluetooth()
            }
            Basic.Button {
                visible: panel.passkeys.phoneAvailable && panel.passkeys.bluetooth === 3
                text: qsTr("Allow Bluetooth")
                onClicked: panel.passkeys.requestBluetoothPermission()
            }
            Basic.Label {
                visible: panel.passkeys.phoneAvailable && panel.passkeys.bluetooth === 2
                text: qsTr("Bluetooth access was denied. Enable it for Arbiter in system settings to use a phone.")
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }
            Basic.ComboBox {
                id: localAccount
                visible: panel.passkeys.verifying && !panel.passkeys.registration
                model: panel.passkeys.accounts
                textRole: "name"
                Layout.fillWidth: true
                contentItem: Basic.Label {
                    text: localAccount.displayText
                    textFormat: Text.PlainText
                    elide: Text.ElideRight
                    verticalAlignment: Text.AlignVCenter
                }
                delegate: Basic.ItemDelegate {
                    id: passkeyAccountDelegate
                    required property var modelData
                    width: localAccount.width
                    contentItem: Basic.Label {
                        text: passkeyAccountDelegate.modelData.name
                        textFormat: Text.PlainText
                        elide: Text.ElideRight
                    }
                }
            }
            Basic.TextField {
                id: vaultPhrase
                visible: panel.passkeys.verifying && (panel.passkeys.registration || panel.passkeys.accounts.length > 0)
                echoMode: TextInput.Password
                placeholderText: qsTr("Arbiter passkey vault passphrase")
                Accessible.name: placeholderText
                Layout.fillWidth: true
                enabled: !panel.passkeys.busy
                onAccepted: panel.passkeys.verify(localAccount.currentIndex, text)
            }
            Basic.Label {
                visible: panel.passkeys.verifying && !panel.passkeys.registration && panel.passkeys.accounts.length === 0
                text: qsTr("No Arbiter passkey matches this request. Cancel and choose another sign-in method.")
                textFormat: Text.PlainText
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }
            Basic.Button {
                visible: panel.passkeys.verifying
                text: panel.passkeys.registration ? qsTr("Create passkey in Arbiter") : qsTr("Sign in with Arbiter")
                enabled: !panel.passkeys.busy && vaultPhrase.text.length > 0
                    && (panel.passkeys.registration || localAccount.currentIndex >= 0)
                onClicked: panel.passkeys.verify(localAccount.currentIndex, vaultPhrase.text)
            }
            Basic.Label {
                visible: panel.passkeys.error.length > 0
                text: panel.passkeys.error
                textFormat: Text.PlainText
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }
            Basic.ScrollView {
                visible: panel.controller.state === WebEngineWebAuthUxRequest.WebAuthUxState.SelectAccount
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(220, accountList.implicitHeight)
                contentWidth: availableWidth
                ColumnLayout {
                    id: accountList
                    width: parent.width
                    Repeater {
                        model: panel.controller.accounts
                        delegate: Basic.Button {
                            id: accountButton
                            required property string modelData
                            text: modelData || qsTr("Unnamed account")
                            Layout.fillWidth: true
                            contentItem: Basic.Label {
                                text: accountButton.text
                                textFormat: Text.PlainText
                                elide: Text.ElideRight
                            }
                            onClicked: panel.controller.selectAccount(modelData)
                        }
                    }
                }
            }
            Basic.TextField {
                id: pinField
                visible: panel.controller.state === WebEngineWebAuthUxRequest.WebAuthUxState.CollectPin
                Layout.fillWidth: true
                echoMode: TextInput.Password
                placeholderText: qsTr("Security key PIN")
                Accessible.name: qsTr("Security key PIN")
                onAccepted: {
                    if (panel.controller.confirmPin) confirmationField.forceActiveFocus()
                    else panel.controller.submitPin(text, "")
                }
            }
            Basic.TextField {
                id: confirmationField
                visible: pinField.visible && panel.controller.confirmPin
                Layout.fillWidth: true
                echoMode: TextInput.Password
                placeholderText: qsTr("Confirm PIN")
                Accessible.name: qsTr("Confirm security key PIN")
                onAccepted: panel.controller.submitPin(pinField.text, text)
            }
            Basic.Label {
                text: panel.controller.error
                textFormat: Text.PlainText
                visible: text.length > 0
                Layout.fillWidth: true
                wrapMode: Text.Wrap
            }
            RowLayout {
                Basic.Button { text: qsTr("Cancel"); onClicked: panel.controller.cancel() }
                Item { Layout.fillWidth: true }
                Basic.Button { text: qsTr("Retry"); visible: panel.controller.canRetry; onClicked: panel.controller.retry() }
                Basic.Button {
                    text: qsTr("Continue")
                    visible: pinField.visible
                    enabled: pinField.text.length > 0
                    onClicked: panel.controller.submitPin(pinField.text, confirmationField.text)
                }
            }
        }
    }
}
