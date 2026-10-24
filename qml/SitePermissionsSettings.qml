pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic as Basic

ColumnLayout {
    id: section
    required property var settings
    property var workspaceChoices: []
    function sessionLabel(profile: string): string {
        if (profile === "Default") return qsTr("Shared session")
        const names = workspaceChoices.filter(space => space.profile === profile).map(space => space.name)
        return names.length ? names.join(", ") : qsTr("Browser session")
    }
    spacing: 14
    onVisibleChanged: if (visible) settings.refresh()
    Basic.Label { text: qsTr("Site permissions"); font.pixelSize: 20; font.weight: Font.DemiBold; color: Theme.text; Layout.fillWidth: true; wrapMode: Text.Wrap }
    UiCheckBox {
        objectName: "geolocationEnabled"
        Layout.fillWidth: true
        leftPadding: 0
        text: qsTr("Allow sites to request your location")
        checked: section.settings.geolocationEnabled
        onToggled: section.settings.geolocationEnabled = checked
    }
    Basic.Label {
        Layout.fillWidth: true
        text: qsTr("Sites still need your approval. Turning this off stops pending requests and removes saved location grants. Your operating system must also allow location services.")
        wrapMode: Text.Wrap
        color: Theme.muted
    }
    Basic.Label {
        Layout.fillWidth: true
        text: qsTr("Saved decisions for browser profiles opened this session. Private-session decisions are kept separate and disappear when that session closes. Camera, microphone, screen sharing, and pointer lock always ask again.")
        wrapMode: Text.Wrap
        color: Theme.muted
    }
    Repeater {
        model: section.settings.savedPermissions
        delegate: Rectangle {
            id: decision
            required property var modelData
            Layout.fillWidth: true
            implicitHeight: decisionRow.implicitHeight + 24
            radius: 14
            color: Theme.field
            RowLayout {
                id: decisionRow
                anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter; margins: 12 }
                spacing: 12
                ColumnLayout {
                    Layout.fillWidth: true
                    Basic.Label { Layout.fillWidth: true; text: decision.modelData.origin; textFormat: Text.PlainText; wrapMode: Text.WrapAnywhere; color: Theme.text }
                    Basic.Label {
                        Layout.fillWidth: true
                        text: qsTr("%1 · %2 · %3").arg(decision.modelData.label).arg(decision.modelData.allowed ? qsTr("Allowed") : qsTr("Blocked")).arg(section.sessionLabel(decision.modelData.profile))
                        textFormat: Text.PlainText
                        wrapMode: Text.Wrap
                        color: Theme.muted
                    }
                }
                UiButton {
                    Layout.alignment: Qt.AlignVCenter
                    text: qsTr("Reset")
                    onClicked: section.settings.resetPermission(decision.modelData.profile, decision.modelData.origin, decision.modelData.type)
                }
            }
        }
    }
    Basic.Label { visible: section.settings.savedPermissions.length === 0; text: qsTr("No saved site permissions."); color: Theme.muted; Layout.fillWidth: true; wrapMode: Text.Wrap }
}
