pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic as Basic
import Arbiter

ColumnLayout {
    id: page
    objectName: "aboutPage"
    readonly property string projectUrl: "https://github.com/pouya-eghbali/arbiter"
    spacing: 24

    RowLayout {
        Layout.fillWidth: true
        spacing: 16
        Rectangle {
            Layout.preferredWidth: 64
            Layout.preferredHeight: 64
            radius: 18
            color: "#11161e"
            Image {
                anchors.fill: parent
                anchors.margins: 7
                source: "qrc:/brand/valkyrie-source.png"
                sourceSize: Qt.size(160, 160)
                fillMode: Image.PreserveAspectFit
                mipmap: true
                Accessible.ignored: true
            }
        }
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 4
            Basic.Label { text: qsTr("Arbiter"); font.pixelSize: 30; font.weight: Font.DemiBold; color: Theme.text }
            Basic.Label { text: qsTr("Version %1").arg(Application.version); color: Theme.muted }
        }
    }
    Basic.Label {
        Layout.fillWidth: true
        text: qsTr("An open-source browser for power users. Zero telemetry. Zero data collection.")
        font.pixelSize: 18
        font.weight: Font.Medium
        color: Theme.text
        wrapMode: Text.Wrap
    }
    Basic.Label {
        Layout.fillWidth: true
        text: qsTr("Built for people who like to make their browser their own. Everyone is welcome to contribute: share an idea, report a bug, improve the docs, or help build what comes next.")
        color: Theme.muted
        wrapMode: Text.Wrap
    }
    Flow {
        Layout.fillWidth: true
        spacing: 8
        UiButton {
            objectName: "aboutProjectLink"
            text: qsTr("GitHub & contribute")
            iconName: "external-link"
            onClicked: Qt.openUrlExternally(page.projectUrl)
        }
        UiButton {
            objectName: "aboutIssuesLink"
            text: qsTr("Report an issue")
            iconName: "external-link"
            onClicked: Qt.openUrlExternally(page.projectUrl + "/issues")
        }
    }
    Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.border }
    ColumnLayout {
        Layout.fillWidth: true
        spacing: 8
        Basic.Label { text: qsTr("Made possible by"); font.pixelSize: 20; font.weight: Font.DemiBold; color: Theme.text }
        Basic.Label {
            Layout.fillWidth: true
            text: qsTr("Thank you to the contributors and open-source projects behind Arbiter.")
            wrapMode: Text.Wrap
            color: Theme.muted
        }
    }
    Repeater {
        model: [
            { name: "Qt & Qt WebEngine", detail: qsTr("Application framework, interface, and Chromium-based web engine"), license: qsTr("Qt licensing"), url: "https://www.qt.io/licensing/" },
            { name: "Chromium", detail: qsTr("Web platform and browser engine"), license: qsTr("BSD-style & third-party licenses"), url: "https://chromium.googlesource.com/chromium/src/+/main/LICENSE" },
            { name: "QtKeychain", detail: qsTr("Secure storage through the operating system"), license: "BSD-3-Clause", url: "https://github.com/frankosterfeld/qtkeychain" },
            { name: "OpenSSL", detail: qsTr("Cryptography for passwords and passkeys"), license: "Apache-2.0", url: "https://www.openssl.org/" },
            { name: "adblock-rust · Brave", detail: qsTr("Ad and tracker blocking engine"), license: "MPL-2.0", url: "https://github.com/brave/adblock-rust" },
            { name: "EasyList & EasyPrivacy", detail: qsTr("Community-maintained ad and tracker filter lists"), license: "CC BY-SA 3.0", url: "https://easylist.to/" },
            { name: "Lucide", detail: qsTr("Interface icons by Lucide and its contributors"), license: "ISC", url: "https://lucide.dev/" },
            { name: "toml++ · Mark Gillard", detail: qsTr("TOML configuration parsing"), license: "MIT", url: "https://github.com/marzer/tomlplusplus" },
            { name: "QR Code generator · Project Nayuki", detail: qsTr("QR codes for signing in with another device"), license: "MIT", url: "https://www.nayuki.io/page/qr-code-generator-library" },
            { name: "SQLite", detail: qsTr("Local history and browser data storage through Qt SQL"), license: qsTr("Public domain"), url: "https://sqlite.org/" },
            { name: "libsecret", detail: qsTr("Access to Linux secret storage"), license: "LGPL-2.1-or-later", url: "https://gitlab.gnome.org/GNOME/libsecret" }
        ]
        delegate: ColumnLayout {
            id: credit
            required property var modelData
            Layout.fillWidth: true
            spacing: 4
            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                Basic.Label {
                    Layout.fillWidth: true
                    text: credit.modelData.name
                    textFormat: Text.PlainText
                    font.weight: Font.DemiBold
                    color: Theme.text
                    wrapMode: Text.Wrap
                }
                UiIconButton {
                    iconName: "external-link"
                    label: qsTr("Visit %1").arg(credit.modelData.name)
                    onClicked: Qt.openUrlExternally(credit.modelData.url)
                }
            }
            Basic.Label { Layout.fillWidth: true; text: credit.modelData.detail; wrapMode: Text.Wrap; color: Theme.muted }
            Basic.Label { Layout.fillWidth: true; text: credit.modelData.license; wrapMode: Text.Wrap; color: Theme.muted; font.pixelSize: 12 }
        }
    }
    UiButton {
        text: qsTr("Third-party license notices")
        iconName: "external-link"
        onClicked: Qt.openUrlExternally(page.projectUrl + "/tree/master/licenses")
    }
}
