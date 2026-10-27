pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic as Controls
import Arbiter
import QtWebEngine

UiMenu {
    id: contextMenu
    objectName: "webContextMenu"
    preferredWidth: 320
    padding: 8
    property var shortcutHints: ({})
    required property WebEngineView view
    property bool passwordsEnabled: true
    signal inspectRequested()
    signal printRequested()
    property bool siteMuted: false
    property bool canMuteSite: false
    signal muteSiteRequested()
    signal passwordsRequested()
    signal passwordSuggestionRequested(var field)
    property int requestGeneration: 0

    function standardHint(command: int): string {
        const mac = Qt.platform.os === "osx"
        const primary = mac ? "⌘" : "Ctrl+"
        switch (command) {
        case WebEngineView.Undo: return primary + "Z"
        case WebEngineView.Redo: return mac ? "⇧⌘Z" : Qt.platform.os === "windows" ? "Ctrl+Y" : "Ctrl+Shift+Z"
        case WebEngineView.Cut: return primary + "X"
        case WebEngineView.Copy: return primary + "C"
        case WebEngineView.Paste: return primary + "V"
        case WebEngineView.PasteAndMatchStyle: return mac ? "⇧⌘V" : "Ctrl+Shift+V"
        case WebEngineView.SelectAll: return primary + "A"
        default: return ""
        }
    }
    Component {
        id: quickActionsItem
        Controls.MenuItem {
            id: strip
            objectName: "contextQuickActions"
            required property var actions
            property int selectedAction: 0
            implicitHeight: 64
            horizontalPadding: 4
            verticalPadding: 4
            text: qsTr("Quick actions")
            Accessible.role: Accessible.ToolBar
            enabled: actions.some(entry => entry.allowed && entry.action.enabled)
            function moveSelection(step: int) {
                for (let offset=1;offset<=actions.length;offset++) {
                    const index=(selectedAction+step*offset+actions.length)%actions.length
                    if(actions[index].allowed && actions[index].action.enabled){selectedAction=index;return}
                }
            }
            function activate(index: int) {
                const entry=actions[index]
                if (!entry || !entry.allowed || !entry.action.enabled) return
                contextMenu.close()
                entry.action.trigger()
            }
            onHighlightedChanged: if (highlighted && (!actions[selectedAction].allowed || !actions[selectedAction].action.enabled)) moveSelection(1)
            Keys.onLeftPressed: moveSelection(-1)
            Keys.onRightPressed: moveSelection(1)
            // Only real toolbar buttons or explicit keyboard activation run an
            // action; clicking the gaps must not accidentally reload the page.
            Keys.onReturnPressed: activate(selectedAction)
            Keys.onEnterPressed: activate(selectedAction)
            Keys.onSpacePressed: activate(selectedAction)
            background: Rectangle { radius: 12; color: Theme.field }
            contentItem: RowLayout {
                spacing: 4
                Item { Layout.fillWidth: true }
                Repeater {
                    model: strip.actions
                    delegate: UiButton {
                        id: quickButton
                        required property var modelData
                        required property int index
                        objectName: "contextQuickAction-" + modelData.command
                        Layout.fillWidth: true
                        Layout.preferredWidth: Math.max(80, quickCaption.implicitWidth + 16)
                        Layout.maximumWidth: 140
                        Layout.fillHeight: true
                        padding: 5
                        flat: true
                        highlighted: strip.highlighted && strip.selectedAction === index
                        enabled: modelData.allowed && modelData.action.enabled
                        Accessible.name: modelData.action.text
                        onHoveredChanged: if (hovered) strip.selectedAction = index
                        onClicked: strip.activate(index)
                        contentItem: ColumnLayout {
                            spacing: 4
                            UiIcon { name: quickButton.modelData.icon; color: Theme.text; Layout.preferredWidth: 19; Layout.preferredHeight: 19; Layout.alignment: Qt.AlignHCenter }
                            Controls.Label { id: quickCaption; text: quickButton.modelData.label; textFormat: Text.PlainText; color: Theme.text; font.pixelSize: 11; Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter; elide: Text.ElideRight }
                        }
                        UiToolTip { visible: quickButton.hovered; text: quickButton.modelData.action.text + (quickButton.modelData.hint ? " (" + quickButton.modelData.hint + ")" : "") }
                    }
                }
                Item { Layout.fillWidth: true }
            }
        }
    }
    Component {
        id: actionItem
        UiMenuItem {
            required property var webAction
            property bool contextEnabled: true
            reserveIconSpace: true
            text: webAction.text
            enabled: contextEnabled && webAction.enabled
            onTriggered: webAction.trigger()
        }
    }
    Component {
        id: suggestionItem
        UiMenuItem {
            iconName: "check"
            reserveIconSpace: true
            onTriggered: contextMenu.view.replaceMisspelledWord(text)
        }
    }
    Component {
        id: separatorItem
        Controls.MenuSeparator {
            objectName: "contextSeparator"
            topPadding: 5; bottomPadding: 5
            leftPadding: 8; rightPadding: 8
            contentItem: Rectangle { implicitHeight: 1; color: Theme.border }
        }
    }
    Component {
        id: extensionItem
        UiMenuItem {
            iconName: "puzzle"
            reserveIconSpace: true
            required property string extensionKey
            required property var menuContext
            onTriggered: contextMenu.view.invokeExtensionMenu(extensionKey, menuContext)
        }
    }
    Component {
        id: passwordsItem
        UiMenuItem {
            iconName: "key-round"
            reserveIconSpace: true
            shortcutText: contextMenu.shortcutHints.passwords || ""
            text: qsTr("Saved passwords…")
            enabled: contextMenu.passwordsEnabled && contextMenu.view.url.toString().startsWith("https://")
            onTriggered: contextMenu.passwordsRequested()
        }
    }
    Component {
        id: printItem
        UiMenuItem {
            objectName: "printPageAction"
            iconName: "printer"
            reserveIconSpace: true
            shortcutText: contextMenu.shortcutHints.print || ""
            text: qsTr("Print…")
            onTriggered: contextMenu.printRequested()
        }
    }
    Component {
        id: muteSiteItem
        UiMenuItem {
            objectName: "muteSiteAction"
            iconName: contextMenu.siteMuted ? "volume-x" : "volume-2"
            reserveIconSpace: true
            text: contextMenu.siteMuted ? qsTr("Unmute site") : qsTr("Mute site")
            onTriggered: contextMenu.muteSiteRequested()
        }
    }
    Connections { target: contextMenu.view; function onUrlChanged() { contextMenu.close() } }
    Component {
        id: inspectItem
        UiMenuItem {
            objectName: "inspectElementAction"
            iconName: "code-xml"
            reserveIconSpace: true
            shortcutText: contextMenu.shortcutHints.inspect || ""
            text: qsTr("Inspect element")
            onTriggered: contextMenu.inspectRequested()
        }
    }
    Component {
        id: generatePasswordItem
        UiMenuItem {
            required property var field
            objectName: "suggestPasswordAction"
            iconName: "key-round"
            reserveIconSpace: true
            text: qsTr("Suggest password")
            onTriggered: contextMenu.passwordSuggestionRequested(field)
        }
    }

    function addWebAction(command: int, icon: string, allowed = true) {
        addItem(actionItem.createObject(contextMenu.contentItem, {
            webAction: view.action(command), contextEnabled: Boolean(allowed),
            iconName: icon, shortcutText: standardHint(command)
        }))
    }
    function quickAction(command: int, icon: string, label: string, allowed = true) {
        return {command: command, action: view.action(command), icon: icon, label: label,
            allowed: Boolean(allowed), hint: standardHint(command)}
    }
    function separator() {
        if (count > 0 && itemAt(count-1).objectName !== "contextSeparator")
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
        let quick = []
        if (request.isContentEditable) {
            quick = [quickAction(WebEngineView.Cut,"scissors",qsTr("Cut"),request.editFlags & ContextMenuRequest.CanCut),
                quickAction(WebEngineView.Copy,"copy",qsTr("Copy"),request.editFlags & ContextMenuRequest.CanCopy),
                quickAction(WebEngineView.Paste,"clipboard-paste",qsTr("Paste"),request.editFlags & ContextMenuRequest.CanPaste)]
        } else if (request.selectedText.length > 0) {
            quick = [quickAction(WebEngineView.Copy,"copy",qsTr("Copy"),request.editFlags & ContextMenuRequest.CanCopy)]
        } else if (request.mediaType === ContextMenuRequest.MediaTypeImage || request.mediaType === ContextMenuRequest.MediaTypeCanvas) {
            quick = [quickAction(WebEngineView.CopyImageToClipboard,"copy",qsTr("Copy image"))]
            if (request.mediaUrl.toString().length > 0) quick.push(quickAction(WebEngineView.DownloadImageToDisk,"download",qsTr("Save image")))
        } else if (request.mediaType === ContextMenuRequest.MediaTypeVideo || request.mediaType === ContextMenuRequest.MediaTypeAudio) {
            quick = [quickAction(WebEngineView.ToggleMediaPlayPause,request.mediaFlags & ContextMenuRequest.MediaPaused ? "play" : "pause",request.mediaFlags & ContextMenuRequest.MediaPaused ? qsTr("Play") : qsTr("Pause"))]
            if (request.mediaFlags & ContextMenuRequest.MediaHasAudio) quick.push(quickAction(WebEngineView.ToggleMediaMute,request.mediaFlags & ContextMenuRequest.MediaMuted ? "volume-x" : "volume-2",request.mediaFlags & ContextMenuRequest.MediaMuted ? qsTr("Unmute") : qsTr("Mute")))
        } else {
            quick = [quickAction(WebEngineView.Back,"arrow-left",qsTr("Back")),
                quickAction(WebEngineView.Forward,"arrow-right",qsTr("Forward")),
                quickAction(WebEngineView.Reload,"rotate-cw",qsTr("Reload"))]
        }
        addItem(quickActionsItem.createObject(contextMenu.contentItem,{actions: quick}))
        separator()
        if (request.isContentEditable) {
            if (request.misspelledWord.length > 0) {
                for (const suggestion of request.spellCheckerSuggestions)
                    addItem(suggestionItem.createObject(contextMenu.contentItem, { text: suggestion }))
                separator()
            }
            addWebAction(WebEngineView.Undo,"undo-2",request.editFlags & ContextMenuRequest.CanUndo)
            addWebAction(WebEngineView.Redo,"redo-2",request.editFlags & ContextMenuRequest.CanRedo)
            addWebAction(WebEngineView.PasteAndMatchStyle,"clipboard-paste",request.editFlags & ContextMenuRequest.CanPaste)
            addWebAction(WebEngineView.SelectAll,"text-cursor",request.editFlags & ContextMenuRequest.CanSelectAll)
        }
        if (request.linkUrl.toString().length > 0) {
            separator()
            addWebAction(WebEngineView.OpenLinkInNewTab,"external-link")
            addWebAction(WebEngineView.OpenLinkInNewBackgroundTab,"panels-top-left")
            addWebAction(WebEngineView.CopyLinkToClipboard,"link")
        }
        if (request.mediaType === ContextMenuRequest.MediaTypeImage
                || request.mediaType === ContextMenuRequest.MediaTypeCanvas) {
            separator()
            if (request.mediaUrl.toString().length > 0)
                addWebAction(WebEngineView.CopyImageUrlToClipboard,"link")
        } else if (request.mediaType === ContextMenuRequest.MediaTypeVideo
                   || request.mediaType === ContextMenuRequest.MediaTypeAudio) {
            separator()
            addWebAction(WebEngineView.ToggleMediaLoop,"repeat")
            if (request.mediaFlags & ContextMenuRequest.MediaCanToggleControls)
                addWebAction(WebEngineView.ToggleMediaControls,"sliders-horizontal")
            if (request.mediaUrl.toString().length > 0)
                addWebAction(WebEngineView.CopyMediaUrlToClipboard,"link")
        }
        separator()
        if (canMuteSite) addItem(muteSiteItem.createObject(contextMenu.contentItem))
        addItem(printItem.createObject(contextMenu.contentItem))
        addItem(passwordsItem.createObject(contextMenu.contentItem))
        const passwordIndex = count - 1
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
        const extensionItems = typeof view.extensionMenuItems === "function"
            ? view.extensionMenuItems(menuContext) : []
        if (extensionItems.length) separator()
        for (const entry of extensionItems) {
            if (entry.separator) separator()
            else addItem(extensionItem.createObject(contextMenu.contentItem, {
                extensionKey: entry.key, menuContext: menuContext, text: entry.title,
                enabled: entry.enabled, checkable: entry.checkable, checked: entry.checked
            }))
        }
        separator()
        addItem(inspectItem.createObject(contextMenu.contentItem))
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
