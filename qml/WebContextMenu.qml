pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic as Controls
import Arbiter
import QtWebEngine

UiMenu {
    id: contextMenu
    objectName: "webContextMenu"
    required property WebEngineView view
    property bool passwordsEnabled: true
    signal inspectRequested()
    signal printRequested()
    signal passwordsRequested()
    signal passwordSuggestionRequested(var field)
    property int requestGeneration: 0

    Component {
        id: actionItem
        UiMenuItem {
            required property var webAction
            property bool contextEnabled: true
            text: webAction.text
            enabled: contextEnabled && webAction.enabled
            onTriggered: webAction.trigger()
        }
    }
    Component {
        id: suggestionItem
        UiMenuItem {
            onTriggered: contextMenu.view.replaceMisspelledWord(text)
        }
    }
    Component {
        id: separatorItem
        Controls.MenuSeparator { contentItem: Rectangle { implicitHeight: 1; color: Theme.border } }
    }
    Component {
        id: extensionItem
        UiMenuItem {
            required property string extensionKey
            required property var menuContext
            onTriggered: contextMenu.view.invokeExtensionMenu(extensionKey, menuContext)
        }
    }
    Component {
        id: passwordsItem
        UiMenuItem {
            text: qsTr("Saved passwords…")
            enabled: contextMenu.passwordsEnabled && contextMenu.view.url.toString().startsWith("https://")
            onTriggered: contextMenu.passwordsRequested()
        }
    }
    Component {
        id: printItem
        UiMenuItem {
            objectName: "printPageAction"
            text: qsTr("Print…")
            onTriggered: contextMenu.printRequested()
        }
    }
    Component {
        id: inspectItem
        UiMenuItem {
            objectName: "inspectElementAction"
            text: qsTr("Inspect element")
            onTriggered: contextMenu.inspectRequested()
        }
    }
    Component {
        id: generatePasswordItem
        UiMenuItem {
            required property var field
            objectName: "suggestPasswordAction"
            text: qsTr("Suggest password")
            onTriggered: contextMenu.passwordSuggestionRequested(field)
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
        const generation = ++requestGeneration
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
        addItem(printItem.createObject(contextMenu.contentItem))
        addItem(passwordsItem.createObject(contextMenu.contentItem))
        const passwordIndex = count - 1
        separator()
        addItem(inspectItem.createObject(contextMenu.contentItem))
        const menuContext = {
            pageUrl: view.mainFrame.url.toString(),
            linkUrl: request.linkUrl.toString(),
            srcUrl: request.mediaUrl.toString(),
            selectionText: request.selectedText,
            editable: request.isContentEditable,
            mediaType: request.mediaType === ContextMenuRequest.MediaTypeImage ? "image"
                : request.mediaType === ContextMenuRequest.MediaTypeVideo ? "video"
                : request.mediaType === ContextMenuRequest.MediaTypeAudio ? "audio" : ""
        }
        const extensionItems = view.extensionMenuItems(menuContext)
        if (extensionItems.length) separator()
        for (const entry of extensionItems) {
            if (entry.separator) separator()
            else addItem(extensionItem.createObject(contextMenu.contentItem, {
                extensionKey: entry.key, menuContext: menuContext, text: entry.title,
                enabled: entry.enabled, checkable: entry.checkable, checked: entry.checked
            }))
        }
        request.accepted = true
        const position = Qt.point(request.position.x, request.position.y)
        const committedUrl = view.mainFrame.url.toString()
        // Standard actions, including Inspect element, must be available without
        // waiting for the optional password-field JavaScript probe.
        popup(view, position)
        if (request.isContentEditable && passwordsEnabled) {
            // Copy everything needed before returning: WebEngine owns request.
            // The isolated inspector exists only in secure HTTPS main frames;
            // native code validates the committed origin again on selection.
            const x = position.x / view.zoomFactor
            const y = position.y / view.zoomFactor
            view.mainFrame.runJavaScript("globalThis.__arbiterPasswords?.inspectPasswordAt(" + x + "," + y + ") || null", 1, function(field) {
                if (!contextMenu.visible || generation !== contextMenu.requestGeneration || contextMenu.view.mainFrame.url.toString() !== committedUrl) return
                if (field && contextMenu.passwordsEnabled)
                    contextMenu.insertItem(passwordIndex, generatePasswordItem.createObject(contextMenu.contentItem, {field: field}))
            })
        }
    }
}
