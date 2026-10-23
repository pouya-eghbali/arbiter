pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls as Controls
import QtWebEngine

Controls.Menu {
    id: contextMenu
    required property WebEngineView view
    property bool passwordsEnabled: true
    signal passwordsRequested()
    popupType: Qt.platform.os === "osx" ? Controls.Popup.Native : Controls.Popup.Window

    Component {
        id: actionItem
        Controls.MenuItem {
            required property var webAction
            property bool contextEnabled: true
            text: webAction.text
            enabled: contextEnabled && webAction.enabled
            onTriggered: webAction.trigger()
        }
    }
    Component {
        id: suggestionItem
        Controls.MenuItem {
            onTriggered: contextMenu.view.replaceMisspelledWord(text)
        }
    }
    Component {
        id: separatorItem
        Controls.MenuSeparator { }
    }
    Component {
        id: passwordsItem
        Controls.MenuItem {
            text: qsTr("Fill or Save Login…")
            enabled: contextMenu.passwordsEnabled && contextMenu.view.url.toString().startsWith("https://")
            onTriggered: contextMenu.passwordsRequested()
        }
    }

    function addWebAction(command: int, allowed = true) {
        addItem(actionItem.createObject(contextMenu.contentItem, {
            webAction: view.action(command), contextEnabled: Boolean(allowed)
        }))
    }

    function separator() {
        if (count > 0)
            addItem(separatorItem.createObject(contextMenu.contentItem))
    }

    function show(request: ContextMenuRequest) {
        // The request belongs to WebEngine. Build the menu synchronously and
        // retain only actions and copied suggestion text after this handler.
        while (count > 0) {
            const item = takeItem(0)
            item.destroy()
        }
        if (request.isContentEditable) {
            if (request.misspelledWord.length > 0) {
                for (const suggestion of request.spellCheckerSuggestions)
                    addItem(suggestionItem.createObject(contextMenu.contentItem, { text: suggestion }))
                separator()
            }
            addWebAction(WebEngineView.Undo, request.editFlags & ContextMenuRequest.CanUndo)
            addWebAction(WebEngineView.Redo, request.editFlags & ContextMenuRequest.CanRedo)
            separator()
            addWebAction(WebEngineView.Cut, request.editFlags & ContextMenuRequest.CanCut)
            addWebAction(WebEngineView.Copy, request.editFlags & ContextMenuRequest.CanCopy)
            addWebAction(WebEngineView.Paste, request.editFlags & ContextMenuRequest.CanPaste)
            addWebAction(WebEngineView.PasteAndMatchStyle, request.editFlags & ContextMenuRequest.CanPaste)
            separator()
            addWebAction(WebEngineView.SelectAll, request.editFlags & ContextMenuRequest.CanSelectAll)
        } else if (request.selectedText.length > 0) {
            addWebAction(WebEngineView.Copy, request.editFlags & ContextMenuRequest.CanCopy)
        }
        if (request.linkUrl.toString().length > 0) {
            separator()
            addWebAction(WebEngineView.OpenLinkInNewTab)
            addWebAction(WebEngineView.OpenLinkInNewBackgroundTab)
            addWebAction(WebEngineView.CopyLinkToClipboard)
        }
        if (request.mediaType === ContextMenuRequest.MediaTypeImage
                || request.mediaType === ContextMenuRequest.MediaTypeCanvas) {
            separator()
            addWebAction(WebEngineView.CopyImageToClipboard)
            if (request.mediaUrl.toString().length > 0)
                addWebAction(WebEngineView.CopyImageUrlToClipboard)
        } else if (request.mediaType === ContextMenuRequest.MediaTypeVideo
                   || request.mediaType === ContextMenuRequest.MediaTypeAudio) {
            separator()
            addWebAction(WebEngineView.ToggleMediaPlayPause)
            if (request.mediaFlags & ContextMenuRequest.MediaHasAudio)
                addWebAction(WebEngineView.ToggleMediaMute)
            addWebAction(WebEngineView.ToggleMediaLoop)
            if (request.mediaFlags & ContextMenuRequest.MediaCanToggleControls)
                addWebAction(WebEngineView.ToggleMediaControls)
            if (request.mediaUrl.toString().length > 0)
                addWebAction(WebEngineView.CopyMediaUrlToClipboard)
        }
        if (count === 0) {
            addWebAction(WebEngineView.Back)
            addWebAction(WebEngineView.Forward)
            addWebAction(WebEngineView.Reload)
        }
        separator()
        addItem(passwordsItem.createObject(contextMenu.contentItem))
        request.accepted = true
        popup(view, request.position)
    }
}
