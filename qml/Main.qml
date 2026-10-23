pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Effects
import QtQuick.Layouts
import QtQuick.Controls.Basic as Basic
import QtWebEngine
import QtWebChannel
import Arbiter.Credentials 1.0

Basic.ApplicationWindow {
    id: window
    required property string browserUserAgent
    required property bool browserDiagnostics
    readonly property var splits: workspaces.splits
    property int splitRevision: 0
    required property var workspaces
    required property var webProfiles
    required property var tabs
    required property var history
    required property var appearance
    required property var siteNavigation
    property int siteNavigationGeneration: 0
    readonly property bool siteNavigationEnabled: Qt.platform.os === "osx" && appearance.siteMenuMode !== "off" && !settingsOpen
    readonly property var homeAddress: homeStart.addressField
    property var webView: null
    property bool changingTabs: false
    property bool shuttingDown: false
    required property var keyBindings
    required property var passwords
    required property var passkeys
    required property var webauthn
    readonly property var bindings: keyBindings.bindings
    property bool settingsOpen: false
    property int settingsCategory: 0
    property int recordingIndex: -1
    property string bindingError: ""
    signal shortcutRecordingChanged(bool recording)
    onRecordingIndexChanged: shortcutRecordingChanged(recordingIndex >= 0)
    width: 1280
    height: 820
    minimumWidth: 640
    minimumHeight: 480
    visible: true
    flags: Qt.Window | Qt.ExpandedClientAreaHint | Qt.NoTitleBarBackgroundHint
    topPadding: 0
    title: settingsOpen ? qsTr("Settings — Arbiter") : !emptyPage && webView && webView.title ? webView.title + " — Arbiter" : "Arbiter"
    color: palette.window
    palette.window: Theme.window
    palette.base: Theme.window
    palette.alternateBase: Theme.field
    palette.text: Theme.text
    palette.windowText: Theme.text
    palette.button: Theme.field
    palette.buttonText: Theme.text
    palette.placeholderText: Theme.muted
    palette.highlight: Theme.selection
    palette.highlightedText: Theme.text
    palette.mid: Theme.border
    readonly property color subtleLine: Theme.border
    readonly property color mutedText: Theme.muted
    readonly property color fieldFill: Theme.field
    readonly property color accent: Theme.accent
    Binding { target: Theme; property: "dark"; value: window.appearance.dark }
    Binding { target: Theme; property: "lightAccent"; value: window.appearance.lightAccent }
    Binding { target: Theme; property: "darkAccent"; value: window.appearance.darkAccent }
    Binding { target: Theme; property: "lightBackground"; value: window.appearance.lightBackground }
    Binding { target: Theme; property: "darkBackground"; value: window.appearance.darkBackground }
    Binding { target: Theme; property: "backdrop"; value: !window.emptyPage && !window.settingsOpen ? window.webView : null }

    readonly property bool emptyPage: !webView || webView.url.toString() === "about:blank"
    readonly property bool pageNavigationEnabled: webView !== null && webView.activeFocus && !settingsOpen && recordingIndex < 0 && !changingTabs && !shuttingDown

    function resetSiteNavigation() {
        ++siteNavigationGeneration
        siteNavigation.reset(webView ? webView.url : "", webView ? webView.title : "",
                             siteNavigationEnabled ? appearance.siteMenuMode : "off")
        refreshSiteNavigation()
    }
    function refreshSiteNavigation() {
        if (!siteNavigationEnabled || !webView || webView.loading || emptyPage) return
        const view = webView
        const generation = siteNavigationGeneration
        const url = view.url.toString()
        view.mainFrame.runJavaScript(siteNavigation.extractionScript, 1, function(snapshot) {
            if (generation === window.siteNavigationGeneration && view === window.webView
                    && url === view.url.toString() && snapshot)
                window.siteNavigation.update(url, snapshot)
        })
    }
    onWebViewChanged: resetSiteNavigation()
    onSiteNavigationEnabledChanged: resetSiteNavigation()
    Connections {
        target: window.appearance
        function onSiteMenuModeChanged() { window.resetSiteNavigation() }
    }
    Connections {
        target: window.siteNavigation
        function onLinkActivated(url: url) {
            if (!window.siteNavigationEnabled || !window.webView) return
            window.webView.url = url
            window.focusPage()
        }
    }
    Timer {
        interval: 2000
        repeat: true
        running: window.siteNavigationEnabled && !window.emptyPage
        onTriggered: window.refreshSiteNavigation()
    }

    function prepareForShutdown() {
        shuttingDown = true
        biometricSetup.close()
        workspacePanel.close()
        historyPanel.close()
        addressPopup.close()
        tabsPopup.close()
        if (webView) {
            webAuthnPanel.cancelFor(webView)
            passwordPanel.cancelFor(webView)
        }
        for (let i = 0; i < tabViews.count; ++i) {
            const view = tabViews.itemAt(i) as WebEngineView
            if (view) { view.startPage.focus = false; view.focus = false }
        }
        webView = null
        contentItem.forceActiveFocus()
        // Release views while their profile and the window's focus delivery
        // agent are still alive, before ApplicationWindow starts destruction.
        tabViews.model = null
    }

    function focusAddress() {
        workspacePanel.close()
        historyPanel.close()
        if (emptyPage && !settingsOpen && webView) {
            if (splits.multiple) webView.startPage.focusAddress(true)
            else homeStart.focusAddress(true)
        } else {
            if (!addressPopup.visible) {
                tabsPopup.close()
                popupAddress.text = settingsOpen ? "arbiter://settings" : webView.url.toString()
                addressPopup.open()
            } else {
                popupAddress.forceActiveFocus()
                popupAddress.selectAll()
            }
        }
    }

    function navigate(input: string) {
        workspacePanel.close()
        if (!webView)
            return
        const text = input.trim()
        if (!text)
            return
        addressPopup.close()
        historyPanel.close()
        if (text.toLowerCase() === "arbiter://settings") {
            window.settingsOpen = true
            return
        }
        window.settingsOpen = false
        if (/^(https?:\/\/|file:\/\/|about:)/i.test(text))
            webView.url = text
        else if (!/\s/.test(text) && (text.includes(".") || /^localhost(:\d+)?(\/|$)/i.test(text)))
            webView.url = (/^localhost(:|\/|$)/i.test(text) ? "http://" : "https://") + text
        else
            webView.url = "https://duckduckgo.com/?q=" + encodeURIComponent(text)
        webView.forceActiveFocus()
    }

    Shortcut { sequence: window.bindings.address; enabled: window.recordingIndex < 0; onActivated: window.focusAddress() }
    Shortcut { sequence: window.bindings.workspaces; enabled: window.recordingIndex < 0; onActivated: window.openWorkspaces() }
    Shortcut { sequence: window.bindings.nextWorkspace; enabled: window.recordingIndex < 0; onActivated: window.workspaces.cycle(1) }
    Shortcut { sequence: window.bindings.previousWorkspace; enabled: window.recordingIndex < 0; onActivated: window.workspaces.cycle(-1) }
    Shortcut { sequence: window.bindings.history; enabled: window.recordingIndex < 0; onActivated: window.openHistory() }
    Shortcut { sequence: window.bindings.passwords; enabled: window.recordingIndex < 0 && !window.settingsOpen; onActivated: window.openPasswords() }
    Shortcut { sequence: window.bindings.reload; enabled: window.webView && window.recordingIndex < 0 && !window.settingsOpen; onActivated: window.webView.reload() }
    Shortcut { sequence: window.bindings.back; enabled: window.pageNavigationEnabled; onActivated: window.webView.goBack() }
    Shortcut { sequence: window.bindings.forward; enabled: window.pageNavigationEnabled; onActivated: window.webView.goForward() }
    // WebEngine's ShortcutOverride preserves native cursor movement in inputs,
    // textareas and contenteditable (including child frames). Only page focus
    // enables scrolling; address fields and other browser controls keep their keys.
    Shortcut { sequence: window.bindings.pageTop; enabled: window.pageNavigationEnabled; onActivated: window.scrollPage(false) }
    Shortcut { sequence: window.bindings.pageBottom; enabled: window.pageNavigationEnabled; onActivated: window.scrollPage(true) }
    Shortcut {
        sequence: window.bindings.stop
        enabled: !addressPopup.visible && !tabsPopup.visible && !historyPanel.visible && !workspacePanel.visible && window.recordingIndex < 0 && !window.settingsOpen
        onActivated: { window.webView.stop(); window.webView.forceActiveFocus() }
    }
    Shortcut { sequence: window.bindings.close; enabled: window.recordingIndex < 0; onActivated: window.closeCurrentTab() }
    Shortcut { sequence: window.bindings.fullscreen; enabled: window.recordingIndex < 0; onActivated: window.toggleFullScreen() }
    Shortcut { sequence: window.bindings.expand; enabled: window.recordingIndex < 0; onActivated: window.toggleExpanded() }

    Shortcut { sequence: window.bindings.newTab; enabled: window.recordingIndex < 0; onActivated: window.newTab() }
    Shortcut { sequence: window.bindings.tabs; enabled: window.recordingIndex < 0; onActivated: window.showTabs() }
    Shortcut { sequence: window.bindings.nextTab; enabled: window.recordingIndex < 0; onActivated: window.nextTab() }
    Shortcut { sequence: window.bindings.previousTab; enabled: window.recordingIndex < 0; onActivated: window.previousTab() }
    Shortcut { sequence: window.bindings.reopenTab; enabled: window.recordingIndex < 0; onActivated: window.reopenTab() }
    Shortcut { sequence: window.bindings.splitHorizontal; enabled: window.recordingIndex < 0; onActivated: window.splitHorizontal() }
    Shortcut { sequence: window.bindings.splitVertical; enabled: window.recordingIndex < 0; onActivated: window.splitVertical() }
    Shortcut { sequence: window.bindings.nextPane; enabled: window.recordingIndex < 0; onActivated: window.nextPane() }
    Shortcut { sequence: window.bindings.previousPane; enabled: window.recordingIndex < 0; onActivated: window.previousPane() }
    Shortcut { sequence: window.bindings.nextHiddenTab; enabled: window.recordingIndex < 0; onActivated: window.nextHiddenTab() }
    Shortcut { sequence: window.bindings.closeWindow; enabled: window.recordingIndex < 0; onActivated: window.close() }

    function scrollPage(bottom: bool) {
        if (!pageNavigationEnabled) return
        webView.mainFrame.runJavaScript(bottom
            ? "window.scrollTo({left:window.scrollX,top:document.scrollingElement.scrollHeight,behavior:'instant'})"
            : "window.scrollTo({left:window.scrollX,top:0,behavior:'instant'})", 1)
    }

    function focusPage() {
        if (workspacePanel.visible)
            workspacePanel.focusSearch()
        else if (historyPanel.visible)
            historyPanel.focusSearch()
        else if (tabsPopup.visible)
            tabSearch.forceActiveFocus()
        else if (addressPopup.visible)
            popupAddress.forceActiveFocus()
        else if (settingsOpen)
            settingsPage.forceActiveFocus()
        else if (emptyPage) {
            if (splits.multiple && webView) webView.startPage.focusAddress(false)
            else homeStart.focusAddress(false)
        } else if (webView)
            webView.forceActiveFocus()
    }

    function syncActiveView() {
        if (changingTabs || shuttingDown) return
        if (webView) webAuthnPanel.cancelFor(webView)
        if (webView) passwordPanel.cancelFor(webView)
        webView = tabViews.itemAt(tabs.currentIndex)
        homeAddress.text = ""
        Qt.callLater(revealActivePane)
        Qt.callLater(focusPage)
    }

    function selectTab(index: int) {
        workspacePanel.close()
        historyPanel.close()
        addressPopup.close()
        tabsPopup.close()
        settingsOpen = false
        recordingIndex = -1
        tabs.currentIndex = index
        syncActiveView()
    }

    function splitHorizontal() { closeSplitPopups(); splits.split(true) }
    function splitVertical() { closeSplitPopups(); splits.split(false) }
    function closeSplitPopups() {
        settingsOpen = false
        recordingIndex = -1
        tabsPopup.close()
        addressPopup.close()
        historyPanel.close()
        workspacePanel.close()
    }
    function revealActivePane() {
        if (!webView || !splits.multiple) return
        // A split changes the viewport inset. Publish its settled size before
        // calculating focus scrolling, and never scroll outside actual bounds.
        splitViewport.updateViewport()
        const p = splits.geometry(webView.tabId)
        if (p.paneId === undefined) return
        let x = Math.max(0, Math.min(splitViewport.contentX, p.x))
        let y = Math.max(0, Math.min(splitViewport.contentY, p.y))
        if (p.x + p.width > x + splitViewport.width)
            x = p.x + Math.min(p.width, splitViewport.width) - splitViewport.width
        if (p.y + p.height > y + splitViewport.height)
            y = p.y + Math.min(p.height, splitViewport.height) - splitViewport.height
        splitViewport.contentX = Math.max(0, Math.min(x, splitViewport.contentWidth - splitViewport.width))
        splitViewport.contentY = Math.max(0, Math.min(y, splitViewport.contentHeight - splitViewport.height))
    }
    function singlePane() { splits.single(); focusPage() }
    function nextPane() { splits.cycle(1); focusPage() }
    function previousPane() { splits.cycle(-1); focusPage() }
    function nextHiddenTab() { if (splits.nextHiddenTab()) selectTab(tabs.currentIndex) }

    Connections {
        target: window.splits
        function onChanged() { ++window.splitRevision }
    }

    function newTab() {
        selectTab(tabs.add("about:blank"))
    }

    function prepareTabRemoval(first: int, last: int) {
        changingTabs = true
        for (let i = first; i <= last; ++i) {
            const view = tabViews.itemAt(i) as WebEngineView
            if (!view) continue
            webAuthnPanel.cancelFor(view)
            passwordPanel.cancelFor(view)
            // Repeater detaches delegates before their deferred destruction.
            // Clear the WebEngine focus scope while its parent is still alive.
            view.startPage.focus = false
            view.focus = false
            if (view === webView) {
                webView = null
                contentItem.forceActiveFocus()
            }
        }
    }
    function finishTabRemoval() {
        changingTabs = false
        syncActiveView()
    }

    function closeCurrentTab() {
        closeTab(tabs.currentIndex)
    }

    function closeTab(index: int) {
        const view = tabViews.itemAt(index) as WebEngineView
        if (!view || view.closeQueued)
            return
        historyPanel.close()
        // Let Chromium run beforeunload and show its confirmation before
        // destroying a page with unsaved work.
        addressPopup.close()
        tabsPopup.close()
        view.triggerWebAction(WebEngineView.RequestClose)
    }

    function finishClosingTab(tabId: int) {
        // Rows can change while Chromium finishes beforeunload or another tab
        // closes. Resolve the stable ID only after the request callback returns.
        let index = -1
        for (let i = 0; i < tabs.count; ++i)
            if (tabs.idAt(i) === tabId) { index = i; break }
        if (index < 0 || shuttingDown) return
        if (tabs.workspaceCount === 1 && workspaces.count === 1) {
            tabs.close(index)
            tabs.add("about:blank")
            window.close()
            return
        }
        addressPopup.close()
        tabs.close(index)
        if (tabs.workspaceCount === 0) tabs.add("about:blank")
        syncActiveView()
        if (tabsPopup.visible)
            tabSearch.forceActiveFocus()
    }

    function nextTab() { tabs.cycle(1); selectTab(tabs.currentIndex) }
    function previousTab() { tabs.cycle(-1); selectTab(tabs.currentIndex) }
    function reopenTab() { if (tabs.reopen()) selectTab(tabs.currentIndex) }
    function showTabs() { workspacePanel.close(); historyPanel.close(); addressPopup.close(); tabsPopup.open() }

    function openExternalUrl(url: string) {
        syncActiveView()
        if (emptyPage && !settingsOpen) {
            navigate(url)
        } else {
            selectTab(tabs.add(url))
        }
    }

    function openWebRequest(request: var, sourceWorkspace: string) {
        // Preserve background-tab requests and keep the request's opener and
        // pending navigation by handing it directly to the new engine view.
        const background = request.destination === WebEngineView.NewViewInBackgroundTab
        const activate = !background && sourceWorkspace === workspaces.currentId
        const index = workspaces.addTab(sourceWorkspace, "about:blank", activate)
        const view = tabViews.itemAt(index) as WebEngineView
        if (view)
            view.acceptAsNewWindow(request)
        if (activate)
            selectTab(index)
    }

    Connections {
        target: window.tabs
        function onCurrentIndexChanged() { window.syncActiveView() }
        function onRowsAboutToBeRemoved(parent, first, last) { window.prepareTabRemoval(first, last) }
        function onRowsRemoved() { Qt.callLater(window.finishTabRemoval) }
        function onModelAboutToBeReset() { window.prepareTabRemoval(0, window.tabs.count - 1) }
        function onModelReset() { Qt.callLater(window.finishTabRemoval) }
    }
    Connections {
        target: window.workspaces
        function onCurrentChanged() {
            window.settingsOpen = false
            window.recordingIndex = -1
            addressPopup.close()
            historyPanel.close()
            tabsPopup.close()
            tabSearch.text = ""
            Qt.callLater(window.focusPage)
        }
    }

    function toggleSettings() {
        workspacePanel.close()
        historyPanel.close()
        if (webView) webAuthnPanel.cancelFor(webView)
        addressPopup.close()
        tabsPopup.close()
        recordingIndex = -1
        bindingError = ""
        settingsOpen = !settingsOpen
        if (settingsOpen)
            settingsPage.forceActiveFocus()
        else
            focusPage()
    }

    function openSettings() {
        if (!settingsOpen)
            toggleSettings()
    }

    function openPasswords() {
        historyPanel.close()
        addressPopup.close()
        tabsPopup.close()
        if (!settingsOpen) passwordPanel.openFor(webView)
    }

    function openPasswordSettings() { historyPanel.close(); passwordPanel.openManager() }
    function openWorkspaces() {
        if (webView) {
            webAuthnPanel.cancelFor(webView)
            passwordPanel.cancelFor(webView)
        }
        addressPopup.close()
        tabsPopup.close()
        historyPanel.close()
        workspacePanel.open()
    }
    function openHistory() {
        workspacePanel.close()
        if (webView) {
            webAuthnPanel.cancelFor(webView)
            passwordPanel.cancelFor(webView)
        }
        addressPopup.close()
        tabsPopup.close()
        historyPanel.open()
    }

    function toggleExpanded() {
        if (visibility === Window.Maximized || visibility === Window.FullScreen)
            showNormal()
        else
            showMaximized()
    }

    function toggleFullScreen() {
        if (visibility === Window.FullScreen)
            showNormal()
        else
            showFullScreen()
    }


    Flickable {
        id: splitViewport
        objectName: "splitViewport"
        anchors.fill: parent
        // Half the divider width keeps the complete pane stroke inside the
        // native window curve, with the same spacing around the whole layout.
        anchors.margins: window.splits.multiple ? 4 : 0
        visible: !window.settingsOpen
        clip: true
        contentWidth: Math.max(width, window.splits.minimumWidth)
        contentHeight: Math.max(height, window.splits.minimumHeight)
        // Scrolling the canvas must not steal gestures from websites.
        interactive: false
        Basic.ScrollBar.horizontal: Basic.ScrollBar { policy: splitViewport.contentWidth > splitViewport.width ? Basic.ScrollBar.AlwaysOn : Basic.ScrollBar.AlwaysOff }
        Basic.ScrollBar.vertical: Basic.ScrollBar { policy: splitViewport.contentHeight > splitViewport.height ? Basic.ScrollBar.AlwaysOn : Basic.ScrollBar.AlwaysOff }
        function clampOffsets() {
            contentX = Math.max(0, Math.min(contentX, contentWidth - width))
            contentY = Math.max(0, Math.min(contentY, contentHeight - height))
        }
        function updateViewport() {
            window.splits.setViewport(width, height)
            clampOffsets()
        }
        onContentWidthChanged: Qt.callLater(splitViewport.clampOffsets)
        onContentHeightChanged: Qt.callLater(splitViewport.clampOffsets)
        // Wait for anchor margins to settle before publishing new geometry.
        onWidthChanged: Qt.callLater(splitViewport.updateViewport)
        onHeightChanged: Qt.callLater(splitViewport.updateViewport)
        Component.onCompleted: updateViewport()
        Item {
            id: paneCanvas
            objectName: "paneCanvas"
            width: splitViewport.contentWidth
            height: splitViewport.contentHeight
            Repeater {
                id: tabViews
                model: window.tabs
                onItemAdded: Qt.callLater(window.syncActiveView)
                onItemRemoved: Qt.callLater(window.syncActiveView)
                delegate: WebEngineView {
                    id: page
                    required property int index
                    required property string workspaceId
                    required property string profileId
                    required property int tabId
                    required property url initialUrl
                    property bool closeQueued: false
                    property string errorText: ""
                    property bool historyReady: false
                    property string historyVisitId: ""
                    property string historyUrl: ""
                    function recordHistory() {
                        // url may expose a pending destination before LoadStarted.
                        // Only save the committed document, including same-document changes.
                        const committed = mainFrame.url.toString()
                        if (!historyReady || loading || profile.offTheRecord || url.toString() !== committed || committed === historyUrl) return
                        historyUrl = committed
                        historyVisitId = window.history.record(mainFrame.url, title, profile.offTheRecord)
                    }
                    readonly property var passwordAgent: passwordSession
                    readonly property var startPage: paneStart
                    StartPage {
                        id: paneStart
                        objectName: "paneStart" + page.tabId
                        // A stable sibling of the engine item, owned by this tab:
                        // an invisible about:blank view must not hide its start page.
                        parent: paneCanvas
                        x: page.x
                        y: page.y
                        width: page.width
                        height: page.height
                        z: 1
                        visible: page.inSplit && window.splits.multiple && !window.settingsOpen && page.url.toString() === "about:blank"
                        addressShortcut: window.keyBindings.nativeSequence(window.bindings.address)
                        tabsShortcut: window.keyBindings.nativeSequence(window.bindings.tabs)
                        onActiveFocusChanged: if (activeFocus && visible && !window.changingTabs && !window.shuttingDown) window.splits.activate(page.paneGeometry.paneId)
                        onNavigateRequested: function(text) { window.splits.activate(page.paneGeometry.paneId); window.navigate(text) }
                        onTabsRequested: { window.splits.activate(page.paneGeometry.paneId); window.showTabs() }
                        onSettingsRequested: window.openSettings()
                    }
                    readonly property var paneGeometry: {
                        const revision = window.splitRevision
                        return window.splits.geometry(tabId)
                    }
                    readonly property bool inSplit: paneGeometry.paneId !== undefined
                    x: inSplit ? paneGeometry.x : 0
                    y: inSplit ? paneGeometry.y : 0
                    width: inSplit ? Math.max(0, paneGeometry.width) : window.width
                    height: inSplit ? Math.max(0, paneGeometry.height) : window.height
                    onActiveFocusChanged: if (activeFocus && inSplit && !window.settingsOpen && !window.changingTabs && !window.shuttingDown) window.splits.activate(paneGeometry.paneId)
                    visible: (tabsPopup.visible && workspaceId === window.workspaces.currentId) || (inSplit && !window.settingsOpen && url.toString() !== "about:blank")
                    z: 1
                    layer.enabled: inSplit && window.splits.multiple
                    layer.effect: MultiEffect {
                        maskEnabled: true
                        maskSource: paneMask
                    }
                    PaneShape {
                        id: paneMask
                        objectName: "paneMask" + page.tabId
                        anchors.fill: parent
                        color: "white"
                        layer.enabled: true
                        visible: false
                    }
                    settings.focusOnNavigationEnabled: index === window.tabs.currentIndex && url.toString() !== "about:blank"
                    url: initialUrl
                    backgroundColor: window.color
                    profile: window.webProfiles.profile(profileId)
                    webChannel: passwordChannel
                    webChannelWorld: 1
                    userScripts.collection: [{
                        name: "Arbiter password forms",
                        injectionPoint: WebEngineScript.DocumentReady,
                        worldId: WebEngineScript.ApplicationWorld,
                        runsOnSubFrames: false,
                        sourceCode: window.passwords.captureScript
                    }]
                    WebChannel {
                        id: passwordChannel
                        Component.onCompleted: registerObject("passwordCapture", passwordSession.capture)
                    }
                    PasswordSession {
                        id: passwordSession
                        manager: window.passwords
                        onCaptureNeedsValidation: passwordSession.validateCapture(page.mainFrame.url, page.profile.offTheRecord)
                        onRunFillScript: function(script) {
                            if (page !== window.webView || window.settingsOpen || !window.passwords.enabled) return
                            page.mainFrame.runJavaScript(script, 1, function(filled) {
                                window.passwords.reportFillResult(filled === true)
                            })
                        }
                    }
                    WebContextMenu {
                        id: pageContextMenu
                        view: page
                        passwordsEnabled: window.passwords.enabled
                        onPasswordsRequested: Qt.callLater(window.openPasswords)
                    }
                    onContextMenuRequested: function(request) { pageContextMenu.show(request) }
                    onNewWindowRequested: function(request) { window.openWebRequest(request, workspaceId) }
                    onWindowCloseRequested: {
                        if (closeQueued) return
                        closeQueued = true
                        const closingId = tabId
                        Qt.callLater(function() { window.finishClosingTab(closingId) })
                    }
                    onWebAuthUxRequested: function(request) {
                        if (page !== window.webView || window.settingsOpen) request.cancel()
                        else webAuthnPanel.show(page, request)
                    }
                    Component.onDestruction: {
                        webAuthnPanel.cancelFor(page)
                        passwordPanel.cancelFor(page)
                    }
                    onTitleChanged: {
                        window.tabs.update(tabId, title, url)
                        if (!profile.offTheRecord && historyVisitId.length > 0)
                            window.history.updateTitle(historyVisitId, url, title)
                        if (page === window.webView) window.siteNavigation.setTitle(title)
                    }
                    onUrlChanged: {
                        Qt.callLater(page.recordHistory)
                        if (page === window.webView) window.resetSiteNavigation()
                        webAuthnPanel.cancelFor(page)
                        passwordSession.invalidate()
                        passwordPanel.cancelFor(page)
                        window.tabs.update(tabId, title, url)
                        if (page === window.webView && window.emptyPage) {
                            window.homeAddress.text = ""
                            Qt.callLater(window.focusPage)
                        }
                    }
                    onJavaScriptConsoleMessage: function(level, message, lineNumber, sourceID) {
                        if (window.browserDiagnostics)
                            console.log("Page console:", level, sourceID, lineNumber, message)
                    }
                    onLoadingChanged: function(request) {
                        if (request.status === WebEngineView.LoadStartedStatus) {
                            historyReady = false
                            historyVisitId = ""
                            historyUrl = ""
                            if (page === window.webView) window.resetSiteNavigation()
                            webAuthnPanel.cancelFor(page)
                            passwordSession.invalidate()
                            passwordPanel.cancelFor(page)
                        }
                        if (request.status === WebEngineView.LoadSucceededStatus) {
                            if (request.url.toString() === page.mainFrame.url.toString()) {
                                historyReady = true
                                recordHistory()
                            }
                        }
                        if (page === window.webView && request.status === WebEngineView.LoadSucceededStatus)
                            window.refreshSiteNavigation()
                        if (window.browserDiagnostics) {
                            console.log("Load:", request.url, request.status,
                                        request.errorDomain, request.errorCode, request.errorString)
                            if (request.status === WebEngineView.LoadSucceededStatus
                                    || request.status === WebEngineView.LoadFailedStatus) {
                                runJavaScript("JSON.stringify({userAgent:navigator.userAgent,secureContext:isSecureContext,workers:typeof Worker,webAssembly:typeof WebAssembly,crypto:typeof crypto.subtle,cookies:navigator.cookieEnabled,cores:navigator.hardwareConcurrency})",
                                              function(result) { console.log("Browser features:", result) })
                            }
                        }
                        if (request.status === WebEngineView.LoadFailedStatus)
                            errorText = request.isErrorPage ? request.errorString : ""
                        else if (request.status === WebEngineView.LoadStartedStatus
                                 || request.status === WebEngineView.LoadSucceededStatus)
                            errorText = ""
                    }
                }
            }

            SplitOverlay {
                z: 2
                id: splitOverlay
                objectName: "splitOverlay"
                anchors.fill: parent
                layout: window.splits
                enabled: !window.settingsOpen && (!Basic.Overlay.overlay || !Basic.Overlay.overlay.visible)
                onFocusRequested: window.focusPage()
            }
        }
    }

    component AddressField: UiTextField {
        implicitHeight: 46
        font.pixelSize: 15
        placeholderText: qsTr("Search or enter address")
        Accessible.name: qsTr("Address and search")
        onAccepted: window.navigate(text)
    }

    component ModalPanel: GlassSurface { radius: 18 }

    component ModalBackdrop: Rectangle {
        color: Theme.scrim
    }

    Rectangle {
        id: homePage
        anchors.fill: parent
        visible: window.emptyPage && !window.settingsOpen && !window.splits.multiple
        color: window.palette.window

        MouseArea {
            anchors.fill: parent
            onPressed: window.startSystemMove()
        }
        StartPage {
            id: homeStart
            anchors.fill: parent
            addressShortcut: window.keyBindings.nativeSequence(window.bindings.address)
            tabsShortcut: window.keyBindings.nativeSequence(window.bindings.tabs)
            onNavigateRequested: function(text) { window.navigate(text) }
            onTabsRequested: window.showTabs()
            onSettingsRequested: window.toggleSettings()
        }
    }

    Rectangle {
        id: settingsPage
        anchors.fill: parent
        visible: window.settingsOpen
        color: Theme.window
        z: 10
        RowLayout {
            anchors.fill: parent
            anchors.margins: window.width < 800 ? 24 : 40
            spacing: window.width < 800 ? 24 : 40
            ColumnLayout {
                Layout.preferredWidth: window.width < 800 ? 150 : 190
                Layout.fillHeight: true
                spacing: 8
                Basic.Label {
                    text: qsTr("Settings")
                    font.pixelSize: 26
                    font.weight: Font.DemiBold
                    color: Theme.text
                    Layout.leftMargin: 16
                    Layout.bottomMargin: 24
                }
                Repeater {
                    id: settingsNavigation
                    model: [qsTr("Appearance"), qsTr("Passwords"), qsTr("Passkeys"), qsTr("Keyboard")]
                    delegate: UiButton {
                        id: categoryButton
                        required property int index
                        required property string modelData
                        Layout.fillWidth: true
                        Layout.preferredHeight: 44
                        text: modelData
                        highlighted: window.settingsCategory === index
                        Accessible.role: Accessible.PageTab
                        Accessible.name: modelData
                        Accessible.selected: highlighted
                        contentItem: Basic.Label {
                            text: categoryButton.text
                            color: categoryButton.highlighted ? Theme.text : Theme.muted
                            font.pixelSize: 14
                            font.weight: categoryButton.highlighted ? Font.DemiBold : Font.Normal
                            verticalAlignment: Text.AlignVCenter
                        }
                        background: Rectangle {
                            radius: height / 2
                            color: categoryButton.highlighted ? Theme.selection : categoryButton.hovered || categoryButton.activeFocus ? Theme.hover : "transparent"
                        }
                        onClicked: {
                            window.recordingIndex = -1
                            window.bindingError = ""
                            window.settingsCategory = index
                        }
                        Keys.onDownPressed: settingsNavigation.itemAt(Math.min(3, index + 1)).forceActiveFocus()
                        Keys.onUpPressed: settingsNavigation.itemAt(Math.max(0, index - 1)).forceActiveFocus()
                    }
                }
                Item { Layout.fillHeight: true }
                UiButton { text: qsTr("Done"); Layout.fillWidth: true; onClicked: window.toggleSettings() }
            }
            Rectangle { Layout.preferredWidth: 1; Layout.fillHeight: true; color: Theme.border }
            Basic.ScrollView {
                id: settingsScroll
                Layout.fillWidth: true
                Layout.fillHeight: true
                contentWidth: availableWidth
                clip: true
                ColumnLayout {
                    width: Math.min(720, settingsScroll.availableWidth)
                    spacing: 24
                    Basic.Label {
                        text: [qsTr("Appearance"), qsTr("Passwords"), qsTr("Passkeys"), qsTr("Keyboard")][window.settingsCategory]
                        font.pixelSize: 28
                        font.weight: Font.DemiBold
                        color: Theme.text
                        Layout.bottomMargin: 4
                    }
                    ColumnLayout {
                        visible: window.settingsCategory === 0
                        Layout.fillWidth: true
                        spacing: 20
                        Basic.Label { text: qsTr("Make Arbiter feel at home."); color: Theme.muted; Layout.fillWidth: true; wrapMode: Text.Wrap }
                        RowLayout {
                            Layout.fillWidth: true
                            Basic.Label { text: qsTr("Color theme"); Layout.fillWidth: true; color: Theme.text }
                            UiComboBox {
                                id: themeChoice
                                objectName: "themeChoice"
                                model: [qsTr("System"), qsTr("Dark"), qsTr("Light")]
                                currentIndex: ["system", "dark", "light"].indexOf(window.appearance.theme)
                                Accessible.name: qsTr("Color theme")
                                onActivated: window.appearance.theme = ["system", "dark", "light"][currentIndex]
                            }
                        }
                        Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Theme.border }
                        UiColorPicker {
                            objectName: "lightAccentPicker"
                            Layout.fillWidth: true
                            title: qsTr("Light accent")
                            value: window.appearance.lightAccent
                            defaultColor: "#2868b8"
                            onColorSelected: color => window.appearance.lightAccent = color
                        }
                        UiColorPicker {
                            objectName: "darkAccentPicker"
                            Layout.fillWidth: true
                            title: qsTr("Dark accent")
                            value: window.appearance.darkAccent
                            defaultColor: "#8dbbff"
                            onColorSelected: color => window.appearance.darkAccent = color
                        }
                        UiColorPicker {
                            objectName: "lightBackgroundPicker"
                            Layout.fillWidth: true
                            title: qsTr("Light background")
                            value: window.appearance.lightBackground
                            defaultColor: "#edf2f7"
                            presets: ["#edf2f7", "#f7f3ed", "#f1f5ed", "#f4eff7", "#eef6f6", "#ffffff", "#e3e7ed", "#f4eeee"]
                            onColorSelected: color => window.appearance.lightBackground = color
                        }
                        UiColorPicker {
                            objectName: "darkBackgroundPicker"
                            Layout.fillWidth: true
                            title: qsTr("Dark background")
                            value: window.appearance.darkBackground
                            defaultColor: "#11161e"
                            presets: ["#11161e", "#19181d", "#181d19", "#1e1820", "#142023", "#121212", "#202530", "#211b18"]
                            onColorSelected: color => window.appearance.darkBackground = color
                        }
                        UiCheckBox {
                            objectName: "navigationPillToggle"
                            text: qsTr("Show navigation pill")
                            checked: window.appearance.navigationPill
                            onToggled: window.appearance.navigationPill = checked
                        }
                        Basic.Label {
                            text: qsTr("Navigation appears when you hover over the small handle. Keyboard shortcuts remain available when the pill is hidden.")
                            color: Theme.muted
                            Layout.fillWidth: true
                            wrapMode: Text.Wrap
                        }
                        Basic.Label {
                            visible: Qt.platform.os === "osx"
                            text: qsTr("Website navigation in the menu bar")
                            color: Theme.text
                            font.weight: Font.Medium
                        }
                        UiComboBox {
                            objectName: "siteMenuModeChoice"
                            visible: Qt.platform.os === "osx"
                            Layout.fillWidth: true
                            model: [qsTr("Tab title"), qsTr("Top-level navigation"), qsTr("Off")]
                            currentIndex: ["title", "navigation", "off"].indexOf(window.appearance.siteMenuMode)
                            Accessible.name: qsTr("Website navigation in the menu bar")
                            onActivated: window.appearance.siteMenuMode = ["title", "navigation", "off"][currentIndex]
                        }
                        Basic.Label {
                            visible: Qt.platform.os === "osx"
                            text: qsTr("Add the active website's navigation links to the macOS menu bar. Use its tab title as one menu, show navigation sections as separate menus, or turn this off.")
                            color: Theme.muted
                            Layout.fillWidth: true
                            wrapMode: Text.Wrap
                        }
                    }
                    ColumnLayout {
                        visible: window.settingsCategory === 1
                        Layout.fillWidth: true
                        spacing: 20
                        Basic.Label {
                            text: qsTr("Keep your saved logins on this computer, protected by your system credential store.")
                            color: Theme.muted
                            Layout.fillWidth: true
                            wrapMode: Text.Wrap
                        }
                        UiCheckBox {
                            text: qsTr("Offer to save and fill passwords")
                            checked: window.passwords.enabled
                            onToggled: window.passwords.enabled = checked
                        }
                        UiButton { text: qsTr("Manage saved passwords…"); onClicked: window.openPasswordSettings() }
                    }
                    ColumnLayout {
                        visible: window.settingsCategory === 2
                        Layout.fillWidth: true
                        spacing: 20
                        Basic.Label {
                            text: window.passkeys.enabled
                                ? qsTr("Use a passkey saved in Arbiter, or scan a QR code with your phone when a website asks you to sign in.")
                                : qsTr("Arbiter's passkey vault and phone QR flow require the patched WebEngine build.")
                            color: Theme.muted
                            Layout.fillWidth: true
                            wrapMode: Text.Wrap
                        }
                        UiButton {
                            text: window.passkeys.configured ? qsTr("Passkey vault is configured") : qsTr("Set up passkey vault…")
                            visible: window.passkeys.enabled
                            enabled: !window.passkeys.configured
                            onClicked: passkeySetup.open()
                        }
                        Basic.Label {
                            visible: window.passkeys.enabled && window.passkeys.configured
                            text: window.passkeys.biometricStatus
                            color: Theme.muted
                            Layout.fillWidth: true
                            wrapMode: Text.Wrap
                        }
                        UiButton {
                            visible: window.passkeys.enabled && window.passkeys.configured
                            text: window.passkeys.biometricEnabled ? qsTr("Disable biometric unlocking") : qsTr("Enable %1…").arg(window.passkeys.biometricName)
                            enabled: !window.passkeys.busy && (window.passkeys.biometricEnabled || window.passkeys.biometricAvailable)
                            onClicked: {
                                if (window.passkeys.biometricEnabled) window.passkeys.setBiometricEnabled(false, "")
                                else biometricSetup.open()
                            }
                        }
                        Basic.Label {
                            visible: window.passkeys.enabled && window.passkeys.configured
                            text: qsTr("Biometric unlocking lasts up to 15 minutes while Arbiter stays active. Enter your passphrase once after restarting or leaving the app; each later passkey use still asks for device verification.")
                            color: Theme.muted
                            Layout.fillWidth: true
                            wrapMode: Text.Wrap
                        }
                        RowLayout {
                            visible: window.passkeys.enabled && window.passkeys.configured
                            UiButton { text: qsTr("Refresh biometric support"); enabled: !window.passkeys.busy; onClicked: window.passkeys.refreshBiometrics() }
                            UiButton { text: qsTr("Lock now"); enabled: !window.passkeys.busy && window.passkeys.biometricReady; onClicked: window.passkeys.lockBiometrics() }
                        }
                        Basic.Label {
                            visible: window.passkeys.enabled && window.passkeys.error.length > 0
                            text: window.passkeys.error
                            textFormat: Text.PlainText
                            Layout.fillWidth: true
                            wrapMode: Text.Wrap
                            color: Theme.text
                        }
                    }
                    ColumnLayout {
                        visible: window.settingsCategory === 3
                        Layout.fillWidth: true
                        spacing: 16
                Basic.Label {
                    text: qsTr("Click a shortcut, then press the new combination. Changes save automatically. Escape cancels recording.")
                    wrapMode: Text.Wrap
                    Layout.fillWidth: true
                    color: window.mutedText
                }
                Repeater {
                    model: window.keyBindings
                    delegate: RowLayout {
                        id: bindingRow
                        required property int index
                        required property string actionLabel
                        required property string keyLabel
                        Layout.fillWidth: true
                        Layout.minimumHeight: 36
                        Basic.Label { text: bindingRow.actionLabel; Layout.fillWidth: true; font.pixelSize: 14 }
                        UiButton {
                            id: recordButton
                            Layout.preferredWidth: 132
                            text: window.recordingIndex === bindingRow.index ? qsTr("Press shortcut…") : bindingRow.keyLabel
                            Accessible.name: qsTr("Change %1 shortcut").arg(bindingRow.actionLabel)
                            highlighted: window.recordingIndex === bindingRow.index
                            onClicked: {
                                window.recordingIndex = bindingRow.index
                                window.bindingError = ""
                                forceActiveFocus()
                            }
                            Keys.priority: Keys.BeforeItem
                            Keys.onPressed: function(event) {
                                if (window.recordingIndex !== bindingRow.index)
                                    return
                                event.accepted = true
                                if (event.isAutoRepeat)
                                    return
                                if (event.key === Qt.Key_Escape) {
                                    window.recordingIndex = -1
                                    window.bindingError = ""
                                    return
                                }
                                window.bindingError = window.keyBindings.assign(bindingRow.index, event.key, event.modifiers)
                                if (!window.bindingError)
                                    window.recordingIndex = -1
                            }
                            onActiveFocusChanged: {
                                if (!activeFocus && window.active && window.recordingIndex === bindingRow.index)
                                    window.recordingIndex = -1
                            }
                        }
                    }
                }
                Basic.Label {
                    text: window.bindingError
                    visible: text.length > 0
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                    color: window.palette.text
                }
                UiButton {
                    text: qsTr("Reset to defaults")
                    onClicked: {
                        window.recordingIndex = -1
                        window.bindingError = ""
                        window.keyBindings.resetDefaults()
                    }
                }
                Basic.Label {
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                    text: Qt.platform.os === "osx"
                        ? qsTr("Move the window: drag its top edge, or hold Option and drag anywhere. Double-click the top edge to expand or restore.")
                        : qsTr("Move the window by dragging the empty start-page background.")
                    color: window.mutedText
                }
                    }
                }
            }
        }
    }

    // Window movement remains available above every website without a toolbar.
    NavigationPill {
        objectName: "navigationPill"
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 12
        z: 21
        visible: window.appearance.navigationPill && !window.emptyPage && !window.settingsOpen
            && !window.webauthn.active && !addressPopup.visible && !tabsPopup.visible && !historyPanel.visible && !workspacePanel.visible
        view: window.webView
        appearance: window.appearance
        tabCount: window.tabs.workspaceCount
        workspaceName: window.workspaces.currentName
        workspaceColor: window.workspaces.currentColor
        onAddressRequested: window.focusAddress()
        onTabsRequested: window.showTabs()
        onPasswordsRequested: window.openPasswords()
        onSettingsRequested: window.openSettings()
        onHistoryRequested: window.openHistory()
        onWorkspacesRequested: window.openWorkspaces()
        onSplitHorizontalRequested: window.splitHorizontal()
        onSplitVerticalRequested: window.splitVertical()
    }
    WorkspacePanel {
        id: workspacePanel
        objectName: "workspacePanel"
        manager: window.workspaces
        tabs: window.tabs
        onClosed: Qt.callLater(window.focusPage)
    }
    HistoryPanel {
        id: historyPanel
        objectName: "historyPanel"
        manager: window.history
        onClosed: Qt.callLater(window.focusPage)
        onVisitRequested: function(url, newTab) {
            close()
            if (newTab) window.selectTab(window.tabs.add(url))
            else window.navigate(url)
        }
    }
    BiometricSetup { id: biometricSetup; objectName: "biometricSetup"; controller: window.passkeys }
    PasskeySetup { id: passkeySetup; objectName: "passkeySetup"; controller: window.passkeys }

    PasswordPanel {
        id: passwordPanel
        anchors.fill: parent
        z: 30
        manager: window.passwords
        activeView: window.webView
    }
    WebAuthnPanel {
        id: webAuthnPanel
        anchors.fill: parent
        z: 31
        controller: window.webauthn
        passkeys: window.passkeys
    }

    MouseArea {
        id: windowHandle
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 10
        z: 20
        hoverEnabled: true
        cursorShape: Qt.SizeAllCursor
        onPressed: if (Qt.platform.os !== "osx") window.startSystemMove()
        onDoubleClicked: window.toggleExpanded()
        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.top
            anchors.topMargin: 3
            width: 44
            height: 3
            radius: 1.5
            color: window.palette.text
            opacity: windowHandle.containsMouse ? 0.45 : 0
            Behavior on opacity { NumberAnimation { duration: 120 } }
        }
    }

    Basic.Popup {
        id: tabsPopup
        parent: Basic.Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(1080, window.width - 48)
        height: Math.min(760, window.height - 64, 202 + Math.max(1, Math.ceil(tabChoices.count / Math.max(1, tabList.columns))) * tabList.cellHeight)
        padding: 24
        modal: true
        dim: true
        focus: true
        closePolicy: Basic.Popup.CloseOnEscape | Basic.Popup.CloseOnPressOutside
        Basic.Overlay.modal: ModalBackdrop { }
        background: ModalPanel { }
        enter: Transition { NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 140 } }
        exit: Transition { NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 100 } }
        contentItem: ColumnLayout {
            spacing: 12
            RowLayout {
                Layout.fillWidth: true
                ColumnLayout {
                    spacing: 3
                    Basic.Label { text: window.workspaces.currentName; textFormat: Text.PlainText; font.pixelSize: 22; font.weight: Font.DemiBold }
                    Basic.Label { text: qsTr("%1 open · Pick up where you left off").arg(window.tabs.workspaceCount); color: window.mutedText; font.pixelSize: 12 }
                }
                Item { Layout.fillWidth: true }
                UiButton { text: qsTr("Workspaces…"); onClicked: window.openWorkspaces() }
                UiButton { text: qsTr("New tab"); onClicked: window.newTab() }
            }
            RowLayout {
                Layout.fillWidth: true
                UiButton { text: qsTr("Split left / right"); onClicked: window.splitHorizontal() }
                UiButton { text: qsTr("Split top / bottom"); onClicked: window.splitVertical() }
                Item { Layout.fillWidth: true }
                UiButton { visible: window.splits.multiple; text: qsTr("Single pane"); onClicked: { tabsPopup.close(); window.singlePane() } }
            }
            UiTextField {
                id: tabSearch
                Layout.fillWidth: true
                placeholderText: qsTr("Search open tabs")
                Accessible.name: qsTr("Search open tabs")
                onTextChanged: tabList.currentIndex = 0
                Keys.onDownPressed: window.moveTabSelection(tabList.columns)
                Keys.onUpPressed: window.moveTabSelection(-tabList.columns)
                Keys.onRightPressed: function(event) {
                    if (text.length === 0) window.moveTabSelection(1)
                    else event.accepted = false
                }
                Keys.onLeftPressed: function(event) {
                    if (text.length === 0) window.moveTabSelection(-1)
                    else event.accepted = false
                }
                onAccepted: {
                    if (tabList.currentIndex >= 0 && tabList.currentIndex < tabChoices.count)
                        window.selectTab(tabChoices.get(tabList.currentIndex).sourceIndex)
                }
            }
            GridView {
                id: tabList
                objectName: "tabsGrid"
                readonly property int columns: Math.max(1, Math.ceil(width / 220))
                readonly property real gutter: 12
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                // The final card ends exactly at the search field's right edge.
                Layout.rightMargin: -gutter
                cellWidth: Math.floor(width / columns)
                cellHeight: (cellWidth - gutter) * 0.625 + 80
                model: tabChoices
                boundsBehavior: Flickable.StopAtBounds
                keyNavigationEnabled: true
                highlightMoveDuration: 0
                Basic.ScrollBar.vertical: Basic.ScrollBar { }
                delegate: Basic.ItemDelegate {
                    id: tabChoice
                    required property int index
                    required property int sourceIndex
                    required property string pageTitle
                    required property string pageUrl
                    required property bool pinned
                    width: tabList.cellWidth - tabList.gutter
                    height: tabList.cellHeight - tabList.gutter
                    padding: 8
                    highlighted: GridView.isCurrentItem
                    activeFocusOnTab: true
                    Accessible.name: pageTitle + ", " + pageUrl
                    onClicked: window.selectTab(sourceIndex)
                    onActiveFocusChanged: if (activeFocus) tabList.currentIndex = index
                    background: Rectangle {
                        radius: 10
                        color: tabChoice.highlighted ? Theme.selection
                            : tabChoice.hovered ? Theme.hover : "transparent"
                        Behavior on color { ColorAnimation { duration: 120 } }
                    }
                    contentItem: ColumnLayout {
                        spacing: 8
                        Rectangle {
                            id: preview
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            color: window.palette.window
                            radius: 8
                            clip: true
                            layer.enabled: true
                            layer.effect: MultiEffect {
                                maskEnabled: true
                                maskSource: previewMask
                            }
                            Rectangle {
                                id: previewMask
                                anchors.fill: parent
                                radius: 8
                                layer.enabled: true
                                visible: false
                            }
                            ShaderEffectSource {
                                anchors.fill: parent
                                visible: tabChoice.pageUrl !== qsTr("Start page")
                                sourceItem: tabViews.itemAt(tabChoice.sourceIndex)
                                // Keep the current page behind the overview, just as
                                // it remains behind the address editor.
                                hideSource: tabsPopup.visible && sourceItem !== window.webView
                                live: tabsPopup.visible
                                smooth: true
                                textureSize: Qt.size(Math.ceil(width * 2), Math.ceil(height * 2))
                                // Show the top of the page without distorting its viewport.
                                sourceRect: sourceItem ? Qt.rect(0, 0, sourceItem.width,
                                    sourceItem.width * height / Math.max(1, width)) : Qt.rect(0, 0, 0, 0)
                            }
                            Column {
                                visible: tabChoice.pageUrl === qsTr("Start page")
                                anchors.centerIn: parent
                                width: parent.width * 0.7
                                spacing: 12
                                Basic.Label {
                                    text: qsTr("Arbiter")
                                    font.pixelSize: 20
                                    font.weight: Font.Medium
                                    anchors.horizontalCenter: parent.horizontalCenter
                                }
                                Rectangle {
                                    width: parent.width
                                    height: 22
                                    radius: 6
                                    color: window.palette.base
                                    Basic.Label {
                                        anchors.centerIn: parent
                                        text: qsTr("Search or enter address")
                                        font.pixelSize: 9
                                        color: window.mutedText
                                    }
                                }
                            }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 6
                            Basic.Label {
                                visible: tabChoice.sourceIndex === window.tabs.currentIndex
                                text: "●"
                                color: window.accent
                                font.pixelSize: 10
                            }
                            Basic.Label {
                                text: tabChoice.pageTitle
                                font.pixelSize: 13
                                Layout.fillWidth: true
                                elide: Text.ElideRight
                                font.weight: Font.Medium
                            }
                            UiButton {
                                text: tabChoice.pinned ? "◆" : "◇"
                                flat: true
                                Layout.preferredWidth: 26
                                Layout.preferredHeight: 24
                                padding: 0
                                horizontalPadding: 0
                                Accessible.name: tabChoice.pinned ? qsTr("Unpin tab") : qsTr("Pin tab")
                                onClicked: window.tabs.togglePinned(tabChoice.sourceIndex)
                            }
                            UiButton {
                                text: "×"
                                flat: true
                                Layout.preferredWidth: 26
                                Layout.preferredHeight: 24
                                padding: 0
                                horizontalPadding: 0
                                font.pixelSize: 16
                                Accessible.name: qsTr("Close %1").arg(tabChoice.pageTitle)
                                onClicked: window.closeTab(tabChoice.sourceIndex)
                            }
                        }
                        Basic.Label {
                            text: tabChoice.pageUrl
                            Layout.fillWidth: true
                            elide: Text.ElideMiddle
                            color: window.mutedText
                            font.pixelSize: 12
                        }
                    }
                }
            }
            Basic.Label {
                visible: tabChoices.count === 0
                text: qsTr("No matching tabs")
                Layout.alignment: Qt.AlignHCenter
                color: window.mutedText
            }
            Basic.Label {
                text: qsTr("↑ ↓ ← →  Navigate     ↵  Open     esc  Dismiss")
                color: window.mutedText
                font.pixelSize: 12
            }
        }
        onAboutToShow: {
            tabSearch.text = ""
            Qt.callLater(function() {
                window.refreshTabChoices()
                tabSearch.forceActiveFocus()
            })
        }
        onClosed: Qt.callLater(window.focusPage)
    }

    function moveTabSelection(delta: int) {
        tabList.currentIndex = Math.max(0, Math.min(tabChoices.count - 1, tabList.currentIndex + delta))
        tabList.positionViewAtIndex(tabList.currentIndex, GridView.Contain)
    }

    ListModel { id: tabChoices }
    function refreshTabChoices() {
        if (!tabsPopup.visible)
            return
        const query = tabSearch.text.toLowerCase().trim()
        tabChoices.clear()
        let selected = 0
        for (const pinned of [true, false]) {
            for (let i = 0; i < tabViews.count; ++i) {
                const view = tabViews.itemAt(i) as WebEngineView
                if (!view || window.tabs.workspaceAt(i) !== window.workspaces.currentId || window.tabs.pinnedAt(i) !== pinned) continue
                const url = view.url.toString()
                const title = url === "about:blank" ? qsTr("New tab") : (view.title || url)
                if (!query || (title + " " + url).toLowerCase().includes(query)) {
                    if (i === window.tabs.currentIndex) selected = tabChoices.count
                    tabChoices.append({sourceIndex: i, pageTitle: title, pageUrl: url === "about:blank" ? qsTr("Start page") : url, pinned: pinned})
                }
            }
        }
        tabList.currentIndex = query ? 0 : selected
    }
    Connections {
        target: tabSearch
        function onTextChanged() { window.refreshTabChoices() }
    }
    Connections {
        target: window.tabs
        function onDataChanged() { window.refreshTabChoices() }
        function onCountChanged() { Qt.callLater(window.refreshTabChoices) }
        function onWorkspaceChanged() { Qt.callLater(window.refreshTabChoices) }
    }

    Basic.Popup {
        id: addressPopup
        parent: Basic.Overlay.overlay
        x: (window.width - width) / 2
        y: Math.max(24, Math.min(100, window.height * 0.12))
        width: Math.min(600, window.width - 48)
        padding: 20
        modal: true
        dim: true
        focus: true
        closePolicy: Basic.Popup.CloseOnEscape | Basic.Popup.CloseOnPressOutside
        Basic.Overlay.modal: ModalBackdrop { }
        background: ModalPanel { }
        enter: Transition { NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 140 } }
        exit: Transition { NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 100 } }
        contentItem: ColumnLayout {
            spacing: 12
            RowLayout {
                Layout.fillWidth: true
                Basic.Label { text: qsTr("Go to"); font.pixelSize: 12; font.weight: Font.Medium; color: window.mutedText; Layout.fillWidth: true }
                Basic.Label { text: qsTr("esc to dismiss"); font.pixelSize: 11; color: window.mutedText }
            }
            AddressField {
                id: popupAddress
                Layout.fillWidth: true
                leftPadding: 0
                rightPadding: 0
                font.pixelSize: 18
                background: Item { }
                Accessible.name: qsTr("Edit address")
            }
        }
        onAboutToShow: Qt.callLater(function() {
            popupAddress.forceActiveFocus()
            popupAddress.selectAll()
        })
        onClosed: Qt.callLater(window.focusPage)
    }

    Basic.Label {
        id: loadError
        text: window.webView ? window.webView.errorText : ""
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 18
        width: Math.min(600, window.width - 40)
        padding: 12
        visible: text.length > 0
        wrapMode: Text.Wrap
        color: window.palette.text
        background: Rectangle { radius: 12; color: window.palette.base }
    }
}
