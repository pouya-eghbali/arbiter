pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic as Basic

Rectangle {
    id: shield
    required property var session
    signal closeRequested()
    anchors.fill: parent
    // Blurred page beneath a strong dark scrim, without a reveal transition.
    color: "#b3050910"
    visible: session !== null && session.locked
    z: 2000000
    focus: visible
    onVisibleChanged: { password.clear(); confirmation.clear(); if (visible) forceActiveFocus() }
    MouseArea { anchors.fill: parent; acceptedButtons: Qt.AllButtons; onWheel: function(wheel) { wheel.accepted=true } }
    Keys.onPressed: function(event) { if (event.key !== Qt.Key_Tab && event.key !== Qt.Key_Backtab) event.accepted = true }
    Rectangle {
        x: Math.max(12, (shield.width - width) / 2)
        y: Math.max(12, (shield.height - height) / 2)
        width: Math.min(456, shield.width - 24)
        height: Math.min(shield.height - 24, lockFields.implicitHeight + 48)
        radius: 24
        color: Theme.surface
        border.color: Theme.border
    }
    ColumnLayout {
        id: lockFields
        anchors.centerIn: parent
        width: Math.min(400, parent.width - 48)
        spacing: 14
        Basic.Label {
            Layout.fillWidth: true
            text: shield.session && shield.session.configured ? qsTr("Private browsing is locked") : qsTr("A little privacy, protected.")
            font.pixelSize: 26
            font.weight: Font.Medium
            color: Theme.text
            wrapMode: Text.Wrap
        }
        Basic.Label {
            Layout.fillWidth: true
            text: shield.session && shield.session.configured
                ? qsTr("Verify to return to your private tabs.")
                : qsTr("Choose a password for this private window. Use it if biometrics are unavailable. It is kept only for this session; closing the window clears it and its browsing data.")
            color: Theme.muted
            wrapMode: Text.Wrap
        }
        UiButton {
            Layout.fillWidth: true
            visible: shield.session !== null && shield.session.configured && shield.session.biometricAvailable
            enabled: shield.session !== null && !shield.session.busy
            highlighted: true
            text: shield.session ? qsTr("Unlock with %1").arg(shield.session.biometricName) : ""
            onClicked: shield.session.unlockBiometric()
        }
        UiTextField {
            id: password
            objectName: "privateSessionPassword"
            Layout.fillWidth: true
            echoMode: TextInput.Password
            maximumLength: 256
            placeholderText: qsTr("Private-session password")
            enabled: shield.session !== null && !shield.session.busy
            onAccepted: {
                if (!shield.session.configured) confirmation.forceActiveFocus()
                else { shield.session.unlockPassword(text); clear() }
            }
        }
        UiTextField {
            id: confirmation
            Layout.fillWidth: true
            visible: shield.session !== null && !shield.session.configured
            enabled: shield.session !== null && !shield.session.busy
            echoMode: TextInput.Password
            maximumLength: 256
            placeholderText: qsTr("Confirm password")
            onAccepted: setupButton.clicked()
        }
        Basic.Label {
            Layout.fillWidth: true
            visible: text.length > 0
            text: shield.session ? shield.session.status : ""
            color: Theme.muted
            wrapMode: Text.Wrap
        }
        Basic.BusyIndicator { running: shield.session !== null && shield.session.busy; visible: running; Layout.alignment: Qt.AlignHCenter }
        UiButton {
            id: setupButton
            Layout.fillWidth: true
            enabled: shield.session !== null && !shield.session.busy
            text: shield.session && shield.session.configured ? qsTr("Unlock with password") : qsTr("Start private browsing")
            onClicked: {
                if (shield.session.configured) shield.session.unlockPassword(password.text)
                else shield.session.setup(password.text, confirmation.text)
                password.clear(); confirmation.clear()
            }
        }
        UiButton { Layout.fillWidth: true; flat: true; text: qsTr("Close private window"); onClicked: shield.closeRequested() }
    }
}
