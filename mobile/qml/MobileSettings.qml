pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic as Controls
import QtQuick.Layouts
import Arbiter

ColumnLayout {
    id: root
    property string category: ""
    readonly property string pageTitle: categories.find(c => c.id === category)?.title || qsTr("Settings")
    readonly property var categories: [
        {
            id: "appearance",
            icon: "sparkles",
            title: qsTr("Appearance")
        },
        {
            id: "language",
            icon: "globe",
            title: qsTr("Language")
        },
        {
            id: "search",
            icon: "search",
            title: qsTr("Search engine")
        },
        {
            id: "privacy",
            icon: "shield-check",
            title: qsTr("Privacy & passwords")
        },
        {
            id: "sleep",
            icon: "moon",
            title: qsTr("Tab sleeping")
        },
        {
            id: "sync",
            icon: "refresh-cw",
            title: qsTr("Local encrypted sync")
        },
        {
            id: "about",
            icon: "globe",
            title: qsTr("About Arbiter")
        }
    ]
    spacing: 24
    Repeater {
        model: root.category === "" ? root.categories : []
        delegate: Controls.AbstractButton {
            id: categoryButton
            required property var modelData
            Layout.fillWidth: true
            implicitHeight: 60
            padding: 16
            Accessible.name: modelData.title
            onClicked: root.category = modelData.id
            background: Rectangle {
                radius: 16
                color: categoryButton.down ? Theme.hover : Theme.field
            }
            contentItem: RowLayout {
                spacing: 14
                UiIcon {
                    name: categoryButton.modelData.icon
                    color: Theme.accent
                    Layout.preferredWidth: 22
                    Layout.preferredHeight: 22
                }
                Text {
                    text: categoryButton.modelData.title
                    Layout.fillWidth: true
                    font.pixelSize: 15
                    color: Theme.text
                    elide: Text.ElideRight
                }
                UiIcon {
                    name: "chevron-right"
                    color: Theme.muted
                    Layout.preferredWidth: 18
                    Layout.preferredHeight: 18
                }
            }
        }
    }
    MobileSection {
        visible: root.category === "appearance"
        Layout.fillWidth: true
        title: qsTr("Appearance")
        MobileComboBox {
            Layout.fillWidth: true
            model: [qsTr("System"), qsTr("Light"), qsTr("Dark")]
            currentIndex: ["system", "light", "dark"].indexOf(browser.settings.theme)
            onActivated: index => browser.setSetting("theme", ["system", "light", "dark"][index])
        }
    }
    MobileColors {
        visible: root.category === "appearance"
        Layout.fillWidth: true
    }
    MobileSection {
        visible: root.category === "language"
        Layout.fillWidth: true
        title: qsTr("Language")
        MobileLanguagePicker {
            Layout.fillWidth: true
        }
    }
    MobileSection {
        visible: root.category === "search"
        Layout.fillWidth: true
        title: qsTr("Search engine")
        MobileComboBox {
            Layout.fillWidth: true
            model: ["DuckDuckGo", "Google", "Bing", "Brave"]
            currentIndex: model.indexOf(browser.settings.searchEngine)
            onActivated: index => browser.setSetting("searchEngine", model[index])
        }
    }
    MobileSection {
        visible: root.category === "privacy"
        Layout.fillWidth: true
        title: qsTr("Privacy & passwords")
        MobileToggle {
            Layout.fillWidth: true
            text: qsTr("Block ads and trackers")
            checked: browser.settings.adblockEnabled
            onToggled: browser.setSetting("adblockEnabled", checked)
        }
        MobileToggle {
            Layout.fillWidth: true
            text: qsTr("Offer to save passwords")
            checked: browser.settings.savePasswords
            onToggled: browser.setSetting("savePasswords", checked)
        }
        MobileToggle {
            Layout.fillWidth: true
            text: qsTr("Show password suggestions")
            checked: browser.settings.suggestPasswords
            onToggled: browser.setSetting("suggestPasswords", checked)
        }
    }
    MobileSection {
        visible: root.category === "privacy"
        Layout.fillWidth: true
        title: qsTr("Ask to unlock passwords")
        MobileComboBox {
            Layout.fillWidth: true
            model: [qsTr("Every time"), qsTr("After 1 minute"), qsTr("After 5 minutes"), qsTr("After 15 minutes")]
            currentIndex: Math.max(0, [0, 60, 300, 900].indexOf(browser.settings.passwordUnlockSeconds))
            onActivated: index => browser.setSetting("passwordUnlockSeconds", [0, 60, 300, 900][index])
        }
    }
    MobileSection {
        visible: root.category === "privacy"
        Layout.fillWidth: true
        title: qsTr("Lock private browsing after inactivity")
        description: qsTr("Private browsing also locks when the app goes into the background. Use your device biometrics or passcode to unlock.")
        MobileComboBox {
            Layout.fillWidth: true
            model: [qsTr("Never"), qsTr("1 minute"), qsTr("5 minutes"), qsTr("15 minutes")]
            currentIndex: Math.max(0, [0, 60, 300, 900].indexOf(browser.settings.privateLockSeconds))
            onActivated: index => browser.setSetting("privateLockSeconds", [0, 60, 300, 900][index])
        }
    }
    MobileSection {
        visible: root.category === "privacy"
        Layout.fillWidth: true
        MobileButton {
            Layout.fillWidth: true
            text: qsTr("Enable credential provider")
            onClicked: browser.enableCredentialProvider()
        }
    }
    MobileSection {
        visible: root.category === "sleep"
        Layout.fillWidth: true
        title: qsTr("Tab sleeping")
        description: qsTr("Audio, loading pages and edited forms stay awake.")
        MobileComboBox {
            Layout.fillWidth: true
            model: [qsTr("Never"), qsTr("5 minutes"), qsTr("10 minutes"), qsTr("30 minutes")]
            currentIndex: Math.max(0, [0, 300, 600, 1800].indexOf(browser.settings.tabSleepSeconds))
            onActivated: index => browser.setSetting("tabSleepSeconds", [0, 300, 600, 1800][index])
        }
    }
    MobileSection {
        visible: root.category === "sync"
        Layout.fillWidth: true
        title: qsTr("Local encrypted sync")
        description: qsTr("Direct encrypted sync over the same Wi-Fi. Keep Arbiter open on your desktop and its sync unlocked.")
        MobileToggle {
            Layout.fillWidth: true
            text: qsTr("Allow background sync")
            checked: browser.settings.backgroundSync
            onToggled: browser.setSetting("backgroundSync", checked)
        }
        MobileButton {
            Layout.fillWidth: true
            text: browser.vault.peerName ? qsTr("Sync now") : qsTr("Pair with your desktop")
            enabled: !browser.vault.busy
            onClicked: browser.vault.peerName ? browser.vault.sync() : root.pairRequested()
        }
    }
    signal pairRequested
    ColumnLayout {
        visible: root.category === "about"
        Layout.fillWidth: true
        spacing: 20
        MobileLogo {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: 64
            Layout.preferredHeight: 64
        }
        Text {
            text: "Arbiter"
            color: Theme.text
            font.pixelSize: 26
            font.weight: Font.DemiBold
            Layout.alignment: Qt.AlignHCenter
        }
        Text {
            text: qsTr("An open-source browser for power users. Zero telemetry or data collection. Everyone is welcome to contribute.")
            color: Theme.muted
            font.pixelSize: 15
            lineHeight: 1.25
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }
        Text {
            text: "GNU GPL v3 · Qt/QML · Qt WebEngine / native WebView · Brave adblock-rust · Lucide"
            color: Theme.muted
            font.pixelSize: 13
            lineHeight: 1.2
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }
        MobileButton {
            Layout.fillWidth: true
            text: "GitHub"
            iconName: "external-link"
            onClicked: Qt.openUrlExternally("https://github.com/pouya-eghbali/arbiter")
        }
        MobileButton {
            Layout.fillWidth: true
            text: qsTr("Report a bug")
            iconName: "bug"
            onClicked: Qt.openUrlExternally("https://github.com/pouya-eghbali/arbiter/issues/new?template=bug_report.yml")
        }
    }
    MobileButton {
        visible: root.category === ""
        Layout.fillWidth: true
        text: qsTr("Set as default browser")
        onClicked: browser.setDefaultBrowser()
    }
}
