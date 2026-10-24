pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic as Basic
import QtQuick.Dialogs as Dialogs

ColumnLayout {
    id: page
    required property var controller
    spacing: 16
    Component.onCompleted: if (visible) controller.refreshProfiles()
    onVisibleChanged: if (visible) controller.refreshProfiles()
    Basic.Label {
        text: qsTr("Import passwords + history")
        color: Theme.text
        font.pixelSize: 18
        Layout.fillWidth: true
        wrapMode: Text.Wrap
    }
    RowLayout {
        UiButton { text: qsTr("From browser"); highlighted: page.controller.direct; enabled: !page.controller.busy; onClicked: page.controller.direct = true }
        UiButton { text: qsTr("From files"); highlighted: !page.controller.direct; enabled: !page.controller.busy; onClicked: page.controller.direct = false }
    }
    ColumnLayout {
        visible: page.controller.direct
        Layout.fillWidth: true
        spacing: 16
        Basic.Label {
            text: qsTr("Choose the browser and profile you want to bring into Arbiter. Your system may ask you to authorize access to its password vault.")
            color: Theme.muted
            Layout.fillWidth: true
            wrapMode: Text.Wrap
        }
        UiComboBox {
            id: profiles
            objectName: "importProfileChoice"
            Layout.fillWidth: true
            model: page.controller.profiles
            textRole: "label"
            enabled: !page.controller.busy
            Accessible.name: qsTr("Browser and profile to import")
            onActivated: page.controller.selectProfile(currentIndex)
        }
        UiButton { text: qsTr("Refresh browsers"); enabled: !page.controller.busy; onClicked: { page.controller.refreshProfiles(); profiles.currentIndex = 0 } }
        Basic.Label {
            text: qsTr("No supported browser profiles were found. You can import exported files instead.")
            visible: page.controller.profiles.length <= 1
            color: Theme.muted
            Layout.fillWidth: true
            wrapMode: Text.Wrap
        }
        UiCheckBox {
            text: page.controller.directPasswordsSupported ? qsTr("Passwords") : qsTr("Passwords (unavailable for direct import)")
            checked: page.controller.importPasswords
            enabled: !page.controller.busy && page.controller.directPasswordsSupported
            onToggled: page.controller.importPasswords = checked
        }
        UiCheckBox { text: page.controller.directHistorySupported ? qsTr("Browsing history") : qsTr("Browsing history (none found for this profile)"); checked: page.controller.importHistory; enabled: !page.controller.busy && page.controller.directHistorySupported; onToggled: page.controller.importHistory = checked }
        UiButton {
            objectName: "directImportButton"
            text: qsTr("Import from browser")
            highlighted: true
            enabled: !page.controller.busy
            onClicked: page.controller.importFromBrowser()
        }
    }
    ColumnLayout {
        visible: !page.controller.direct
        Layout.fillWidth: true
        spacing: 16
        Basic.Label {
            text: qsTr("Choose a password CSV, a history file, or both. Preview the counts before importing.")
            color: Theme.muted
            Layout.fillWidth: true
            wrapMode: Text.Wrap
        }
        Basic.Label { text: qsTr("Passwords"); color: Theme.text; font.weight: Font.DemiBold }
        RowLayout {
            UiButton { text: qsTr("Choose password CSV…"); enabled: !page.controller.busy; onClicked: passwordFile.open() }
            UiButton { text: qsTr("Clear"); enabled: !page.controller.busy && page.controller.passwordFileLabel.length > 0; onClicked: page.controller.setPasswordFile("") }
        }
        Basic.Label { text: page.controller.passwordFileLabel || qsTr("No password file selected"); textFormat: Text.PlainText; color: Theme.muted; Layout.fillWidth: true; elide: Text.ElideMiddle }
        Basic.Label { text: qsTr("History"); color: Theme.text; font.weight: Font.DemiBold }
        RowLayout {
            UiButton { text: qsTr("Choose history file…"); enabled: !page.controller.busy; onClicked: historyFile.open() }
            UiButton { text: qsTr("Clear"); enabled: !page.controller.busy && page.controller.historyFileLabel.length > 0; onClicked: page.controller.setHistoryFile("") }
        }
        Basic.Label { text: page.controller.historyFileLabel || qsTr("No history file selected"); textFormat: Text.PlainText; color: Theme.muted; Layout.fillWidth: true; elide: Text.ElideMiddle }
        Basic.Label {
            text: qsTr("Password CSV exports from Chrome, Edge, Brave, Firefox, Safari and Apple Passwords are supported. For Safari, extract its browsing-data ZIP and choose Passwords.csv and History.json. History can also come from a browser History / places.sqlite database or Chrome Takeout BrowserHistory.json.")
            color: Theme.muted
            Layout.fillWidth: true
            wrapMode: Text.Wrap
        }
        Basic.Label {
            text: qsTr("Password CSV files are unencrypted. Delete the export when you are finished.")
            color: Theme.muted
            Layout.fillWidth: true
            wrapMode: Text.Wrap
        }
        RowLayout {
            UiButton { objectName: "previewImportButton"; text: qsTr("Preview import"); enabled: !page.controller.busy; onClicked: page.controller.preview() }
            UiButton { objectName: "confirmImportButton"; text: qsTr("Import selected data"); highlighted: true; enabled: page.controller.ready && !page.controller.busy; onClicked: page.controller.apply() }
        }
    }
    Basic.Label {
        text: qsTr("Existing Arbiter passwords are kept, and duplicate visits are skipped. HTTPS logins are supported; passkeys, one-time codes and password notes are not imported.")
        color: Theme.muted
        Layout.fillWidth: true
        wrapMode: Text.Wrap
    }
    Basic.Label {
        visible: page.controller.ready
        text: qsTr("Ready: %1 new passwords and %2 history visits (up to 10,000). Skipped: %3 duplicate or unsupported password rows and %4 history rows. History duplicates already in Arbiter are skipped when importing.")
            .arg(page.controller.passwordCount).arg(page.controller.historyCount).arg(page.controller.skippedPasswords).arg(page.controller.skippedHistory)
        color: Theme.text
        textFormat: Text.PlainText
        Layout.fillWidth: true
        wrapMode: Text.Wrap
    }
    Basic.Label {
        visible: page.controller.busy
        text: page.controller.importing ? qsTr("Saving passwords: %1 / %2…").arg(page.controller.processed).arg(page.controller.passwordCount) : qsTr("Reading browser data… Approve the system unlock prompt if one appears.")
        color: Theme.muted
        Layout.fillWidth: true
        wrapMode: Text.Wrap
    }
    UiButton { text: page.controller.importing ? qsTr("Stop import") : qsTr("Cancel"); visible: page.controller.busy || page.controller.ready; onClicked: page.controller.cancel() }
    Basic.Label { text: page.controller.error; textFormat: Text.PlainText; visible: text.length > 0; color: Theme.text; Layout.fillWidth: true; wrapMode: Text.Wrap }
    Basic.Label { text: page.controller.message; textFormat: Text.PlainText; visible: text.length > 0; color: Theme.text; Layout.fillWidth: true; wrapMode: Text.Wrap }
    Dialogs.FileDialog {
        id: passwordFile
        title: qsTr("Choose browser password export")
        nameFilters: [qsTr("Password exports (*.csv)"), qsTr("All files (*)")]
        onAccepted: page.controller.setPasswordFile(selectedFile)
    }
    Dialogs.FileDialog {
        id: historyFile
        title: qsTr("Choose browser history")
        nameFilters: [qsTr("All history files (*)"), qsTr("JSON exports (*.json)"), qsTr("History databases (*.sqlite *.db)")]
        onAccepted: page.controller.setHistoryFile(selectedFile)
    }
}
