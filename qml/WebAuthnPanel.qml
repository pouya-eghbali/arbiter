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
        Basic.Overlay.modal: Rectangle { color: Theme.scrim }
        focus: true
        closePolicy: Basic.Popup.CloseOnEscape
        onClosed: { vaultPhrase.text = ""; if (panel.controller.active) panel.controller.cancel() }
        background: GlassSurface { dense: true }
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
                text: panel.passkeys.verifying
                    ? panel.passkeys.registration ? qsTr("Verify to create a passkey in Arbiter.") : qsTr("Verify to sign in with your Arbiter passkey.")
                    : panel.controller.description
                textFormat: Text.PlainText
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                color: Theme.muted
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
                UiButton { text: qsTr("Use Arbiter passkey"); visible: panel.passkeys.localAvailable; onClicked: panel.passkeys.useLocal() }
                UiButton { text: qsTr("Use a phone"); visible: panel.passkeys.phoneAvailable; onClicked: panel.passkeys.usePhone() }
                UiButton { text: qsTr("Windows Hello / security key"); visible: panel.passkeys.systemAvailable; onClicked: panel.passkeys.useSystem() }
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
            UiButton {
                visible: !panel.passkeys.verifying && panel.passkeys.phoneAvailable && panel.passkeys.bluetooth === 1
                text: qsTr("Turn on Bluetooth")
                onClicked: panel.passkeys.powerOnBluetooth()
            }
            UiButton {
                visible: !panel.passkeys.verifying && panel.passkeys.phoneAvailable && panel.passkeys.bluetooth === 3
                text: qsTr("Allow Bluetooth")
                onClicked: panel.passkeys.requestBluetoothPermission()
            }
            Basic.Label {
                visible: !panel.passkeys.verifying && panel.passkeys.phoneAvailable && panel.passkeys.bluetooth === 2
                text: qsTr("Bluetooth access was denied. Enable it for Arbiter in system settings to use a phone.")
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }
            UiComboBox {
                id: localAccount
                visible: panel.passkeys.verifying && !panel.passkeys.registration
                model: panel.passkeys.accounts
                textRole: "name"
                Layout.fillWidth: true
            }

            UiButton {
                visible: panel.passkeys.verifying && panel.passkeys.biometricEnabled && panel.passkeys.biometricAvailable
                text: qsTr("Use %1").arg(panel.passkeys.biometricName)
                objectName: "verifyBiometricButton"
                primary: true
                enabled: !panel.passkeys.busy && panel.passkeys.biometricReady
                    && (panel.passkeys.registration || localAccount.currentIndex >= 0)
                onClicked: panel.passkeys.verifyBiometric(localAccount.currentIndex)
            }
            Basic.Label {
                visible: panel.passkeys.verifying && panel.passkeys.biometricEnabled
                text: panel.passkeys.busy ? panel.passkeys.biometricStatus : panel.passkeys.biometricReady
                    ? qsTr("Or use your vault passphrase below.")
                    : qsTr("Use your passphrase once to enable biometric unlocking for this session.")
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                color: Theme.muted
            }
            UiTextField {
                id: vaultPhrase
                objectName: "passkeyVaultPhrase"
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
            UiButton {
                visible: panel.passkeys.verifying
                text: panel.passkeys.registration ? qsTr("Create passkey in Arbiter") : qsTr("Sign in with Arbiter")
                objectName: "verifyPassphraseButton"
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
                        delegate: UiButton {
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
            UiTextField {
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
            UiTextField {
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
                UiButton { text: qsTr("Cancel"); onClicked: panel.controller.cancel() }
                Item { Layout.fillWidth: true }
                UiButton { text: qsTr("Retry"); visible: panel.controller.canRetry; onClicked: panel.controller.retry() }
                UiButton {
                    text: qsTr("Continue")
                    visible: pinField.visible
                    enabled: pinField.text.length > 0
                    onClicked: panel.controller.submitPin(pinField.text, confirmationField.text)
                }
            }
        }
    }
}
