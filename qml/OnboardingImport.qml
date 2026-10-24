pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic as Basic
import "ui"

ColumnLayout {
    id: page
    required property var selection
    required property var importer
    property int refocusIndex: -1
    spacing: 18
    Timer {
        id: restoreFocus
        interval: 0
        onTriggered: { const card = browserCards.itemAt(page.refocusIndex); if (card) card.forceActiveFocus() }
    }
    Component.onCompleted: selection.refresh()
    Basic.Label { text: qsTr("Bring your things."); font.pixelSize: 32; color: Theme.text; Layout.fillWidth: true; wrapMode: Text.Wrap }
    Basic.Label {
        text: qsTr("Choose the browsers you want to bring into Arbiter. Continue imports the available passwords and history from every detected profile in the browsers you select.")
        color: Theme.muted; Layout.fillWidth: true; wrapMode: Text.Wrap
    }
    ImportProgress { objectName: "onboardingImportProgress"; controller: page.importer; heading: page.selection.progress }
    GridLayout {
        objectName: "importBrowserGrid"
        Layout.fillWidth: true
        columns: width >= 780 ? 3 : 2
        columnSpacing: 14
        rowSpacing: 14
        uniformCellWidths: true
        Repeater {
            id: browserCards
            model: page.selection.browsers
            UiCheckBox {
                id: card
                required property var modelData
                required property int index
                objectName: "importBrowser_" + modelData.name
                Layout.fillWidth: true
                Layout.preferredWidth: 1
                Layout.preferredHeight: 96
                padding: 14
                checked: modelData.selected
                enabled: !page.selection.busy && !page.importer.busy
                Accessible.name: modelData.accessRequired ? qsTr("Allow access to %1").arg(modelData.name) : qsTr("Import from %1, %2 profiles").arg(modelData.name).arg(modelData.profiles.length)
                onToggled: {
                    const keepFocus = activeFocus
                    page.refocusIndex = index
                    page.selection.toggle(index, card.Window.window)
                    if (keepFocus) restoreFocus.restart()
                }
                indicator: Rectangle {
                    x: card.leftPadding
                    y: (card.height - height) / 2
                    width: 22; height: 22; radius: 7
                    color: card.checked ? Theme.accent : Theme.field
                    border.width: 1
                    border.color: card.checked || card.activeFocus ? Theme.accent : Theme.border
                    Text { anchors.centerIn: parent; text: "✓"; visible: card.checked; color: Theme.accentText; font.pixelSize: 15 }
                }
                background: Rectangle {
                    radius: 18
                    color: card.checked ? Theme.selection : card.hovered ? Theme.hover : Theme.field
                    border.width: 1
                    border.color: card.checked || card.activeFocus ? Theme.accent : Theme.border
                }
                contentItem: Item {
                    implicitHeight: 64
                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 34
                        spacing: 10
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 5
                            Basic.Label { text: card.modelData.name; textFormat: Text.PlainText; color: Theme.text; font.pixelSize: 17; font.weight: Font.DemiBold; Layout.fillWidth: true; elide: Text.ElideRight }
                            Basic.Label {
                                text: (card.modelData.passwords && card.modelData.history ? qsTr("Passwords + history") : card.modelData.passwords ? qsTr("Passwords") : qsTr("History"))
                                    + "\n" + (card.modelData.accessRequired ? qsTr("Select to allow access") : card.modelData.profiles.length === 1 ? qsTr("1 profile") : qsTr("%1 profiles").arg(card.modelData.profiles.length))
                                color: Theme.muted; font.pixelSize: 11; Layout.fillWidth: true; wrapMode: Text.Wrap
                            }
                        }
                        Image {
                            Layout.preferredWidth: 36; Layout.preferredHeight: 36
                            source: "image://browsers/" + encodeURIComponent(card.modelData.name)
                            sourceSize.width: 72; sourceSize.height: 72
                            fillMode: Image.PreserveAspectFit
                            Accessible.ignored: true
                        }
                    }
                }
            }
        }
    }
    Basic.Label {
        visible: page.selection.browsers.length === 0
        text: qsTr("No supported browser profiles were found. Start fresh, or import exported files later in Settings → Import.")
        color: Theme.muted; Layout.fillWidth: true; wrapMode: Text.Wrap
    }
    Basic.Label { text: page.importer.discoveryWarning; visible: text.length > 0; color: Theme.muted; Layout.fillWidth: true; wrapMode: Text.Wrap; textFormat: Text.PlainText }
    UiButton { text: qsTr("Refresh browsers"); flat: true; enabled: !page.selection.busy && !page.importer.busy; onClicked: page.selection.refresh() }
    Basic.Label { text: qsTr("Your system may ask you to unlock a browser's password vault. Existing Arbiter passwords are kept; duplicate history visits are skipped. Passwords unavailable for direct import can be brought in later from an exported file in Settings."); color: Theme.muted; Layout.fillWidth: true; wrapMode: Text.Wrap }
    Basic.Label { text: page.importer.passkeyImportNotice; color: Theme.muted; Layout.fillWidth: true; wrapMode: Text.Wrap }
    UiButton { text: qsTr("Stop importing"); visible: page.selection.busy; onClicked: page.selection.cancel() }
    Repeater {
        model: page.selection.results
        Basic.Label {
            required property var modelData
            text: modelData.label + "\n" + modelData.message
            textFormat: Text.PlainText; color: Theme.text; Layout.fillWidth: true; wrapMode: Text.Wrap
        }
    }
    Basic.Label { visible: page.selection.done && page.selection.hasFailures; text: qsTr("Some profiles could not be imported. Retry them, or continue setup and import them later from Settings."); color: Theme.muted; Layout.fillWidth: true; wrapMode: Text.Wrap }
    UiButton { objectName: "retryBrowserImports"; text: qsTr("Retry failed imports"); visible: page.selection.done && page.selection.hasFailures; enabled: !page.selection.busy && !page.importer.busy; onClicked: page.selection.retryFailed() }
}
