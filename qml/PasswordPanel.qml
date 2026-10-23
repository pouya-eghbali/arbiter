pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic as Basic

Item {
    id: panel
    required property var manager
    required property var activeView
    property var target: null
    property var accounts: []
    property string origin: ""
    property bool formAvailable: false
    property bool filling: false
    property int inspection: 0
    property string deleteId: ""
    property string deleteLabel: ""
    property bool editing: false

    function cancelFor(view: var) {
        if (target !== view) return
        inspection++
        if (target) target.passwordAgent.invalidate()
        fillPopup.close()
        target = null
    }
    function openFor(view: var) {
        if (!view || !manager.enabled) return
        if (target) cancelFor(target)
        target = view
        origin = ""
        accounts = []
        formAvailable = false
        filling = false
        manager.clearMessage()
        const request = ++inspection
        view.mainFrame.runJavaScript("globalThis.__arbiterPasswords?.inspect() || null", 1, function(result) {
            if (panel.inspection !== request || panel.target !== view || panel.activeView !== view) return
            view.passwordAgent.prepare(view.mainFrame.url, result || {})
        })
    }
    function openManager() {
        manager.clearMessage()
        managePopup.open()
    }
    function edit(origin: string, username: string) {
        editing = origin.length > 0
        siteField.text = origin
        usernameField.text = username
        passwordField.text = ""
        editPopup.open()
    }

    Connections {
        target: panel.target ? panel.target.passwordAgent : null
        function onPrepared(origin: string) {
            panel.origin = origin
            panel.formAvailable = origin.length > 0
            panel.accounts = origin ? panel.manager.accountsFor(origin) : []
            fillPopup.open()
        }
    }
    Connections {
        target: panel.manager
        function onPendingChanged() {
            if (panel.manager.pending) {
                saveUsername.text = panel.manager.pendingUsername
                savePopup.open()
            } else savePopup.close()
        }
        function onCredentialSaved() { if (editPopup.visible) editPopup.close() }
        function onStateChanged() {
            if (panel.manager.error || panel.manager.message) noticeTimer.restart()
            if (!panel.manager.enabled && panel.target) panel.cancelFor(panel.target)
        }
    }

    component Button: Basic.Button {
        id: control
        padding: 10
        horizontalPadding: 16
        background: Rectangle { radius: 7; color: control.down ? "#444444" : "#292929"; border.color: "#444444" }
        contentItem: Basic.Label { text: control.text; color: control.enabled ? "#eeeeee" : "#777777"; horizontalAlignment: Text.AlignHCenter; textFormat: Text.PlainText }
    }
    component Field: Basic.TextField {
        id: control
        implicitHeight: 42
        padding: 12
        selectByMouse: true
        color: "#eeeeee"
        background: Rectangle { color: "#111111"; radius: 7; border.color: control.activeFocus ? "#888888" : "#444444" }
    }
    component Popup: Basic.Popup {
        parent: Basic.Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(600, panel.width - 40)
        padding: 24
        modal: true
        dim: true
        focus: true
        closePolicy: Basic.Popup.CloseOnEscape | Basic.Popup.CloseOnPressOutside
        background: Rectangle { color: "#191919"; radius: 12; border.color: "#444444" }
    }

    Popup {
        id: fillPopup
        onClosed: {
            if (!panel.filling && panel.target) panel.target.passwordAgent.invalidate()
            if (panel.activeView) panel.activeView.forceActiveFocus()
        }
        contentItem: ColumnLayout {
            spacing: 14
            Basic.Label { text: qsTr("Passwords"); font.pixelSize: 22 }
            Basic.Label { text: panel.origin; Layout.fillWidth: true; wrapMode: Text.WrapAnywhere; textFormat: Text.PlainText }
            Basic.Label {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                text: !panel.formAvailable ? qsTr("Focus a login field on an HTTPS page, then open Passwords again. Embedded frames and forms that submit to another website are not filled.")
                    : panel.accounts.length === 0 ? qsTr("No saved accounts for this website.") : qsTr("Choose an account to fill. Arbiter will not submit the form.")
                color: "#aaaaaa"
            }
            Basic.ScrollView {
                visible: panel.accounts.length > 0
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(240, accountColumn.implicitHeight)
                contentWidth: availableWidth
                ColumnLayout {
                    id: accountColumn
                    width: parent.width
                    Repeater {
                        model: panel.accounts
                        delegate: Button {
                            required property var modelData
                            Layout.fillWidth: true
                            text: modelData.username || qsTr("No username")
                            enabled: !panel.manager.busy
                            onClicked: {
                                if (panel.target !== panel.activeView) return
                                panel.filling = true
                                panel.target.passwordAgent.fill(modelData.id, panel.target.mainFrame.url)
                                fillPopup.close()
                            }
                        }
                    }
                }
            }
            RowLayout {
                Button {
                    text: qsTr("Save current login")
                    enabled: panel.formAvailable && !panel.manager.busy && panel.target && !panel.target.profile.offTheRecord
                    onClicked: {
                        panel.target.mainFrame.runJavaScript("globalThis.__arbiterPasswords?.capture() === true", 1)
                        fillPopup.close()
                    }
                }
                Button { text: qsTr("Manage passwords"); onClicked: { fillPopup.close(); panel.openManager() } }
            }
        }
    }

    Popup {
        id: savePopup
        closePolicy: Basic.Popup.CloseOnEscape
        onClosed: if (panel.manager.pending && !panel.manager.busy) panel.manager.dismissPending()
        contentItem: ColumnLayout {
            spacing: 14
            Basic.Label { text: panel.manager.pendingUpdate ? qsTr("Update password?") : qsTr("Save password?"); font.pixelSize: 22 }
            Basic.Label {
                text: panel.manager.pendingOrigin
                textFormat: Text.PlainText
                Layout.fillWidth: true
                wrapMode: Text.WrapAnywhere
            }
            Basic.Label {
                text: qsTr("Save the submitted password in your system credential store. If sign-in failed, dismiss this prompt.")
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                color: "#aaaaaa"
            }
            Field { id: saveUsername; Layout.fillWidth: true; placeholderText: qsTr("Username (optional)"); Accessible.name: qsTr("Username to save") }
            Basic.Label { text: panel.manager.error; visible: text.length > 0; Layout.fillWidth: true; wrapMode: Text.Wrap }
            RowLayout {
                Button { text: qsTr("Not now"); enabled: !panel.manager.busy; onClicked: panel.manager.dismissPending() }
                Item { Layout.fillWidth: true }
                Button { text: qsTr("Save"); enabled: !panel.manager.busy; onClicked: panel.manager.acceptPending(saveUsername.text) }
            }
        }
    }

    Popup {
        id: managePopup
        height: Math.min(650, panel.height - 60)
        contentItem: ColumnLayout {
            spacing: 14
            RowLayout {
                Basic.Label { text: qsTr("Saved passwords"); font.pixelSize: 22; Layout.fillWidth: true }
                Button { text: qsTr("Done"); onClicked: managePopup.close() }
            }
            Basic.CheckBox {
                text: qsTr("Offer to save and fill passwords")
                checked: panel.manager.enabled
                onToggled: panel.manager.enabled = checked
            }
            Basic.Label {
                text: qsTr("Passwords use macOS Keychain, Windows Credential Manager, or a Linux keyring. Websites and usernames are indexed locally. Credentials do not sync between devices.")
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                color: "#aaaaaa"
            }
            Field { id: searchField; Layout.fillWidth: true; placeholderText: qsTr("Search sites and usernames"); Accessible.name: qsTr("Search saved passwords") }
            Basic.ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                contentWidth: availableWidth
                ColumnLayout {
                    width: parent.width
                    Repeater {
                        model: panel.manager.entries
                        delegate: RowLayout {
                            id: credentialRow
                            required property var modelData
                            visible: (modelData.origin + " " + modelData.username).toLowerCase().includes(searchField.text.toLowerCase())
                            Layout.fillWidth: true
                            ColumnLayout {
                                Layout.fillWidth: true
                                Basic.Label { text: credentialRow.modelData.origin; textFormat: Text.PlainText; Layout.fillWidth: true; elide: Text.ElideMiddle }
                                Basic.Label { text: credentialRow.modelData.username || qsTr("No username"); textFormat: Text.PlainText; Layout.fillWidth: true; elide: Text.ElideRight; color: "#aaaaaa" }
                            }
                            Button {
                                text: qsTr("Update")
                                enabled: !panel.manager.busy && panel.manager.enabled
                                onClicked: panel.edit(credentialRow.modelData.origin, credentialRow.modelData.username)
                            }
                            Button {
                                text: qsTr("Delete")
                                enabled: !panel.manager.busy
                                onClicked: {
                                    panel.deleteId = credentialRow.modelData.id
                                    panel.deleteLabel = credentialRow.modelData.origin + " — " + credentialRow.modelData.username
                                    deletePopup.open()
                                }
                            }
                        }
                    }
                    Basic.Label { visible: panel.manager.entries.length === 0; text: qsTr("No saved passwords yet."); color: "#aaaaaa" }
                }
            }
            Basic.Label { text: panel.manager.error; visible: text.length > 0; Layout.fillWidth: true; wrapMode: Text.Wrap }
            Button { text: qsTr("Add login"); enabled: !panel.manager.busy && panel.manager.enabled; onClicked: panel.edit("", "") }
        }
    }

    Popup {
        id: editPopup
        closePolicy: Basic.Popup.CloseOnEscape
        onAboutToShow: showPassword.checked = false
        onClosed: { passwordField.text = ""; siteField.text = ""; usernameField.text = "" }
        contentItem: ColumnLayout {
            spacing: 14
            Basic.Label { text: panel.editing ? qsTr("Replace saved password") : qsTr("Add login"); font.pixelSize: 22 }
            Field { id: siteField; Layout.fillWidth: true; placeholderText: qsTr("https://example.com"); readOnly: panel.editing; Accessible.name: qsTr("Website") }
            Field { id: usernameField; Layout.fillWidth: true; placeholderText: qsTr("Username (optional)"); readOnly: panel.editing; Accessible.name: qsTr("Username") }
            Field { id: passwordField; Layout.fillWidth: true; placeholderText: qsTr("Password"); echoMode: TextInput.Password; Accessible.name: qsTr("Password") }
            Basic.Label {
                text: qsTr("Saving here does not change the password on the website. Use the same password when creating or updating your website account.")
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                color: "#aaaaaa"
            }
            Basic.Label { text: panel.manager.error; visible: text.length > 0; Layout.fillWidth: true; wrapMode: Text.Wrap }
            RowLayout {
                Button { text: qsTr("Generate"); onClicked: passwordField.text = panel.manager.generatePassword() }
                Button { id: showPassword; text: qsTr("Show"); checkable: true; onToggled: passwordField.echoMode = checked ? TextInput.Normal : TextInput.Password }
                Item { Layout.fillWidth: true }
                Button { text: qsTr("Cancel"); enabled: !panel.manager.busy; onClicked: editPopup.close() }
                Button {
                    text: qsTr("Save")
                    enabled: !panel.manager.busy && siteField.text.length > 0 && passwordField.text.length > 0
                    onClicked: panel.manager.save(siteField.text, usernameField.text, passwordField.text)
                }
            }
        }
    }
    Popup {
        id: deletePopup
        contentItem: ColumnLayout {
            spacing: 14
            Basic.Label { text: qsTr("Delete this password?"); font.pixelSize: 22 }
            Basic.Label { text: panel.deleteLabel; textFormat: Text.PlainText; Layout.fillWidth: true; wrapMode: Text.WrapAnywhere }
            RowLayout {
                Button { text: qsTr("Cancel"); onClicked: deletePopup.close() }
                Button { text: qsTr("Delete"); enabled: !panel.manager.busy; onClicked: { panel.manager.remove(panel.deleteId); deletePopup.close() } }
            }
        }
    }

    Timer { id: noticeTimer; interval: 9000; onTriggered: panel.manager.clearMessage() }
    Basic.Label {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 60
        width: Math.min(600, panel.width - 40)
        padding: 14
        text: panel.manager.error || panel.manager.message
        visible: text.length > 0 && !managePopup.visible && !editPopup.visible && !savePopup.visible
        wrapMode: Text.Wrap
        textFormat: Text.PlainText
        background: Rectangle { radius: 10; color: "#292929"; border.color: "#555555" }
    }
}
