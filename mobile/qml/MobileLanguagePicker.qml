import QtQuick
import Arbiter

UiComboBox {
    id: picker
    readonly property var codes: ["system", "en", "de", "fr", "it", "es", "pt", "ja", "zh"]
    model: [qsTr("System language"), "English", "Deutsch", "Français", "Italiano", "Español", "Português", "日本語", "简体中文"]
    currentIndex: Math.max(0, codes.indexOf(browser.settings.language))
    onActivated: index => browser.setSetting("language", codes[index])
}
