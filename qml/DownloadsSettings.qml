pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic as Basic
import QtQuick.Dialogs as Dialogs
import Arbiter

ColumnLayout {
    id: page
    required property var manager
    signal showRequested()
    spacing: 16
    Basic.Label { Layout.fillWidth: true; text: qsTr("Save downloaded files to"); color: Theme.text; font.weight: Font.DemiBold }
    Basic.Label { Layout.fillWidth: true; text: page.manager.preferences.directoryPath; textFormat: Text.PlainText; color: Theme.muted; wrapMode: Text.WrapAnywhere }
    RowLayout {
        UiButton { text: qsTr("Choose folder…"); onClicked: folder.open() }
        UiButton { text: qsTr("Show downloads"); onClicked: page.showRequested() }
    }
    Dialogs.FolderDialog {
        id: folder
        title: qsTr("Choose download folder")
        currentFolder: page.manager.preferences.directory
        onAccepted: page.manager.preferences.directory = selectedFolder
    }
    Basic.Label {
        Layout.fillWidth: true
        text: qsTr("Downloads start automatically in this folder. Existing files are kept; duplicate names receive a number. Files never open automatically. Private windows keep their download list only until that window closes; saved files remain on disk.")
        color: Theme.muted
        wrapMode: Text.Wrap
    }
}
