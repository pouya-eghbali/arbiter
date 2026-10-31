pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic as Controls
import QtQuick.Layouts
import Arbiter

MobilePresentation {
    id: root
    signal dismiss
    property real keyboardInset: 0
    property string editingId: ""
    function focusEditor() {
        if (!editor.visible) return;
        const target = workspaceName.Window.window;
        if (target) target.requestActivate();
        workspaceName.forceActiveFocus();
    }
    function edit(id, name) {
        editingId = id;
        workspaceName.text = name;
        isolated.checked = false;
        editor.open();
    }
    NativeSheet {
        anchors.fill: parent
        anchors.margins: 12
        anchors.topMargin: 12 + 16 * (1 - root.progress)
        anchors.bottomMargin: 12 - 16 * (1 - root.progress)
        interactive: root.presented
        onEscapeRequested: root.dismiss()
        requested: Theme.liquidGlass && !editor.visible
        onNativeActiveChanged: if (!nativeActive && editor.visible) Qt.callLater(root.focusEditor)
    ColumnLayout {
        width: Math.min(parent.width - 40, 560)
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.topMargin: 12
        anchors.bottomMargin: 12
        spacing: 16
        RowLayout {
            Layout.fillWidth: true
            Text {
                text: qsTr("Workspaces")
                color: Theme.textFor(parent)
                font.pixelSize: 24
                elide: Text.ElideRight
                font.weight: Font.DemiBold
                Layout.fillWidth: true
            }
            MobileIconButton {
                objectName: "mobileWorkspaceNewButton"
                iconName: "plus"
                label: qsTr("New workspace")
                onClicked: root.edit("", "")
            }
            MobileCloseButton {
                label: qsTr("Done")
                onClicked: root.dismiss()
            }
        }
        Controls.ScrollView {
            id: workspaceScroll
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: availableWidth
            clip: true
            ColumnLayout {
                width: workspaceScroll.availableWidth
                spacing: 12
                Repeater {
                    model: browser.workspaces
                    delegate: Rectangle {
                        id: card
                        required property var modelData
                        readonly property var previews: browser.tabs.filter(t => t.workspace === modelData.id && Boolean(t.private) === browser.privateMode)
                        Layout.fillWidth: true
                        implicitHeight: body.implicitHeight + 28
                        radius: 18
                        readonly property bool nativeMaterial: Theme.nativeFor(card)
                        readonly property bool selected: modelData.id === browser.workspaceId
                        color: nativeMaterial ? Qt.alpha(Theme.textFor(card), selected ? .10 : .035)
                            : selected ? Qt.alpha(Theme.accent, .12) : Theme.fieldFor(parent)
                        border.width: selected ? 1 : 0
                        border.color: nativeMaterial ? Qt.alpha(Theme.textFor(card), .25) : Qt.alpha(Theme.accent, .75)
                        UiButton {
                            anchors.fill: parent
                            flat: true
                            text: ""
                            background: Item {}
                            Accessible.name: card.modelData.name
                            onClicked: {
                                browser.selectWorkspace(card.modelData.id);
                                root.dismiss();
                            }
                            onPressAndHold: root.edit(card.modelData.id, card.modelData.name)
                        }
                        ColumnLayout {
                            id: body
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.margins: 14
                            spacing: 10
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 10
                                Rectangle {
                                    width: 13
                                    height: 13
                                    radius: 7
                                    color: card.modelData.color || Theme.accent
                                }
                                Text {
                                    textFormat: Text.PlainText
                                    text: card.modelData.name
                                    color: Theme.textFor(parent)
                                    font.pixelSize: 16
                                    font.weight: Font.Medium
                                    Layout.fillWidth: true
                                    elide: Text.ElideRight
                                }
                                MobileIconButton {
                                    iconName: "ellipsis"
                                    label: qsTr("Edit workspace")
                                    implicitWidth: 44
                                    implicitHeight: 44
                                    iconSize: 18
                                    onClicked: root.edit(card.modelData.id, card.modelData.name)
                                }
                                Rectangle {
                                    width: 23
                                    height: 23
                                    radius: 12
                                    color: card.selected ? (card.nativeMaterial ? Qt.alpha(Theme.textFor(card), .12) : Theme.accent) : "transparent"
                                    border.width: 1
                                    border.color: card.nativeMaterial ? Qt.alpha(Theme.textFor(card), .28) : card.selected ? Theme.accent : Theme.mutedFor(parent)
                                    UiIcon {
                                        anchors.centerIn: parent
                                        width: 14
                                        height: 14
                                        name: "check"
                                        visible: card.modelData.id === browser.workspaceId
                                        color: card.nativeMaterial ? Theme.textFor(card) : Theme.contrastingText(Theme.accent)
                                    }
                                }
                            }
                            Repeater {
                                model: card.previews.slice(0, 3)
                                delegate: RowLayout {
                                    required property var modelData
                                    spacing: 9
                                    Layout.fillWidth: true
                                    UiIcon {
                                        name: modelData.sleeping ? "moon" : "globe"
                                        color: Theme.mutedFor(parent)
                                        Layout.preferredWidth: 16
                                        Layout.preferredHeight: 16
                                    }
                                    Text {
                                        Layout.fillWidth: true
                                        textFormat: Text.PlainText
                                        text: (modelData.title || qsTr("New tab")) + (modelData.url ? " — " + modelData.url.replace(/^https?:\/\//, "").split("/")[0] : "")
                                        color: Theme.mutedFor(parent)
                                        font.pixelSize: 12
                                        elide: Text.ElideRight
                                    }
                                }
                            }
                            Text {
                                visible: card.previews.length > 3
                                text: "+ " + (card.previews.length === 4 ? qsTr("1 other tab") : qsTr("%1 other tabs").arg(card.previews.length - 3))
                                color: Theme.mutedFor(parent)
                                font.pixelSize: 12
                                Layout.topMargin: 2
                            }
                            Text {
                                visible: card.previews.length === 0
                                text: qsTr("No open tabs")
                                color: Theme.mutedFor(parent)
                                font.pixelSize: 12
                            }
                        }
                    }
                }
            }
        }
    }
    }
    UiPopup {
        id: editor
        objectName: "workspaceEditor"
        x: (root.width - width) / 2
        y: Math.max(12, (root.height - root.keyboardInset - height) / 2)
        width: Math.min(root.width - 24, 380)
        height: Math.min(editorBody.implicitHeight + 40, Math.max(160, root.height - root.keyboardInset - 24))
        padding: 20
        modal: true
        focus: true
        background: GlassSurface {
            dense: true
            radius: 24
        }
        contentItem: Controls.ScrollView {
            id: editorScroll
            contentWidth: availableWidth
            clip: true
            ColumnLayout {
                id: editorBody
                width: editorScroll.availableWidth
                spacing: 16
                Text {
                    text: root.editingId ? qsTr("Edit workspace") : qsTr("New workspace")
                    font.pixelSize: 22
                    font.weight: Font.DemiBold
                    color: Theme.textFor(parent)
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
                MobileField {
                    id: workspaceName
                    objectName: "mobileWorkspaceNameInput"
                    Layout.fillWidth: true
                    placeholderText: qsTr("Workspace name")
                    onAccepted: if (save.enabled)
                        save.clicked()
                }
                MobileToggle {
                    id: isolated
                    visible: !root.editingId
                    Layout.fillWidth: true
                    text: qsTr("Use a separate session")
                }
                MobileButton {
                    id: save
                    Layout.fillWidth: true
                    text: qsTr("Save")
                    primary: true
                    enabled: workspaceName.text.trim().length > 0
                    onClicked: {
                        if (root.editingId)
                            browser.renameWorkspace(root.editingId, workspaceName.text);
                        else
                            browser.addWorkspace(workspaceName.text, isolated.checked);
                        editor.close();
                    }
                }
                MobileButton {
                    Layout.fillWidth: true
                    visible: Boolean(root.editingId) && browser.workspaces.length > 1
                    iconName: "trash-2"
                    text: qsTr("Delete")
                    flat: true
                    onClicked: {
                        browser.removeWorkspace(root.editingId);
                        editor.close();
                    }
                }
                MobileButton {
                    Layout.fillWidth: true
                    flat: true
                    objectName: "mobileWorkspaceCancelButton"
                    text: qsTr("Cancel")
                    onClicked: editor.close()
                }
            }
        }
        onOpened: Qt.callLater(root.focusEditor)
        onClosed: Qt.inputMethod.hide()
    }
}
