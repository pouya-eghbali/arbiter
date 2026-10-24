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
    property bool awaitingInspection: false
    property rect suggestionField: Qt.rect(0, 0, 0, 0)

    function updateSuggestionAnchor() {
        if (!target) return
        const at = target.mapToItem(Basic.Overlay.overlay,
            suggestionField.x * target.zoomFactor, suggestionField.y * target.zoomFactor)
        suggestionPopup.field = Qt.rect(at.x, at.y,
            suggestionField.width * target.zoomFactor, suggestionField.height * target.zoomFactor)
    }
    onWidthChanged: if (suggestionPopup.visible) updateSuggestionAnchor()
    onHeightChanged: if (suggestionPopup.visible) updateSuggestionAnchor()

    function cancelFor(view: var) {
        if (target !== view) return
        awaitingInspection = false
        inspection++
        if (target) target.passwordAgent.invalidate()
        suggestionPopup.close()
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
        awaitingInspection = true
        const request = ++inspection
        view.mainFrame.runJavaScript("globalThis.__arbiterPasswords?.inspect() || null", 1, function(result) {
            if (panel.inspection !== request || panel.target !== view || panel.activeView !== view) return
            view.passwordAgent.prepare(view.mainFrame.url, result || {})
        })
    }
    function offerFor(view: var, site: string, rect: rect) {
        if (!view || view !== activeView || !manager.enabled || manager.busy || fillPopup.visible || savePopup.visible) return
        const matches = manager.accountsFor(site)
        if (matches.length === 0) return
        if (target && target !== view) cancelFor(target)
        target = view
        origin = site
        accounts = matches
        formAvailable = true
        filling = false
        suggestionField = rect
        updateSuggestionAnchor()
        const flick = suggestionScroll.contentItem as Flickable
        flick.contentY = 0
        suggestionPopup.open()
    }
    function fillAccount(id: string) {
        if (!target || target !== activeView || manager.busy) return
        filling = true
        target.passwordAgent.fill(id, target.mainFrame.url)
        suggestionPopup.close()
        fillPopup.close()
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
            if (!panel.awaitingInspection) return
            panel.awaitingInspection = false
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

    component Popup: Basic.Popup {
        parent: Basic.Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(600, panel.width - 40)
        padding: 24
        modal: true
        dim: true
        Basic.Overlay.modal: Rectangle { color: Theme.scrim }
        focus: true
        closePolicy: Basic.Popup.CloseOnEscape | Basic.Popup.CloseOnPressOutside
        background: GlassSurface { dense: true }
    }

    // Compact, nonmodal menu: keep typing focus in the page and constrain the
    // entire scrolling surface, including its actions, to the available space.
    component SuggestionAction: Basic.ItemDelegate {
        id: action
        implicitHeight: 34
        width: suggestionContent.width
        horizontalPadding: 10
        hoverEnabled: true
        opacity: enabled ? 1 : 0.45
        contentItem: Text {
            text: action.text
            textFormat: Text.PlainText
            font.pixelSize: 13
            color: Theme.text
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
        background: Rectangle {
            radius: 8
            color: action.down ? Theme.pressed : action.hovered || action.highlighted ? Theme.hover : "transparent"
        }
    }
    Basic.Popup {
        id: suggestionPopup
        objectName: "passwordSuggestions"
        parent: Basic.Overlay.overlay
        property rect field: Qt.rect(0, 0, 0, 0)
        readonly property real edge: 8
        readonly property real desiredHeight: Math.min(340, suggestionContent.implicitHeight + padding * 2)
        readonly property real below: Math.max(0, parent.height - edge - Math.max(edge, field.y + field.height + 6))
        readonly property real above: Math.max(0, Math.min(parent.height - edge, field.y - 6) - edge)
        readonly property bool opensBelow: below >= desiredHeight || below >= above
        width: Math.max(0, Math.min(300, parent.width - edge * 2))
        height: Math.min(desiredHeight, opensBelow ? below : above)
        x: Math.max(edge, Math.min(field.x, parent.width - width - edge))
        y: Math.max(edge, Math.min(opensBelow ? field.y + field.height + 6 : field.y - 6 - height,
                                  parent.height - height - edge))
        padding: 8
        modal: false
        focus: false
        closePolicy: Basic.Popup.CloseOnEscape | Basic.Popup.CloseOnPressOutside
        background: GlassSurface { dense: true; radius: 14 }
        contentItem: Basic.ScrollView {
            id: suggestionScroll
            objectName: "passwordSuggestionScroll"
            clip: true
            contentWidth: availableWidth
            Basic.ScrollBar.horizontal.policy: Basic.ScrollBar.AlwaysOff
            Basic.ScrollBar.vertical.policy: Basic.ScrollBar.AsNeeded
            Column {
                id: suggestionContent
                width: suggestionScroll.availableWidth
                spacing: 2
                Basic.Label {
                    width: parent.width
                    leftPadding: 10; topPadding: 4; bottomPadding: 6
                    text: qsTr("Passwords"); color: Theme.muted; font.pixelSize: 12
                }
                Repeater {
                    model: panel.accounts
                    SuggestionAction {
                        required property var modelData
                        text: modelData.username || qsTr("No username")
                        enabled: !panel.manager.busy
                        onClicked: panel.fillAccount(modelData.id)
                    }
                }

            }
        }
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
                color: Theme.muted
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
                        delegate: UiButton {
                            required property var modelData
                            Layout.fillWidth: true
                            text: modelData.username || qsTr("No username")
                            enabled: !panel.manager.busy
                            onClicked: {
                                panel.fillAccount(modelData.id)
                            }
                        }
                    }
                }
            }
            RowLayout {
                UiButton { text: qsTr("Manage passwords"); onClicked: { fillPopup.close(); panel.openManager() } }
            }
        }
    }

    Popup {
        id: savePopup
        objectName: "savePasswordPrompt"
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
                color: Theme.muted
            }
            UiTextField { id: saveUsername; Layout.fillWidth: true; placeholderText: qsTr("Username (optional)"); Accessible.name: qsTr("Username to save") }
            Basic.Label { text: panel.manager.error; visible: text.length > 0; Layout.fillWidth: true; wrapMode: Text.Wrap }
            RowLayout {
                UiButton { text: qsTr("Not now"); enabled: !panel.manager.busy; onClicked: panel.manager.dismissPending() }
                Item { Layout.fillWidth: true }
                UiButton { text: qsTr("Save"); enabled: !panel.manager.busy; onClicked: panel.manager.acceptPending(saveUsername.text) }
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
                UiButton { text: qsTr("Done"); onClicked: managePopup.close() }
            }
            UiCheckBox {
                text: qsTr("Offer to save and fill passwords")
                checked: panel.manager.enabled
                onToggled: panel.manager.enabled = checked
            }
            Basic.Label {
                text: qsTr("Passwords use macOS Keychain, Windows Credential Manager, or a Linux keyring. Websites and usernames are indexed locally. Credentials do not sync between devices.")
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                color: Theme.muted
            }
            UiTextField { id: searchField; Layout.fillWidth: true; placeholderText: qsTr("Search sites and usernames"); Accessible.name: qsTr("Search saved passwords") }
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
                                Basic.Label { text: credentialRow.modelData.username || qsTr("No username"); textFormat: Text.PlainText; Layout.fillWidth: true; elide: Text.ElideRight; color: Theme.muted }
                            }
                            UiButton {
                                text: qsTr("Update")
                                enabled: !panel.manager.busy && panel.manager.enabled
                                onClicked: panel.edit(credentialRow.modelData.origin, credentialRow.modelData.username)
                            }
                            UiButton {
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
                    Basic.Label { visible: panel.manager.entries.length === 0; text: qsTr("No saved passwords yet."); color: Theme.muted }
                }
            }
            Basic.Label { text: panel.manager.error; visible: text.length > 0; Layout.fillWidth: true; wrapMode: Text.Wrap }
            UiButton { text: qsTr("Add login"); enabled: !panel.manager.busy && panel.manager.enabled; onClicked: panel.edit("", "") }
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
            UiTextField { id: siteField; Layout.fillWidth: true; placeholderText: qsTr("https://example.com"); readOnly: panel.editing; Accessible.name: qsTr("Website") }
            UiTextField { id: usernameField; Layout.fillWidth: true; placeholderText: qsTr("Username (optional)"); readOnly: panel.editing; Accessible.name: qsTr("Username") }
            UiTextField { id: passwordField; Layout.fillWidth: true; placeholderText: qsTr("Password"); echoMode: TextInput.Password; Accessible.name: qsTr("Password") }
            Basic.Label {
                text: qsTr("Saving here does not change the password on the website. Use the same password when creating or updating your website account.")
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                color: Theme.muted
            }
            Basic.Label { text: panel.manager.error; visible: text.length > 0; Layout.fillWidth: true; wrapMode: Text.Wrap }
            RowLayout {
                UiButton { text: qsTr("Generate"); onClicked: passwordField.text = panel.manager.generatePassword() }
                UiButton { id: showPassword; text: qsTr("Show"); checkable: true; onToggled: passwordField.echoMode = checked ? TextInput.Normal : TextInput.Password }
                Item { Layout.fillWidth: true }
                UiButton { text: qsTr("Cancel"); enabled: !panel.manager.busy; onClicked: editPopup.close() }
                UiButton {
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
                UiButton { text: qsTr("Cancel"); onClicked: deletePopup.close() }
                UiButton { text: qsTr("Delete"); enabled: !panel.manager.busy; onClicked: { panel.manager.remove(panel.deleteId); deletePopup.close() } }
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
        background: GlassSurface { dense: true; radius: 12 }
    }
}
