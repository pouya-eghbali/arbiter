pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Window
import QtQuick.Layouts
import QtQuick.Controls.Basic as Basic
import "ui"

Basic.ApplicationWindow {
    id: setup
    required property var controller
    required property var appearance
    required property var importSelection
    required property var importer
    required property var systemIntegration
    property bool reduceMotion: systemIntegration.reducedMotion
    property bool revealed: false
    function resetScroll() { const flick = scroll.contentItem as Flickable; if (flick) flick.contentY = 0 }
    minimumWidth: 680; minimumHeight: 570
    width: Math.max(minimumWidth, Math.min(940, Screen.desktopAvailableWidth - 48))
    height: Math.max(minimumHeight, Math.min(760, Screen.desktopAvailableHeight - 64))
    x: Screen.virtualX + (Screen.width - width) / 2
    y: Screen.virtualY + (Screen.height - height) / 2
    title: qsTr("Welcome to Arbiter")
    color: Theme.window
    palette.window: Theme.window
    palette.base: Theme.window
    palette.text: Theme.text
    palette.windowText: Theme.text
    palette.button: Theme.field
    palette.buttonText: Theme.text
    palette.placeholderText: Theme.muted
    palette.highlight: Theme.selection
    palette.highlightedText: Theme.text
    visible: true
    onActiveChanged: if (active && controller.step === 4) systemIntegration.refreshDefault()
    onClosing: { importSelection.cancel(); importer.cancel(); controller.abort() }
    Binding { target: Theme; property: "dark"; value: setup.appearance.dark }
    Binding { target: Theme; property: "lightAccent"; value: setup.appearance.lightAccent }
    Binding { target: Theme; property: "darkAccent"; value: setup.appearance.darkAccent }
    Binding { target: Theme; property: "lightBackground"; value: setup.appearance.lightBackground }
    Binding { target: Theme; property: "darkBackground"; value: setup.appearance.darkBackground }
    Component.onCompleted: { revealed = true; if (controller.step === 4) systemIntegration.refreshDefault() }
    Timer {
        id: importResultsScroll
        interval: 80
        onTriggered: {
            const flick = scroll.contentItem as Flickable
            if (flick && setup.controller.step === 1) flick.contentY = Math.max(0, flick.contentHeight - flick.height)
        }
    }
    Connections {
        target: setup.importSelection
        function onChanged() { if (setup.importSelection.busy && setup.controller.step === 1) setup.resetScroll() }
        function onCompleted(success) {
            if (setup.controller.step !== 1) return
            if (success) { setup.controller.next(); setup.resetScroll() }
            else importResultsScroll.restart()
        }
    }
    Connections {
        target: setup.controller
        function onChanged() {
            if (setup.controller.step === 4) setup.systemIntegration.refreshDefault()
        }
    }
    // Native title bar is intentionally available during setup, before a preset is applied.
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: setup.width < 800 ? 28 : 48
        spacing: 24
        RowLayout {
            spacing: 10
            Basic.Label { text: "Arbiter"; font.pixelSize: 18; font.weight: Font.DemiBold; color: Theme.text }
            Item { Layout.fillWidth: true }
            Basic.Label {
                text: [qsTr("Welcome"), qsTr("Import"), qsTr("Your space"), qsTr("Colors"), qsTr("Default browser"), qsTr("Ready")][setup.controller.step]
                color: Theme.muted
            }
            Row {
                spacing: 5
                Repeater {
                    model: 6
                    Rectangle {
                        required property int index
                        width: index === setup.controller.step ? 18 : 5
                        height: 5; radius: 3
                        color: index === setup.controller.step ? Theme.accent : Theme.border
                    }
                }
            }
        }
        Basic.ScrollView {
            id: scroll
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: availableWidth
            clip: true
            ColumnLayout {
                width: scroll.availableWidth
                spacing: 22
                ColumnLayout {
                    visible: setup.controller.step === 0
                    Layout.fillWidth: true
                    spacing: 20
                    Item { Layout.preferredHeight: 16 }
                    Rectangle {
                        id: logo
                        Layout.alignment: Qt.AlignHCenter
                        Layout.preferredWidth: 128; Layout.preferredHeight: 128
                        radius: 36
                        color: Theme.accent
                        opacity: setup.revealed ? 1 : 0
                        scale: setup.revealed ? 1 : 0.92
                        Behavior on opacity { enabled: !setup.reduceMotion; NumberAnimation { duration: 650; easing.type: Easing.OutCubic } }
                        Behavior on scale { enabled: !setup.reduceMotion; NumberAnimation { duration: 650; easing.type: Easing.OutCubic } }
                        Image { anchors.fill: parent; anchors.margins: 14; source: "qrc:/brand/valkyrie-source.png"; fillMode: Image.PreserveAspectFit; sourceSize.width: 256; sourceSize.height: 256; Accessible.name: qsTr("Arbiter logo") }
                    }
                    Basic.Label {
                        text: qsTr("Welcome to Arbiter.")
                        font.pixelSize: 36; font.weight: Font.DemiBold
                        color: Theme.text
                        Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.Wrap
                        opacity: setup.revealed ? 1 : 0
                        Behavior on opacity { enabled: !setup.reduceMotion; NumberAnimation { duration: 800; easing.type: Easing.InOutCubic } }
                    }
                    Basic.Label { text: qsTr("A little less browser. A little more web."); color: Theme.muted; font.pixelSize: 18; Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.Wrap }
                    Basic.Label { text: qsTr("Bring your things, find your style, and make yourself at home."); color: Theme.muted; Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.Wrap }
                    UiCheckBox { text: qsTr("Reduce motion"); checked: setup.reduceMotion; onToggled: setup.reduceMotion = checked; Layout.alignment: Qt.AlignHCenter }
                }
                Loader {
                    active: setup.controller.step === 1
                    visible: active
                    Layout.fillWidth: true
                    sourceComponent: OnboardingImport {
                        selection: setup.importSelection
                        importer: setup.importer
                    }
                }
                ColumnLayout {
                    visible: setup.controller.step === 2
                    Layout.fillWidth: true
                    spacing: 20
                    Basic.Label { text: qsTr("Make yourself at home."); font.pixelSize: 32; color: Theme.text; Layout.fillWidth: true; wrapMode: Text.Wrap }
                    Basic.Label { text: qsTr("How much browser would you like around your web?"); color: Theme.muted; Layout.fillWidth: true; wrapMode: Text.Wrap }
                    RowLayout {
                        spacing: 12
                        Layout.fillWidth: true
                        Repeater {
                            model: [ { key: "minimal", title: qsTr("Minimal"), detail: qsTr("Just the web. Shortcuts do the rest."), pill: false, deco: false },
                                { key: "glacier", title: qsTr("Glacier"), detail: qsTr("A little navigation, tucked away."), pill: true, deco: false },
                                { key: "classic", title: qsTr("Classic"), detail: qsTr("Familiar window controls and navigation."), pill: true, deco: true } ]
                            UiButton {
                                id: presetButton
                                required property var modelData
                                Layout.fillWidth: true
                                Layout.preferredWidth: 220
                                Layout.preferredHeight: 250
                                objectName: "preset_" + modelData.key
                                highlighted: setup.controller.preset === modelData.key
                                Accessible.name: modelData.title + ". " + modelData.detail
                                Accessible.role: Accessible.RadioButton
                                Accessible.checkable: true
                                Accessible.checked: highlighted
                                onClicked: setup.controller.choosePreset(modelData.key)
                                contentItem: ColumnLayout {
                                    spacing: 12
                                    BrowserPreview { Layout.fillWidth: true; Layout.preferredHeight: 130; pill: presetButton.modelData.pill; decorations: presetButton.modelData.deco }
                                    Basic.Label { text: presetButton.modelData.title; font.pixelSize: 18; font.weight: Font.DemiBold; color: Theme.text }
                                    Basic.Label { text: presetButton.modelData.detail; Layout.fillWidth: true; Layout.minimumHeight: 36; Layout.preferredHeight: 36; wrapMode: Text.Wrap; color: Theme.muted; font.pixelSize: 12 }
                                }
                            }
                        }
                    }
                    Basic.Label { text: qsTr("You can change these anytime in Appearance settings."); color: Theme.muted; Layout.fillWidth: true; wrapMode: Text.Wrap }
                }
                ColumnLayout {
                    visible: setup.controller.step === 3
                    Layout.fillWidth: true
                    spacing: 20
                    Basic.Label { text: qsTr("Pick your colors."); font.pixelSize: 32; color: Theme.text; Layout.fillWidth: true; wrapMode: Text.Wrap }
                    RowLayout {
                        Repeater {
                            model: [{ key: "system", title: qsTr("System") }, { key: "light", title: qsTr("Light") }, { key: "dark", title: qsTr("Dark") }]
                            UiButton {
                                required property var modelData
                                text: modelData.title
                                objectName: "onboardingTheme_" + modelData.key
                                highlighted: setup.appearance.theme === modelData.key
                                Accessible.role: Accessible.RadioButton
                                Accessible.checked: highlighted
                                onClicked: setup.appearance.theme = modelData.key
                            }
                        }
                    }
                    Basic.Label { text: qsTr("System follows your device. Customize the accent and background for the current appearance; Light and Dark are saved separately."); color: Theme.muted; Layout.fillWidth: true; wrapMode: Text.Wrap }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 24
                        UiColorPicker {
                            Layout.fillWidth: true
                            Layout.preferredWidth: 1
                            Layout.alignment: Qt.AlignTop
                            title: setup.appearance.dark ? qsTr("Dark accent") : qsTr("Light accent")
                            value: setup.appearance.dark ? setup.appearance.darkAccent : setup.appearance.lightAccent
                            defaultColor: setup.appearance.dark ? "#8dbbff" : "#2868b8"
                            onColorSelected: color => { if (setup.appearance.dark) setup.appearance.darkAccent = color; else setup.appearance.lightAccent = color }
                        }
                        UiColorPicker {
                            objectName: "onboardingBackgroundPicker"
                            Layout.fillWidth: true
                            Layout.preferredWidth: 1
                            Layout.alignment: Qt.AlignTop
                            title: setup.appearance.dark ? qsTr("Dark background") : qsTr("Light background")
                            value: setup.appearance.dark ? setup.appearance.darkBackground : setup.appearance.lightBackground
                            defaultColor: setup.appearance.dark ? "#11161e" : "#edf2f7"
                            presets: setup.appearance.dark
                                ? ["#11161e", "#19181d", "#181d19", "#1e1820", "#142023", "#121212", "#202530", "#211b18"]
                                : ["#edf2f7", "#f7f3ed", "#f1f5ed", "#f4eff7", "#eef6f6", "#ffffff", "#e3e7ed", "#f4eeee"]
                            onColorSelected: color => { if (setup.appearance.dark) setup.appearance.darkBackground = color; else setup.appearance.lightBackground = color }
                        }
                    }
                    BrowserPreview {
                        Layout.fillWidth: true; Layout.preferredHeight: 140
                        pill: setup.appearance.navigationPill
                        decorations: setup.appearance.windowDecorations
                        caption: qsTr("Your own little corner of the web.")
                    }
                }
                ColumnLayout {
                    visible: setup.controller.step === 4
                    Layout.fillWidth: true
                    spacing: 20
                    Basic.Label { text: qsTr("Make this your everyday browser?"); font.pixelSize: 32; color: Theme.text; Layout.fillWidth: true; wrapMode: Text.Wrap }
                    Basic.Label { text: qsTr("Open web links in Arbiter. Your system may ask you to confirm. This is completely optional."); color: Theme.muted; Layout.fillWidth: true; wrapMode: Text.Wrap }
                    UiButton {
                        objectName: "onboardingDefaultButton"
                        text: setup.systemIntegration.isDefault ? qsTr("Arbiter is your default browser") : setup.systemIntegration.busy ? qsTr("Waiting for your system…") : qsTr("Set as default")
                        primary: !setup.systemIntegration.isDefault
                        enabled: !setup.systemIntegration.busy && !setup.systemIntegration.isDefault
                        onClicked: setup.systemIntegration.requestDefault()
                    }
                    Basic.Label { text: setup.systemIntegration.message; textFormat: Text.PlainText; visible: text.length > 0; color: Theme.muted; Layout.fillWidth: true; wrapMode: Text.Wrap }
                    Basic.Label { text: qsTr("You can also choose Arbiter later in your system's default application settings."); color: Theme.muted; Layout.fillWidth: true; wrapMode: Text.Wrap }
                }
                ColumnLayout {
                    visible: setup.controller.step === 5
                    Layout.fillWidth: true
                    spacing: 20
                    Basic.Label { text: qsTr("You're home."); font.pixelSize: 36; font.weight: Font.DemiBold; color: Theme.text; Layout.fillWidth: true; wrapMode: Text.Wrap }
                    Basic.Label { text: qsTr("Your browser, with a little more breathing room."); color: Theme.muted; font.pixelSize: 18; Layout.fillWidth: true; wrapMode: Text.Wrap }
                    BrowserPreview { Layout.fillWidth: true; Layout.preferredHeight: 220; pill: setup.appearance.navigationPill; decorations: setup.appearance.windowDecorations; caption: qsTr("Ready when you are.") }
                    Basic.Label { text: qsTr("Take a quick, optional tour of addresses, tabs and workspaces, and split views. You can replay it from Settings."); color: Theme.muted; Layout.fillWidth: true; wrapMode: Text.Wrap }
                }
            }
        }
        Basic.Label { text: setup.controller.error; textFormat: Text.PlainText; visible: text.length > 0; color: Theme.text; Layout.fillWidth: true; wrapMode: Text.Wrap }
        RowLayout {
            Layout.fillWidth: true
            UiButton { objectName: "onboardingBack"; text: qsTr("Back"); visible: setup.controller.step > 0; enabled: !setup.importSelection.busy && !setup.importer.busy && !setup.systemIntegration.busy; onClicked: { if (setup.controller.step === 1) setup.importer.cancel(); setup.controller.back(); setup.resetScroll() } }
            Item { Layout.fillWidth: true }
            UiButton { objectName: "onboardingSkipTutorial"; text: qsTr("Start browsing"); visible: setup.controller.step === 5; onClicked: setup.controller.finish(false) }
            UiButton {
                objectName: "onboardingContinue"
                primary: true
                text: setup.controller.step === 0 ? qsTr("Get started") : setup.controller.step === 1 ? (setup.importSelection.done ? (setup.importSelection.hasFailures ? qsTr("Continue anyway") : qsTr("Continue")) : setup.importSelection.selectedCount > 0 ? qsTr("Import and continue") : qsTr("Start fresh")) : setup.controller.step === 4 && !setup.systemIntegration.isDefault ? qsTr("Not now") : setup.controller.step === 5 ? qsTr("Show me around") : qsTr("Continue")
                enabled: !setup.importSelection.busy && !setup.importer.busy && !setup.systemIntegration.busy && (setup.controller.step !== 4 || setup.active)
                onClicked: {
                    if (setup.controller.step === 5) setup.controller.finish(true)
                    else if (setup.controller.step === 1 && setup.importSelection.selectedCount > 0 && !setup.importSelection.done) setup.importSelection.start()
                    else { if (setup.controller.step === 1) setup.importer.cancel(); setup.controller.next(); setup.resetScroll() }
                }
            }
        }
    }
}
