// Isolated developer fixture: fabricated account metadata, no OS vault or network.
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic as Basic

Window {
    id: root
    width: 1080; height: 760; visible: true; color: Theme.window
    property bool showManager: false
    QtObject {
        id: mock
        property bool enabled: true
        property bool biometricEnabled: true
        property int verificationCooldown: 300
        property bool busy: false
        property bool pending: false
        property string error: ""
        property string message: ""
        property string pendingUsername: ""
        property string pendingOrigin: ""
        property bool pendingUpdate: false
        property var entries: Array.from({ length: 40 }, (_, n) => ({ id: "fixture-" + n, origin: "https://" + ["wikipedia.org", "developer.mozilla.org", "news.ycombinator.com", "github.com"][n % 4], username: "sample-user-" + (n + 1) }))
        signal stateChanged()
        signal credentialSaved()
        function clearMessage() {}
        function remove(id) { entries = entries.filter(entry => entry.id !== id) }
    }
    ColumnLayout {
        anchors.fill: parent; anchors.margins: 48; spacing: 22
        Basic.Label { text: "Passwords"; font.pixelSize: 28; font.weight: Font.DemiBold; color: Theme.text }
        RowLayout {
            Repeater { model: ["Passwords", "Passkeys", "Sync", "Import"]; UiButton { required property string modelData; text: modelData; flat: true; highlighted: modelData === "Passwords" } }
        }
        PasswordSettings { Layout.fillWidth: true; manager: mock; onManageRequested: passwordPanel.openManager() }
        Item { Layout.fillHeight: true }
    }
    PasswordPanel { id: passwordPanel; anchors.fill: parent; manager: mock; activeView: null }
    function openManagerPreview() { passwordPanel.openManager() }
    Component.onCompleted: { Theme.dark = true; Theme.liquidGlass = false; if (showManager) Qt.callLater(passwordPanel.openManager) }
}
