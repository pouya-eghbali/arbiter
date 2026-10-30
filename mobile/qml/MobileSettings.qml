pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import Arbiter

ColumnLayout {
    id: root
    spacing: 18
    Text {
        textFormat: Text.PlainText
        text: qsTr("Appearance")
        font.pixelSize: 22
        font.weight: Font.DemiBold
        color: Theme.text
    }
    UiComboBox {
        Layout.fillWidth: true
        model: [qsTr("System"), qsTr("Light"), qsTr("Dark")]
        currentIndex: ["system", "light", "dark"].indexOf(browser.settings.theme)
        onActivated: index => browser.setSetting("theme", ["system", "light", "dark"][index])
    }
    MobileColors {
        Layout.fillWidth: true
    }
    Text {
        textFormat: Text.PlainText
        text: qsTr("Language")
        font.pixelSize: 22
        font.weight: Font.DemiBold
        color: Theme.text
    }
    MobileLanguagePicker {
        Layout.fillWidth: true
    }
    Text {
        textFormat: Text.PlainText
        text: qsTr("Search engine")
        font.pixelSize: 22
        font.weight: Font.DemiBold
        color: Theme.text
    }
    UiComboBox {
        Layout.fillWidth: true
        model: ["DuckDuckGo", "Google", "Bing", "Brave"]
        currentIndex: model.indexOf(browser.settings.searchEngine)
        onActivated: index => browser.setSetting("searchEngine", model[index])
    }
    Text {
        textFormat: Text.PlainText
        text: qsTr("Privacy & passwords")
        font.pixelSize: 22
        font.weight: Font.DemiBold
        color: Theme.text
    }
    UiCheckBox {
        text: qsTr("Block ads and trackers")
        checked: browser.settings.adblockEnabled
        onToggled: browser.setSetting("adblockEnabled", checked)
    }
    UiCheckBox {
        text: qsTr("Offer to save passwords")
        checked: browser.settings.savePasswords
        onToggled: browser.setSetting("savePasswords", checked)
    }
    UiCheckBox {
        text: qsTr("Show password suggestions")
        checked: browser.settings.suggestPasswords
        onToggled: browser.setSetting("suggestPasswords", checked)
    }
    Text {
        textFormat: Text.PlainText
        text: qsTr("Ask to unlock passwords")
        color: Theme.muted
    }
    UiComboBox {
        Layout.fillWidth: true
        model: [qsTr("Every time"), qsTr("After 1 minute"), qsTr("After 5 minutes"), qsTr("After 15 minutes")]
        currentIndex: Math.max(0, [0, 60, 300, 900].indexOf(browser.settings.passwordUnlockSeconds))
        onActivated: index => browser.setSetting("passwordUnlockSeconds", [0, 60, 300, 900][index])
    }
    Text {
        textFormat: Text.PlainText
        text: qsTr("Lock private browsing after inactivity")
        color: Theme.muted
        wrapMode: Text.WordWrap
        Layout.fillWidth: true
    }
    UiComboBox {
        Layout.fillWidth: true
        model: [qsTr("Never"), qsTr("1 minute"), qsTr("5 minutes"), qsTr("15 minutes")]
        currentIndex: Math.max(0, [0, 60, 300, 900].indexOf(browser.settings.privateLockSeconds))
        onActivated: index => browser.setSetting("privateLockSeconds", [0, 60, 300, 900][index])
    }
    Text {
        textFormat: Text.PlainText
        text: qsTr("Private browsing also locks when the app goes into the background. Use your device biometrics or passcode to unlock.")
        color: Theme.muted
        wrapMode: Text.WordWrap
        Layout.fillWidth: true
    }
    Text {
        textFormat: Text.PlainText
        text: qsTr("Tab sleeping")
        font.pixelSize: 22
        font.weight: Font.DemiBold
        color: Theme.text
    }
    UiComboBox {
        Layout.fillWidth: true
        model: [qsTr("Never"), qsTr("5 minutes"), qsTr("10 minutes"), qsTr("30 minutes")]
        currentIndex: Math.max(0, [0, 300, 600, 1800].indexOf(browser.settings.tabSleepSeconds))
        onActivated: index => browser.setSetting("tabSleepSeconds", [0, 300, 600, 1800][index])
    }
    Text {
        textFormat: Text.PlainText
        text: qsTr("Audio, loading pages and edited forms stay awake.")
        color: Theme.muted
        wrapMode: Text.WordWrap
        Layout.fillWidth: true
    }
    Text {
        textFormat: Text.PlainText
        text: qsTr("Local encrypted sync")
        font.pixelSize: 22
        font.weight: Font.DemiBold
        color: Theme.text
    }
    UiCheckBox {
        text: qsTr("Allow background sync")
        checked: browser.settings.backgroundSync
        onToggled: browser.setSetting("backgroundSync", checked)
    }
    Text {
        textFormat: Text.PlainText
        text: qsTr("Direct encrypted sync over the same Wi-Fi. Keep Arbiter open on your desktop and its sync unlocked.")
        color: Theme.muted
        wrapMode: Text.WordWrap
        Layout.fillWidth: true
    }
    UiButton {
        text: qsTr("Set as default browser")
        onClicked: browser.setDefaultBrowser()
    }
    UiButton {
        text: qsTr("Enable credential provider")
        onClicked: browser.enableCredentialProvider()
    }
    Text {
        textFormat: Text.PlainText
        text: qsTr("About Arbiter")
        font.pixelSize: 22
        font.weight: Font.DemiBold
        color: Theme.text
    }
    Text {
        textFormat: Text.PlainText
        text: qsTr("An open-source browser for power users. Zero telemetry or data collection. Everyone is welcome to contribute.")
        color: Theme.muted
        wrapMode: Text.WordWrap
        Layout.fillWidth: true
    }
    Text {
        textFormat: Text.PlainText
        text: "GNU GPL v3 · Qt/QML · Qt WebEngine / native WebView · Brave adblock-rust · Lucide"
        color: Theme.muted
        wrapMode: Text.WordWrap
        Layout.fillWidth: true
    }
    UiButton {
        text: "GitHub"
        iconName: "external-link"
        onClicked: Qt.openUrlExternally("https://github.com/pouya-eghbali/arbiter")
    }
    UiButton {
        text: qsTr("Report a bug")
        iconName: "bug"
        onClicked: Qt.openUrlExternally("https://github.com/pouya-eghbali/arbiter/issues/new?template=bug_report.yml")
    }
}
