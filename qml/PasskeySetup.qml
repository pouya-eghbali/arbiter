pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic as Basic

Basic.Popup {
    id: setup
    required property var controller
    parent: Basic.Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(540, parent ? parent.width - 40 : 540)
    padding: 24
    modal: true
    focus: true
    closePolicy: controller.busy ? Basic.Popup.NoAutoClose : Basic.Popup.CloseOnEscape
    onClosed: { phrase.text = ""; confirmation.text = "" }
    background: GlassSurface { dense: true }
    Connections {
        target: setup.controller
        function onChanged() {
            if (setup.controller.configured) setup.close()
            if (!setup.controller.busy) { phrase.text = ""; confirmation.text = "" }
        }
    }
    contentItem: ColumnLayout {
        spacing: 14
        Basic.Label { text: qsTr("Set up Arbiter passkeys"); font.pixelSize: 22 }
        Basic.Label {
            text: qsTr("Choose a passphrase of at least 12 characters. Use it to create or use an Arbiter passkey, or enable optional biometric unlocking in Settings. Private keys are encrypted and kept in your operating system's credential store on this computer. There is no sync or passphrase recovery; losing this profile, its system credentials, or the passphrase can lose access to these passkeys. Keep another sign-in method for your accounts.")
            textFormat: Text.PlainText
            Layout.fillWidth: true
            wrapMode: Text.Wrap
        }
        UiTextField { id: phrase; Layout.fillWidth: true; echoMode: TextInput.Password; placeholderText: qsTr("Vault passphrase"); enabled: !setup.controller.busy }
        UiTextField { id: confirmation; Layout.fillWidth: true; echoMode: TextInput.Password; placeholderText: qsTr("Confirm vault passphrase"); enabled: !setup.controller.busy }
        Basic.Label { text: setup.controller.error; visible: text.length > 0; textFormat: Text.PlainText; Layout.fillWidth: true; wrapMode: Text.Wrap }
        RowLayout {
            UiButton { text: qsTr("Cancel"); enabled: !setup.controller.busy; onClicked: setup.close() }
            Item { Layout.fillWidth: true }
            UiButton {
                text: qsTr("Create vault")
                primary: true
                enabled: !setup.controller.busy && phrase.text.length >= 12 && phrase.text === confirmation.text
                onClicked: { setup.controller.setup(phrase.text, confirmation.text); phrase.text = ""; confirmation.text = "" }
            }
        }
    }
}
