pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic as Controls
import QtQuick.Layouts
import Arbiter

ColumnLayout {
    id: root
    property string category: ""
    property string query: ""
    readonly property string pageTitle: categories.find(c => c.id === category)?.title || qsTr("Settings")
    readonly property var categories: [
        { id: "general", icon: "settings", title: qsTr("General"), detail: qsTr("Search, language, default browser and tab sleeping") },
        { id: "appearance", icon: "sparkles", title: qsTr("Appearance"), detail: qsTr("Theme, Liquid Glass, colors and backgrounds") },
        { id: "privacy", icon: "shield-check", title: qsTr("Privacy"), detail: qsTr("Ad blocking, private browsing and permissions") },
        { id: "passwords", icon: "key-round", title: qsTr("Passwords & passkeys"), detail: qsTr("Saved credentials, autofill and unlocking") },
        { id: "sync", icon: "refresh-cw", title: qsTr("Sync"), detail: qsTr("Desktop pairing and local encrypted sync") },
        { id: "about", icon: "globe", title: qsTr("About Arbiter"), detail: qsTr("Version, license and contributions") }
    ]
    readonly property var filteredCategories: categories.filter(c => !query.trim() || (c.title + " " + c.detail).toLocaleLowerCase().includes(query.trim().toLocaleLowerCase()))
    signal pairRequested
    signal vaultRequested
    signal historyRequested
    signal downloadsRequested
    spacing: 22
    component Caption: Text {
        Layout.fillWidth: true
        color: Theme.mutedFor(parent)
        font.pixelSize: 13
        textFormat: Text.PlainText
        wrapMode: Text.WordWrap
        lineHeight: 1.2
    }
    MobileField {
        visible: root.category === ""
        Layout.fillWidth: true
        objectName: "mobileSettingsSearch"
        placeholderText: qsTr("Search settings")
        Accessible.name: placeholderText
        text: root.query
        onTextEdited: root.query = text
    }
    ColumnLayout {
        visible: root.category === ""
        Layout.fillWidth: true
        spacing: 8
        Repeater {
            model: root.category === "" ? root.filteredCategories : []
            delegate: MobileSettingsRow {
                required property var modelData
                objectName: "settingsCategory-" + modelData.id
                Layout.fillWidth: true
                text: modelData.title
                detail: modelData.detail
                iconName: modelData.icon
                onClicked: { Qt.inputMethod.hide(); root.category = modelData.id }
            }
        }
        Caption { visible: root.filteredCategories.length === 0; text: qsTr("No matching settings") }
    }

    MobileSection {
        visible: root.category === "general"
        Layout.fillWidth: true
        title: qsTr("Search engine")
        MobileComboBox {
            objectName: "mobileSearchEngine"
            Layout.fillWidth: true
            Accessible.name: qsTr("Default search engine")
            model: ["DuckDuckGo", "Google", "Bing", "Brave", "Ecosia", "Startpage"]
            currentIndex: model.indexOf(browser.settings.searchEngine)
            onActivated: index => browser.setSetting("searchEngine", model[index])
        }
    }
    MobileSection {
        visible: root.category === "general"
        Layout.fillWidth: true
        title: qsTr("Language")
        MobileLanguagePicker { Layout.fillWidth: true; Accessible.name: qsTr("Language") }
    }
    MobileSection {
        visible: root.category === "general"
        Layout.fillWidth: true
        title: qsTr("Default browser")
        MobileButton { Layout.fillWidth: true; text: qsTr("Set as default browser"); onClicked: browser.setDefaultBrowser() }
    }
    MobileSection {
        visible: root.category === "general"
        Layout.fillWidth: true
        title: qsTr("Tab sleeping")
        description: qsTr("Audio, loading pages and edited forms stay awake.")
        MobileComboBox {
            objectName: "mobileTabSleepTimeout"
            Layout.fillWidth: true
            Accessible.name: qsTr("Tab sleeping")
            readonly property var durations: [0, 60, 300, 600, 900, 1800, 3600]
            model: [qsTr("Never"), qsTr("1 minute"), qsTr("5 minutes"), qsTr("10 minutes"), qsTr("15 minutes"), qsTr("30 minutes"), qsTr("1 hour")]
            currentIndex: durations.indexOf(browser.settings.tabSleepSeconds)
            displayText: currentIndex < 0 ? qsTr("%1 seconds").arg(browser.settings.tabSleepSeconds) : currentText
            onActivated: index => browser.setSetting("tabSleepSeconds", durations[index])
        }
    }
    MobileSettingsRow {
        visible: root.category === "general"
        Layout.fillWidth: true
        text: qsTr("Downloads")
        iconName: "download"
        onClicked: root.downloadsRequested()
    }

    MobileSection {
        visible: root.category === "appearance"
        Layout.fillWidth: true
        title: qsTr("Color theme")
        MobileComboBox {
            Layout.fillWidth: true
            Accessible.name: qsTr("Color theme")
            model: [qsTr("System"), qsTr("Light"), qsTr("Dark")]
            currentIndex: ["system", "light", "dark"].indexOf(browser.settings.theme)
            onActivated: index => browser.setSetting("theme", ["system", "light", "dark"][index])
        }
    }
    NativeGlassHost { id: glassCapability; visible: false }
    MobileSection {
        visible: root.category === "appearance" && glassCapability.available
        Layout.fillWidth: true
        description: qsTr("Liquid Glass uses the system appearance. Custom colors apply to Glacier when Liquid Glass is off.")
        MobileToggle { Layout.fillWidth: true; text: qsTr("Liquid Glass (experimental)"); checked: browser.settings.liquidGlass === true; onToggled: browser.setSetting("liquidGlass", checked) }
    }
    MobileColors { visible: root.category === "appearance"; Layout.fillWidth: true }

    MobileSection {
        visible: root.category === "privacy"
        Layout.fillWidth: true
        title: qsTr("Ad blocking")
        MobileToggle { Layout.fillWidth: true; text: qsTr("Block ads and trackers"); checked: browser.settings.adblockEnabled; onToggled: browser.setSetting("adblockEnabled", checked) }
        Caption { text: qsTr("Pause blocking for a website from the browser menu. Restore blocking for individual websites below.") }
        Repeater {
            model: browser.settings.adblockDisabledHosts || []
            delegate: RowLayout {
                required property string modelData
                Layout.fillWidth: true
                Caption { text: parent.modelData }
                MobileIconButton {
                    iconName: "x"
                    label: qsTr("Resume ad blocking for %1").arg(parent.modelData)
                    onClicked: browser.setSetting("adblockDisabledHosts", browser.settings.adblockDisabledHosts.filter(host => host !== parent.modelData))
                }
            }
        }
        Caption { visible: !(browser.settings.adblockDisabledHosts || []).length; text: qsTr("No website exceptions") }
    }
    MobileSection {
        visible: root.category === "privacy"
        Layout.fillWidth: true
        title: qsTr("Lock private browsing after inactivity")
        description: qsTr("Private browsing also locks when the app goes into the background. Use your device biometrics or passcode to unlock.")
        MobileComboBox {
            Layout.fillWidth: true
            Accessible.name: qsTr("Lock private browsing after inactivity")
            readonly property var durations: [0, 60, 300, 900, 1800]
            model: [qsTr("Never"), qsTr("1 minute"), qsTr("5 minutes"), qsTr("15 minutes"), qsTr("30 minutes")]
            currentIndex: durations.indexOf(browser.settings.privateLockSeconds)
            displayText: currentIndex < 0 ? qsTr("%1 seconds").arg(browser.settings.privateLockSeconds) : currentText
            onActivated: index => browser.setSetting("privateLockSeconds", durations[index])
        }
    }
    MobileSettingsRow { visible: root.category === "privacy"; Layout.fillWidth: true; text: qsTr("History"); detail: qsTr("Review or delete browsing history"); iconName: "history"; onClicked: root.historyRequested() }
    MobileSection {
        visible: root.category === "privacy"
        Layout.fillWidth: true
        title: qsTr("Website permissions")
        description: qsTr("Camera, microphone and location permissions are managed by your device.")
        MobileButton { Layout.fillWidth: true; text: qsTr("Open system settings"); iconName: "external-link"; onClicked: browser.openSystemSettings() }
    }

    MobileSection {
        visible: root.category === "passwords"
        Layout.fillWidth: true
        title: qsTr("Autofill")
        MobileToggle { Layout.fillWidth: true; text: qsTr("Offer to save passwords"); checked: browser.settings.savePasswords; onToggled: browser.setSetting("savePasswords", checked) }
        MobileToggle { Layout.fillWidth: true; text: qsTr("Show password suggestions"); checked: browser.settings.suggestPasswords; onToggled: browser.setSetting("suggestPasswords", checked) }
    }
    MobileSection {
        visible: root.category === "passwords"
        Layout.fillWidth: true
        title: qsTr("Ask to unlock passwords")
        description: qsTr("Use your device biometrics or passcode to unlock saved credentials.")
        MobileComboBox {
            Layout.fillWidth: true
            Accessible.name: qsTr("Ask to unlock passwords")
            readonly property var durations: [0, 60, 300, 900, 1800, 3600]
            model: [qsTr("Every time"), qsTr("After 1 minute"), qsTr("After 5 minutes"), qsTr("After 15 minutes"), qsTr("30 minutes"), qsTr("1 hour")]
            currentIndex: durations.indexOf(browser.settings.passwordUnlockSeconds)
            displayText: currentIndex < 0 ? qsTr("%1 seconds").arg(browser.settings.passwordUnlockSeconds) : currentText
            onActivated: index => browser.setSetting("passwordUnlockSeconds", durations[index])
        }
    }
    MobileSettingsRow { objectName: "settingsVaultLink"; visible: root.category === "passwords"; Layout.fillWidth: true; text: qsTr("Passwords & passkeys"); detail: qsTr("Manage saved credentials and sync conflicts"); iconName: "key-round"; onClicked: root.vaultRequested() }
    MobileSection {
        visible: root.category === "passwords"
        Layout.fillWidth: true
        description: qsTr("Use Arbiter passwords and passkeys in other apps.")
        MobileButton { Layout.fillWidth: true; text: qsTr("Enable credential provider"); onClicked: browser.enableCredentialProvider() }
    }
    MobileButton { visible: root.category === "passwords" && browser.vault.unlocked; Layout.fillWidth: true; text: qsTr("Lock vault"); iconName: "lock"; onClicked: browser.vault.lock() }

    MobileSection {
        visible: root.category === "sync"
        Layout.fillWidth: true
        title: qsTr("Local encrypted sync")
        description: qsTr("Direct encrypted sync over the same Wi-Fi. Keep Arbiter open on your desktop and its sync unlocked.")
        Caption { text: browser.vault.peerName ? qsTr("Paired with %1").arg(browser.vault.peerName) : qsTr("No paired desktop") }
        MobileToggle { Layout.fillWidth: true; text: qsTr("Allow background sync"); checked: browser.settings.backgroundSync; onToggled: browser.setSetting("backgroundSync", checked) }
        MobileButton { Layout.fillWidth: true; text: browser.vault.peerName ? qsTr("Sync now") : qsTr("Pair with your desktop"); enabled: !browser.vault.busy; onClicked: browser.vault.peerName ? browser.vault.sync() : root.pairRequested() }
        Caption { visible: browser.vault.busy; text: qsTr("Syncing…") }
        Caption { visible: browser.vault.error.length > 0; text: browser.vault.error }
        MobileButton { Layout.fillWidth: true; visible: Boolean(browser.vault.peerName); enabled: !browser.vault.busy; text: qsTr("Disconnect desktop"); flat: true; onClicked: disconnect.open() }
    }
    MobileSettingsRow { visible: root.category === "sync" && browser.vault.conflicts > 0; Layout.fillWidth: true; text: qsTr("Resolve sync conflicts"); iconName: "refresh-cw"; onClicked: root.vaultRequested() }

    ColumnLayout {
        visible: root.category === "about"
        Layout.fillWidth: true
        spacing: 16
        MobileLogo { Layout.alignment: Qt.AlignHCenter; Layout.preferredWidth: 64; Layout.preferredHeight: 64 }
        Text { text: "Arbiter"; color: Theme.text; font.pixelSize: 26; font.weight: Font.DemiBold; Layout.alignment: Qt.AlignHCenter }
        Caption { text: qsTr("Version %1").arg(browser.version); horizontalAlignment: Text.AlignHCenter }
        Caption { text: qsTr("An open-source browser for power users. Zero telemetry or data collection. Everyone is welcome to contribute.") }
        Caption { text: "GNU GPL v3 · Qt/QML · Qt WebEngine / native WebView · Brave adblock-rust · Lucide" }
        MobileSettingsRow { Layout.fillWidth: true; text: "GitHub"; iconName: "external-link"; onClicked: Qt.openUrlExternally("https://github.com/pouya-eghbali/arbiter") }
        MobileSettingsRow { Layout.fillWidth: true; text: qsTr("Report a bug"); iconName: "bug"; onClicked: Qt.openUrlExternally("https://github.com/pouya-eghbali/arbiter/issues/new?template=bug_report.yml") }
        MobileSettingsRow { Layout.fillWidth: true; text: qsTr("Request a feature"); iconName: "sparkles"; onClicked: Qt.openUrlExternally("https://github.com/pouya-eghbali/arbiter/issues/new?template=feature_request.yml") }
        MobileSettingsRow { Layout.fillWidth: true; text: qsTr("License"); detail: "GNU GPL v3"; iconName: "code-xml"; onClicked: Qt.openUrlExternally("https://github.com/pouya-eghbali/arbiter/blob/master/LICENSE") }
    }
    UiPopup {
        id: disconnect
        parent: Controls.Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(380, parent.width - 32)
        height: contentItem.implicitHeight + topPadding + bottomPadding
        padding: 20
        modal: true
        focus: true
        closePolicy: Controls.Popup.CloseOnEscape
        contentItem: ColumnLayout {
            spacing: 16
            Caption { text: qsTr("Disconnect this desktop? Credentials already saved on this device are kept.") }
            MobileButton { Layout.fillWidth: true; text: qsTr("Disconnect desktop"); onClicked: { browser.vault.disconnectPeer(); disconnect.close() } }
            MobileButton { Layout.fillWidth: true; text: qsTr("Cancel"); flat: true; onClicked: disconnect.close() }
        }
    }
}
