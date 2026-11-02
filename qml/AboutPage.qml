pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic as Basic
import Arbiter
import "qrc:/about/AboutContent.js" as AboutContent

ColumnLayout {
    id: page
    objectName: "aboutPage"
    readonly property string projectUrl: "https://github.com/pouya-eghbali/arbiter"
    required property var updateTracker
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
        text: AboutContent.intro()
        font.pixelSize: 18
        font.weight: Font.Medium
        color: Theme.text
        wrapMode: Text.Wrap
    }
    Basic.Label {
        Layout.fillWidth: true
        text: AboutContent.contribute()
        color: Theme.muted
        wrapMode: Text.Wrap
    }
    Basic.Label {
        Layout.fillWidth: true
        text: AboutContent.licenseNotice()
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
            text: qsTr("Report a bug")
            iconName: "external-link"
            onClicked: Qt.openUrlExternally(page.projectUrl + "/issues/new?template=bug_report.yml")
        }
        UiButton {
            objectName: "aboutFeatureRequestLink"
            text: qsTr("Request a feature")
            iconName: "external-link"
            onClicked: Qt.openUrlExternally(page.projectUrl + "/issues/new?template=feature_request.yml")
        }
        UiButton {
            objectName: "aboutCompatibilityLink"
            text: qsTr("Report a compatibility issue")
            iconName: "external-link"
            onClicked: Qt.openUrlExternally(page.projectUrl + "/issues/new?template=compatibility_report.yml")
        }
        UiButton {
            text: qsTr("GPL v3 license")
            iconName: "external-link"
            onClicked: Qt.openUrlExternally(page.projectUrl + "/blob/master/LICENSE")
        }
    }
    Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.border }
    ColumnLayout {
        Layout.fillWidth: true
        spacing: 10
        Basic.Label { text: qsTr("Updates"); font.pixelSize: 20; font.weight: Font.DemiBold; color: Theme.text }
        RowLayout {
            Layout.fillWidth: true
            spacing: 10
            Basic.BusyIndicator {
                visible: page.updateTracker.state === "checking"
                running: visible
                implicitWidth: 24; implicitHeight: 24
            }
            Basic.Label {
                Layout.fillWidth: true
                text: {
                    switch (page.updateTracker.state) {
                    case "checking": return qsTr("Checking for updates…")
                    case "available": return qsTr("Version %1 is available.").arg(page.updateTracker.latestVersion)
                    case "current": return qsTr("You're up to date.")
                    case "ahead": return qsTr("You're using a newer build.")
                    case "error": return page.updateTracker.error
                    default: return qsTr("Check for new releases.")
                    }
                }
                textFormat: Text.PlainText
                color: page.updateTracker.state === "available" ? Theme.accent : Theme.text
                wrapMode: Text.Wrap
            }
        }
        Basic.Label {
            visible: !isNaN(page.updateTracker.lastChecked.getTime())
            text: visible ? qsTr("Last checked: %1").arg(Qt.formatDateTime(page.updateTracker.lastChecked, Qt.locale().dateTimeFormat(Locale.ShortFormat))) : ""
            color: Theme.muted
            Layout.fillWidth: true
            wrapMode: Text.Wrap
        }
        Flow {
            Layout.fillWidth: true
            spacing: 8
            UiButton {
                objectName: "checkForUpdatesButton"
                text: qsTr("Check for updates")
                iconName: "rotate-cw"
                enabled: page.updateTracker.state !== "checking"
                onClicked: page.updateTracker.check()
            }
            UiButton {
                objectName: "updateReleaseButton"
                visible: page.updateTracker.latestVersion.length > 0
                text: qsTr("View release")
                iconName: "external-link"
                onClicked: Qt.openUrlExternally(page.updateTracker.releaseUrl)
            }
        }
        UiCheckBox {
            objectName: "automaticUpdatesCheckBox"
            Layout.fillWidth: true
            text: qsTr("Automatically check for updates")
            checked: page.updateTracker.automatic
            onToggled: page.updateTracker.automatic = checked
        }
        Basic.Label {
            Layout.fillWidth: true
            text: qsTr("Checks GitHub once a day. No browsing data is sent.")
            color: Theme.muted
            wrapMode: Text.Wrap
        }
    }
    Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.border }
    ColumnLayout {
        Layout.fillWidth: true
        spacing: 8
        Basic.Label { text: AboutContent.creditsTitle(); font.pixelSize: 20; font.weight: Font.DemiBold; color: Theme.text }
        Basic.Label {
            Layout.fillWidth: true
            text: AboutContent.creditsThanks()
            wrapMode: Text.Wrap
            color: Theme.muted
        }
    }
    Repeater {
        model: AboutContent.credits()
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
    Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.border }
    ColumnLayout {
        Layout.fillWidth: true
        spacing: 8
        Basic.Label { text: qsTr("Widevine & DRM"); font.pixelSize: 20; font.weight: Font.DemiBold; color: Theme.text }
        Basic.Label {
            Layout.fillWidth: true
            text: AboutContent.widevineNotice()
            wrapMode: Text.Wrap
            color: Theme.muted
        }
    }
}
