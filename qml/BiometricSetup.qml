pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic as Basic

Basic.Popup {
    popupType: Basic.Popup.Item
    id: setup
    required property var controller
    property bool submitted: false
    parent: Basic.Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(540, parent ? parent.width - 40 : 540)
    padding: 24
    modal: true
    focus: true
    closePolicy: controller.busy ? Basic.Popup.NoAutoClose : Basic.Popup.CloseOnEscape
    background: GlassSurface { dense: true }
    onOpened: { submitted = false; phrase.text = ""; phrase.forceActiveFocus() }
    onClosed: { phrase.text = ""; if (submitted && controller.busy) controller.cancelBiometric() }
    Connections {
        target: setup.controller
        function onChanged() {
            if (!setup.visible || !setup.submitted || setup.controller.busy) return
            phrase.text = ""
            if (setup.controller.biometricEnabled && setup.controller.error.length === 0) setup.close()
        }
    }
    contentItem: ColumnLayout {
        spacing: 14
        Basic.Label { text: qsTr("Enable %1").arg(setup.controller.biometricName); font.pixelSize: 22 }
        Basic.Label {
            text: qsTr("Enter your vault passphrase, then verify with your device. Later passkey operations can use device verification for up to 15 minutes while Arbiter stays active. Your passphrase remains the fallback and is required again after restarting or leaving the app.")
            Layout.fillWidth: true
            wrapMode: Text.Wrap
        }
        UiTextField {
            id: phrase
            objectName: "biometricEnrollmentPhrase"
            Layout.fillWidth: true
            echoMode: TextInput.Password
            placeholderText: qsTr("Vault passphrase")
            Accessible.name: placeholderText
            enabled: !setup.controller.busy
            onAccepted: enableButton.clicked()
        }
        Basic.Label { text: setup.controller.biometricStatus; Layout.fillWidth: true; wrapMode: Text.Wrap; color: Theme.muted }
        Basic.Label { text: setup.controller.error; visible: text.length > 0; textFormat: Text.PlainText; Layout.fillWidth: true; wrapMode: Text.Wrap }
        RowLayout {
            UiButton { text: qsTr("Cancel"); onClicked: { setup.controller.cancelBiometric(); setup.close() } }
            Item { Layout.fillWidth: true }
            UiButton {
                id: enableButton
                objectName: "enableBiometricsButton"
                text: qsTr("Enable")
                primary: true
                enabled: !setup.controller.busy && setup.controller.biometricAvailable && phrase.text.length > 0
                onClicked: {
                    if (!enabled) return
                    setup.submitted = true
                    setup.controller.setBiometricEnabled(true, phrase.text)
                    phrase.text = ""
                }
            }
        }
    }
}
