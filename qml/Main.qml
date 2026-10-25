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
    property var privacy: null
    property var privateSession: null
    property bool privateMode: false
    readonly property bool privateCovered: privateMode && (!privateSession || privateSession.locked)
    signal preferencesRequested()
    function newPrivateWindow() { if (privacy) privacy.open() }
    function lockPrivateWindow() { if (privateSession) privateSession.lock() }
    property bool temporaryPill: false
    readonly property bool edgeActionsAllowed: window.active && !privateCovered && !shuttingDown && !settingsOpen
        && !window.webauthn.active && !sitePermissionQueue.active && !desktopSharePanel.request
        && !addressPopup.visible && !tabsPopup.visible && !historyPanel.visible && !workspacePanel.visible
        && !downloadsPanel.visible && !extensionPanel.visible && !extensionReview.visible
        && !splitWithPicker.visible && !tutorial.visible && !passkeySetup.visible && !biometricSetup.visible
        && !pagePrinter.busy && !nativeShare.active && !passwordPanel.overlayVisible

    readonly property bool canSharePage: webView && browserPreferences.shareableUrl(webView.url).toString().length > 0
    property bool uiReady: false
    Component.onCompleted: { downloads.setBlocked(privateCovered); uiReady = true; if (privateCovered) privateShield.forceActiveFocus() }
    onPrivateCoveredChanged: {
        downloads.setBlocked(privateCovered)
        if (!uiReady) return
        if (privateCovered) {
            sitePermissionQueue.clear()
            desktopSharePanel.cancel()
            nativeShare.cancel()
            downloadsPanel.close()
            developerTools.close()
            temporaryPill = false
            splitWithPicker.close(); workspacePanel.close(); historyPanel.close(); addressPopup.close(); tabsPopup.close()
            biometricSetup.close(); passkeySetup.close(); tutorial.close()
            if (webView) { webAuthnPanel.cancelFor(webView); passwordPanel.cancelFor(webView) }
            for (let i = 0; i < tabViews.count; ++i) {
                const view = tabViews.itemAt(i) as WebEngineView
                if (view) view.cancelPrivateInteraction()
            }
            navigationPill.dismissMenu()
            recordingIndex = -1
            privateShield.forceActiveFocus()
        } else Qt.callLater(window.focusPage)
    }
    Binding { target: Basic.Overlay.overlay; property: "opacity"; value: 0; when: window.privateCovered }
    Binding { target: Basic.Overlay.overlay; property: "enabled"; value: false; when: window.privateCovered }
    onClosing: if (privateMode) prepareForShutdown()
    required property string browserUserAgent
    required property bool browserDiagnostics
    readonly property var splits: workspaces.splits
    property int splitRevision: 0
    required property var workspaces
    required property var webProfiles
    readonly property string currentStoreItem: !privateMode && extensionCatalog && webView ? extensionCatalog.storeItemId(webView.url.toString()) : ""
    readonly property var extensionCatalog: webView ? webProfiles.extensions(webView.profile) : null
    required property var localization
    required property var sitePermissionSettings

    SitePermissions {
        id: sitePermissionQueue
        objectName: "sitePermissionQueue"
        focusedView: window.webView
        enabled: window.active && !window.privateCovered && !window.settingsOpen && !window.shuttingDown
                 && !desktopSharePanel.request && !window.webauthn.active
        locationEnabled: window.sitePermissionSettings.geolocationEnabled
        onResolved: window.sitePermissionSettings.refresh()
    }
    required property var tabs
    required property var tabSleep
    required property var history
    required property var downloads
    required property var onboarding
    required property var adblocker
    required property var siteAudio
    required property var browserPreferences
    required property var appearance
    readonly property string focusedWorkspaceId: webView ? webView.workspaceId : workspaces.currentId
    readonly property var focusedWorkspace: workspaceInfo(focusedWorkspaceId)
    function workspaceInfo(id: string): var {
        for (const space of workspaces.choices) if (space.id === id) return space
        return {name: "", color: window.accent, tabs: 0}
    }
    // Snapshot at launch: changing native window flags can recreate the surface.
    required property bool windowDecorations
    required property var siteNavigation
    property int siteNavigationGeneration: 0
    readonly property bool siteNavigationEnabled: !privateMode && active && Qt.platform.os === "osx" && appearance.siteMenuMode !== "off" && !settingsOpen
    readonly property var homeAddress: homeStart.addressField
    property var webView: null
    property bool changingTabs: false
    property bool shuttingDown: false
    required property var keyBindings
    required property var importer
    required property var passwords
    required property var passkeys
    required property var webauthn
    property string settingsFile: ""
    property url settingsDirectoryUrl
    property string settingsError: ""
    readonly property var bindings: keyBindings.bindings
    property bool settingsOpen: false
    property bool editingDarkAppearance: appearance.dark
    property int settingsCategory: 0
    function clearPrivacyFields() { currentPrivacyPassword.clear(); newPrivacyPassword.clear(); confirmPrivacyPassword.clear() }
    onActiveChanged: if (!active && uiReady) clearPrivacyFields()
    onSettingsCategoryChanged: {
        if (uiReady && settingsCategory !== 7) clearPrivacyFields()
        if (importer && settingsCategory !== 5 && !importer.importing) importer.cancel()
        Qt.callLater(function() { categoryScroll.revealCategory(window.settingsCategory) })
    }
    onSettingsOpenChanged: {
        if (uiReady && settingsOpen) { developerTools.close(); desktopSharePanel.cancel(); nativeShare.cancel(); temporaryPill = false }
        if (uiReady && !settingsOpen) clearPrivacyFields()
        if (importer && !settingsOpen && !importer.importing) importer.cancel()
    }
    property int recordingIndex: -1
    property string bindingError: ""
    signal shortcutRecordingChanged(bool recording)
    onRecordingIndexChanged: shortcutRecordingChanged(recordingIndex >= 0)
    width: 1280
    height: 820
    minimumWidth: 640
    minimumHeight: 480
    visible: true
    flags: windowDecorations ? Qt.Window : Qt.Window | Qt.ExpandedClientAreaHint | Qt.NoTitleBarBackgroundHint
    topPadding: 0
    title: privateMode ? qsTr("Private browsing — Arbiter") : settingsOpen ? qsTr("Settings — Arbiter") : !emptyPage && webView && webView.title ? webView.title + " — Arbiter" : "Arbiter"
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
    readonly property Item glassBackdrop: privateCovered ? null : (settingsOpen ? settingsPage : pageBackdrop)

    readonly property url backgroundImage: appearance.dark ? appearance.darkBackgroundImage : appearance.lightBackgroundImage
    readonly property bool emptyPage: !webView || webView.url.toString() === "about:blank"
    readonly property bool pageNavigationEnabled: webView !== null && webView.activeFocus && !settingsOpen && recordingIndex < 0 && !changingTabs && !shuttingDown

    function resetSiteNavigation() {
        if (privateMode) return
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
        pagePrinter.cancel()
        developerTools.close()
        downloadsPanel.close()
        nativeShare.cancel()
        temporaryPill = false
        splitWithPicker.close()
        shuttingDown = true
        sitePermissionQueue.clear()
        desktopSharePanel.cancel()
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
        if (privateCovered) return;
        nativeShare.cancel()
        splitWithPicker.close()
        workspacePanel.close()
        historyPanel.close()
        if (!addressPopup.visible) {
            tabsPopup.close()
            popupAddress.text = settingsOpen ? "arbiter://settings" : emptyPage ? "" : webView.url.toString()
            addressPopup.open()
        } else popupAddress.focusAddress(true)
    }

    function activateSuggestedTab(tabId: int) {
        if (privateCovered) return
        for (let row = 0; row < tabs.count; ++row) {
            if (tabs.idAt(row) !== tabId) continue
            addressPopup.close()
            const pane = splits.geometry(tabId)
            if (pane.paneId === undefined) workspaces.activate(tabs.workspaceAt(row))
            selectTab(row)
            return
        }
    }

    function navigate(input: string) {
        if (privateCovered) return;
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
            webView.url = window.browserPreferences.searchUrl(text)
        webView.forceActiveFocus()
    }

    function showNavigation() {
        if (privateCovered || settingsOpen || shuttingDown || window.webauthn.active || sitePermissionQueue.active || desktopSharePanel.request) return
        nativeShare.cancel()
        addressPopup.close()
        tabsPopup.close()
        historyPanel.close()
        workspacePanel.close()
        temporaryPill = true
        Qt.callLater(function() { if (!window.privateCovered && !window.settingsOpen && !window.shuttingDown) navigationPill.reveal() })
    }
    function showDownloads() {
        if (privateCovered || shuttingDown || window.webauthn.active || sitePermissionQueue.active || desktopSharePanel.request || extensionReview.visible) return
        nativeShare.cancel()
        splitWithPicker.close()
        if (webView) passwordPanel.cancelFor(webView)
        addressPopup.close(); tabsPopup.close(); historyPanel.close(); workspacePanel.close(); extensionPanel.close()
        navigationPill.dismissMenu()
        downloadsPanel.open()
    }
    Shortcut { sequence: window.bindings.developerTools; enabled: !window.privateCovered && !window.settingsOpen && !window.shuttingDown && window.recordingIndex < 0; onActivated: window.toggleDeveloperTools() }
    Shortcut { sequence: window.bindings.downloads; enabled: !window.privateCovered && window.recordingIndex < 0; onActivated: window.showDownloads() }
    function showExtensions() {
        if (privateMode || privateCovered || settingsOpen || shuttingDown || window.webauthn.active || sitePermissionQueue.active || desktopSharePanel.request) return
        nativeShare.cancel()
        addressPopup.close()
        tabsPopup.close()
        historyPanel.close()
        workspacePanel.close()
        navigationPill.dismissMenu()
        extensionPanel.showList()
    }
    Shortcut { sequence: window.bindings.extensions; enabled: !window.privateMode && !window.privateCovered && !window.settingsOpen && window.recordingIndex < 0; onActivated: window.showExtensions() }
    function sharePage() {
        if (privateCovered || settingsOpen || shuttingDown || !canSharePage || window.webauthn.active || sitePermissionQueue.active || desktopSharePanel.request) return
        addressPopup.close()
        tabsPopup.close()
        historyPanel.close()
        workspacePanel.close()
        navigationPill.dismissMenu()
        nativeShare.show(webView, webView.url, webView.loading ? "" : webView.title, navigationPill.visible && navigationPill.expanded ? navigationPill.shareAnchor() : Qt.point(window.width / 2, window.height - 40))
    }
    Shortcut { sequence: window.bindings.showNavigation; enabled: !window.privateCovered && !window.settingsOpen && window.recordingIndex < 0; onActivated: window.showNavigation() }
    Shortcut { sequence: window.bindings.sharePage; enabled: !window.privateCovered && !window.settingsOpen && window.canSharePage && window.recordingIndex < 0; onActivated: window.sharePage() }
    function printPage(view: var) {
        if (!view || view !== webView || emptyPage || privateCovered || settingsOpen || shuttingDown || pagePrinter.busy) return
        nativeShare.cancel()
        addressPopup.close()
        tabsPopup.close()
        historyPanel.close()
        workspacePanel.close()
        navigationPill.dismissMenu()
        pagePrinter.show(view, view.title)
    }
    Shortcut {
        sequence: window.bindings.print
        enabled: !window.privateCovered && !window.settingsOpen && !window.emptyPage && !window.shuttingDown && !pagePrinter.busy && window.recordingIndex < 0
        onActivated: window.printPage(window.webView)
    }
    function toggleAdBlock() {
        if (privateCovered || !webView || !window.adblocker.enabled) return
        webView.adBlock.paused = !webView.adBlock.paused
        webView.reload()
    }
    Shortcut { sequence: window.bindings.toggleAdBlock; enabled: !window.privateCovered && !window.settingsOpen && window.recordingIndex < 0; onActivated: window.toggleAdBlock() }
    Shortcut { sequence: window.bindings.address; enabled: !window.privateCovered && window.recordingIndex < 0; onActivated: window.focusAddress() }
    Shortcut { sequence: window.bindings.workspaces; enabled: !window.privateCovered && window.recordingIndex < 0; onActivated: window.openWorkspaces() }
    Shortcut { sequence: window.bindings.nextWorkspace; enabled: !window.privateCovered && window.recordingIndex < 0; onActivated: window.workspaces.cycle(1) }
    Shortcut { sequence: window.bindings.previousWorkspace; enabled: !window.privateCovered && window.recordingIndex < 0; onActivated: window.workspaces.cycle(-1) }
    Shortcut { sequence: window.bindings.history; enabled: !window.privateCovered && window.recordingIndex < 0; onActivated: window.openHistory() }
    Shortcut { sequence: window.bindings.passwords; enabled: !window.privateCovered && window.recordingIndex < 0 && !window.settingsOpen; onActivated: window.openPasswords() }
    Shortcut { sequence: window.bindings.reload; enabled: !window.privateCovered && window.webView && window.recordingIndex < 0 && !window.settingsOpen; onActivated: window.webView.reload() }
    Shortcut { sequence: window.bindings.back; enabled: !window.privateCovered && window.pageNavigationEnabled; onActivated: window.webView.goBack() }
    Shortcut { sequence: window.bindings.forward; enabled: !window.privateCovered && window.pageNavigationEnabled; onActivated: window.webView.goForward() }
    // WebEngine's ShortcutOverride preserves native cursor movement in inputs,
    // textareas and contenteditable (including child frames). Only page focus
    // enables scrolling; address fields and other browser controls keep their keys.
    Shortcut { sequence: window.bindings.pageTop; enabled: !window.privateCovered && window.pageNavigationEnabled; onActivated: window.scrollPage(false) }
    Shortcut { sequence: window.bindings.pageBottom; enabled: !window.privateCovered && window.pageNavigationEnabled; onActivated: window.scrollPage(true) }
    Shortcut {
        sequence: window.bindings.stop
        enabled: !window.privateCovered && !splitWithPicker.visible && !addressPopup.visible && !tabsPopup.visible && !historyPanel.visible && !workspacePanel.visible && window.recordingIndex < 0 && !window.settingsOpen
        onActivated: { window.webView.stop(); window.webView.forceActiveFocus() }
    }
    Shortcut { sequence: window.bindings.close; enabled: !window.privateCovered && window.recordingIndex < 0; onActivated: window.closeCurrentTab() }
    Shortcut { sequence: window.bindings.fullscreen; enabled: !window.privateCovered && window.recordingIndex < 0; onActivated: window.toggleFullScreen() }
    Shortcut { sequence: window.bindings.expand; enabled: !window.privateCovered && window.recordingIndex < 0; onActivated: window.toggleExpanded() }

    Shortcut { sequence: window.bindings.newTab; enabled: !window.privateCovered && window.recordingIndex < 0; onActivated: window.newTab() }
    Shortcut { sequence: window.bindings.tabs; enabled: !window.privateCovered && window.recordingIndex < 0; onActivated: window.showTabs() }
    Shortcut { sequence: window.bindings.nextTab; enabled: !window.privateCovered && window.recordingIndex < 0; onActivated: window.nextTab() }
    Shortcut { sequence: window.bindings.previousTab; enabled: !window.privateCovered && window.recordingIndex < 0; onActivated: window.previousTab() }
    Shortcut { sequence: window.bindings.reopenTab; enabled: !window.privateCovered && window.recordingIndex < 0; onActivated: window.reopenTab() }
    Shortcut { sequence: window.bindings.splitHorizontal; enabled: !window.privateCovered && window.recordingIndex < 0; onActivated: window.splitHorizontal() }
    Shortcut { sequence: window.bindings.splitVertical; enabled: !window.privateCovered && window.recordingIndex < 0; onActivated: window.splitVertical() }
    Shortcut { sequence: window.bindings.splitWith; enabled: !window.privateCovered && window.recordingIndex < 0; onActivated: window.openSplitWith() }
    Shortcut { sequence: window.bindings.nextPane; enabled: !window.privateCovered && window.recordingIndex < 0; onActivated: window.nextPane() }
    Shortcut { sequence: window.bindings.previousPane; enabled: !window.privateCovered && window.recordingIndex < 0; onActivated: window.previousPane() }
    Shortcut { sequence: window.bindings.nextHiddenTab; enabled: !window.privateCovered && window.recordingIndex < 0; onActivated: window.nextHiddenTab() }
    Shortcut { sequence: window.bindings.closeWindow; enabled: !window.privateCovered && window.recordingIndex < 0; onActivated: window.close() }

    Shortcut { sequence: window.bindings.privateWindow || ""; enabled: !window.privateCovered && window.recordingIndex < 0; onActivated: window.newPrivateWindow() }
    Shortcut { sequence: window.bindings.lockPrivate || ""; enabled: window.privateMode && !window.privateCovered; onActivated: window.lockPrivateWindow() }
    Shortcut { sequence: window.bindings.closeWindow; enabled: window.privateCovered; onActivated: window.close() }

    function scrollPage(bottom: bool) {
        if (!pageNavigationEnabled) return
        webView.mainFrame.runJavaScript(bottom
            ? "window.scrollTo({left:window.scrollX,top:document.scrollingElement.scrollHeight,behavior:'instant'})"
            : "window.scrollTo({left:window.scrollX,top:0,behavior:'instant'})", 1)
    }

    function focusPage() {
        if (privateCovered) return;
        if (splitWithPicker.visible)
            return
        else if (workspacePanel.visible)
            workspacePanel.focusSearch()
        else if (historyPanel.visible)
            historyPanel.focusSearch()
        else if (tabsPopup.visible)
            tabSearch.forceActiveFocus()
        else if (addressPopup.visible)
            popupAddress.focusAddress(false)
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

    function splitHorizontal() {
        if (privateCovered) return; closeSplitPopups(); splits.split(true) }
    function splitVertical() {
        if (privateCovered) return; closeSplitPopups(); splits.split(false) }
    function openSplitWith() {
        if (privateCovered) return; closeSplitPopups(); splitWithPicker.openFor() }
    function closeSplitPopups() {
        splitWithPicker.close()
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
    function singlePane() {
        if (privateCovered) return; splits.single(); focusPage() }
    function nextPane() {
        if (privateCovered) return; splits.cycle(1); focusPage() }
    function previousPane() {
        if (privateCovered) return; splits.cycle(-1); focusPage() }
    function nextHiddenTab() {
        if (privateCovered) return; if (splits.nextHiddenTab()) selectTab(tabs.currentIndex) }

    Connections {
        target: window.splits
        function onChanged() { ++window.splitRevision }
    }

    function newTab() {
        if (privateCovered) return;
        selectTab(workspaces.addTab(focusedWorkspaceId, "about:blank", false))
    }

    function prepareTabRemoval(first: int, last: int) {
        changingTabs = true
        for (let i = first; i <= last; ++i) {
            const view = tabViews.itemAt(i) as WebEngineView
            if (!view) continue
            developerTools.closeFor(view)
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
        if (privateCovered) return;
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
        if (tabs.currentIndex < 0) tabs.add("about:blank")
        syncActiveView()
        if (tabsPopup.visible)
            tabSearch.forceActiveFocus()
    }

    function nextTab() {
        if (privateCovered) return; if (splits.cycleTab(1)) selectTab(tabs.currentIndex) }
    function previousTab() {
        if (privateCovered) return; if (splits.cycleTab(-1)) selectTab(tabs.currentIndex) }
    function reopenTab() {
        if (privateCovered) return; if (tabs.reopen()) selectTab(tabs.currentIndex) }
    function showTabs() {
        if (privateCovered) return; splitWithPicker.close(); workspacePanel.close(); historyPanel.close(); addressPopup.close(); tabsPopup.open() }

    function openExternalUrl(url: string) {
        syncActiveView()
        if (emptyPage && !settingsOpen) {
            navigate(url)
        } else {
            selectTab(tabs.add(url))
        }
    }

    function openWebRequest(request: var, sourceWorkspace: string) {
        if (privateCovered) return;
        // Preserve background-tab requests and keep the request's opener and
        // pending navigation by handing it directly to the new engine view.
        const background = request.destination === WebEngineView.NewViewInBackgroundTab
        const activate = !background && (sourceWorkspace === workspaces.currentId || (webView && sourceWorkspace === webView.workspaceId))
        const index = workspaces.addTab(sourceWorkspace, "about:blank", false)
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
        if (privateCovered) return;
        if (privateMode) { preferencesRequested(); return }
        splitWithPicker.close()
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

    function toggleDeveloperTools() {
        if (privateCovered || settingsOpen || shuttingDown || !webView) return
        if (developerTools.target === webView) { developerTools.close(); focusPage() }
        else developerTools.openFor(webView)
    }

    function openPasswords() {
        if (privateCovered) return;
        splitWithPicker.close()
        historyPanel.close()
        addressPopup.close()
        tabsPopup.close()
        if (!settingsOpen) passwordPanel.openFor(webView)
    }

    function openPasswordSettings() { if (privateCovered) return; historyPanel.close(); passwordPanel.openManager() }
    function openWorkspaces() {
        if (privateCovered) return;
        splitWithPicker.close()
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
        if (privateCovered) return;
        splitWithPicker.close()
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
        if (privateCovered) return;
        if (visibility === Window.Maximized || visibility === Window.FullScreen)
            showNormal()
        else
            showMaximized()
    }

    function toggleFullScreen() {
        if (privateCovered) return;
        if (visibility === Window.FullScreen)
            showNormal()
        else
            showFullScreen()
    }


    function shortcutMatches(label: string, keys: string, action: string): bool {
        function normalize(text) {
            return text.toLowerCase().replace(/⌘/g, " cmd ").replace(/⌥/g, " alt ")
                .replace(/⌃/g, " ctrl ").replace(/⇧/g, " shift ")
                .replace(/⇥/g, " tab ").replace(/⎋/g, " esc ").replace(/[↩⌅]/g, " enter ")
                .replace(/⌫/g, " backspace ").replace(/⌦/g, " delete ")
                .replace(/←/g, " left ").replace(/→/g, " right ").replace(/↑/g, " up ").replace(/↓/g, " down ")
                .replace(/\bcommand\b/g, "cmd").replace(/\bcontrol\b/g, "ctrl").replace(/\b(option|opt)\b/g, "alt")
                .replace(/\bescape\b/g, "esc").replace(/\breturn\b/g, "enter")
                .replace(/\+/g, " ").replace(/\s+/g, " ").trim()
        }
        const terms = normalize(shortcutSearch.text).split(" ").filter(term => term.length > 0)
        if (terms.length === 0) return true
        if (terms.every(term => normalize(label).includes(term))) return true
        const modifiers = ["cmd", "ctrl", "alt", "shift", "meta"]
        const hasModifier = terms.some(term => modifiers.includes(term))
        const hasKey = terms.some(term => !modifiers.includes(term))
        const portable = bindings[action] || ""
        const platformKeys = Qt.platform.os === "osx"
            ? portable.replace(/\bCtrl\b/g, "Command").replace(/\bMeta\b/g, "Control") : portable
        return [keys, portable, platformKeys].some(sequence => {
            const parts = normalize(sequence).split(" ")
            return terms.every(term => parts.includes(term))
                && (!hasModifier || !hasKey || parts.length === terms.length)
        })
    }

    function moveTabSelection(delta: int) {
        tabList.currentIndex = Math.max(0, Math.min(tabChoices.count - 1, tabList.currentIndex + delta))
        tabList.positionViewAtIndex(tabList.currentIndex, GridView.Contain)
    }

    function refreshTabChoices() {
        if (!tabsPopup.visible)
            return
        const query = tabSearch.text.toLowerCase().trim()
        tabChoices.clear()
        let selected = 0
        for (const pinned of [true, false]) {
            for (let i = 0; i < tabViews.count; ++i) {
                const view = tabViews.itemAt(i) as WebEngineView
                if (!view || window.tabs.workspaceAt(i) !== window.focusedWorkspaceId || window.tabs.pinnedAt(i) !== pinned) continue
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

    Item {
        id: browsingContent
        anchors.fill: parent
        enabled: !window.privateCovered
        Item {
            id: pageBackdrop
            objectName: "pageBackdrop"
            anchors.fill: parent
            BrowserBackground {
                id: windowBackground
                objectName: "windowBackground"
                anchors.fill: parent
                imageSource: window.backgroundImage
                visible: !window.settingsOpen && (window.emptyPage || window.splits.multiple)
            }
            Flickable {
                id: splitViewport
                objectName: "splitViewport"
                anchors.fill: parent
                anchors.bottomMargin: developerTools.height + (window.splits.multiple ? 4 : 0)
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
                            function cancelPrivateInteraction() {
                                sitePermissionQueue.cancelFor(page)
                                desktopSharePanel.cancelFor(page)
                                nativeShare.cancelFor(page)
                                pageContextMenu.close()
                                passwordSession.invalidate()
                                startPage.focus = false
                                focus = false
                            }
                            required property int index
                            required property string workspaceId
                            required property string profileId
                            required property int tabId
                            required property url initialUrl
                            readonly property alias adBlock: viewAdBlock
                            urlRequestInterceptor: viewAdBlock
                            required property bool tabPinned
                            extensionContext: QtObject {
                                id: extensionTabContext
                                property int extensionTabId: -1
                                readonly property QtObject window: page.Window.window
                                readonly property int index: page.index
                                readonly property bool active: page === page.Window.window.webView
                                readonly property bool pinned: page.tabPinned
                                readonly property bool blocked: page.Window.window.privateMode || page.Window.window.privateCovered || page.Window.window.shuttingDown
                                function activate() { page.Window.window.selectTab(page.index) }
                                function closeTab() { page.Window.window.closeTab(page.index) }
                                function setPinned(value: bool) {
                                    if (page.tabPinned !== value) page.Window.window.tabs.togglePinned(page.index)
                                }
                                function createTab(url: string, active: bool): int { return createTabAt(url, active, -1) }
                                function createTabAt(url: string, active: bool, index: int): int {
                                    const owner = page.Window.window
                                    if (blocked) return -1
                                    const row = owner.workspaces.addTab(page.workspaceId, url, false, index)
                                    const view = tabViews.itemAt(row) as WebEngineView
                                    if (!view) return -1
                                    if (active) owner.selectTab(row)
                                    return view.extensionContext.extensionTabId
                                }
                            }
                            ViewAdBlock {
                                id: viewAdBlock
                                manager: window.adblocker
                                onStateChanged: {
                                    if (!enabled) page.mainFrame.runJavaScript("globalThis.__arbiterAdblock?.clear()", 1, function() {})
                                    else Qt.callLater(page.applyAdBlockCosmetics)
                                }
                            }
                            function applyAdBlockCosmetics() {
                                if (!viewAdBlock.enabled || page.loading || !page.visible) return
                                page.mainFrame.runJavaScript("globalThis.__arbiterAdblock?.collect()", 1, function(result) {
                                    if (!result || !viewAdBlock.enabled || result.url !== page.mainFrame.url.toString()) return
                                    const selectors = viewAdBlock.cosmetic(result.url, result.classes, result.ids)
                                    if (selectors.length) page.mainFrame.runJavaScript("globalThis.__arbiterAdblock?.apply(" + JSON.stringify(result.token) + "," + JSON.stringify(selectors) + ")", 1, function() {})
                                })
                            }
                            Timer {
                                interval: 750
                                repeat: true
                                running: viewAdBlock.enabled && page.visible && !page.loading && !window.privateCovered
                                onTriggered: page.applyAdBlockCosmetics()
                            }
                            property bool closeQueued: false
                            property string errorText: ""
                            property bool historyReady: false
                            readonly property var sourceWorkspace: window.workspaceInfo(workspaceId)
                            Timer { id: workspaceBadgeTimer; interval: 1800 }
                            Connections {
                                target: window
                                function onWebViewChanged() { if (window.webView === page) workspaceBadgeTimer.restart() }
                            }
                            GlassSurface {
                                sourceItem: page
                                visible: page.inSplit && window.webView === page && page.workspaceId !== window.workspaces.currentId && workspaceBadgeTimer.running
                                parent: paneCanvas
                                x: page.x + page.width - width - 12
                                y: page.y + 12
                                width: Math.min(page.width - 24, workspaceBadge.implicitWidth + 24)
                                height: 30
                                radius: 15
                                dense: true
                                z: 4
                                Basic.Label {
                                    id: workspaceBadge
                                    anchors.centerIn: parent
                                    width: parent.width - 24
                                    elide: Text.ElideRight
                                    text: page.sourceWorkspace.name
                                    textFormat: Text.PlainText
                                    font.pixelSize: 12
                                    color: Theme.text
                                }
                            }
                            HistorySwipe {
                                id: historySwipe
                                view: page
                                enabled: window.edgeActionsAllowed && page.visible && !pageContextMenu.visible && window.appearance.historySwipeEnabled
                                canGoBack: page.canGoBack
                                canGoForward: page.canGoForward
                                onTriggered: function(direction) {
                                    window.splits.activate(page.paneGeometry.paneId)
                                    if (direction < 0) page.goBack(); else page.goForward()
                                }
                            }
                            Connections {
                                target: page
                                function onUrlChanged() { historySwipe.cancel() }
                                function onWidthChanged() { historySwipe.cancel() }
                            }
                            Shortcut { sequence: "Escape"; enabled: historySwipe.active; onActivated: historySwipe.cancel() }
                            HistorySwipeOverlay {
                                parent: historySwipeLayer
                                x: page.x - splitViewport.contentX
                                y: page.y - splitViewport.contentY
                                width: page.width; height: page.height
                                visible: page.visible
                                controller: historySwipe
                            }
                            TabSleepController {
                                objectName: "tabSleep" + page.tabId
                                view: page
                                timeoutSeconds: window.tabSleep.timeoutSeconds
                                keepAwake: page.inSplit || page === window.webView || page === developerTools.target
                            }
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
                                surfaceSource: windowBackground
                                id: paneStart
                                history: window.history
                                browserPreferences: window.browserPreferences
                                privateMode: window.privateMode
                                tabs: window.tabs
                                currentTabId: page.tabId
                                onTabRequested: function(tabId) { window.activateSuggestedTab(tabId) }
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
                            width: inSplit ? Math.max(0, paneGeometry.width) : splitViewport.width
                            height: inSplit ? Math.max(0, paneGeometry.height) : splitViewport.height
                            onActiveFocusChanged: if (activeFocus && inSplit && !window.settingsOpen && !window.changingTabs && !window.shuttingDown) window.splits.activate(paneGeometry.paneId)
                            visible: (tabsPopup.visible && workspaceId === window.focusedWorkspaceId) || (inSplit && !window.settingsOpen && url.toString() !== "about:blank")
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
                            // Chromium’s built-in PDF viewer requires plugin support.
                            settings.pluginsEnabled: true
                            settings.pdfViewerEnabled: true
                            settings.focusOnNavigationEnabled: index === window.tabs.currentIndex && url.toString() !== "about:blank"
                            url: initialUrl
                            // Transparent document canvases use the browser's
                            // normal white backing, independent of UI colors.
                            backgroundColor: "white"
                            readonly property bool siteMuted: {
                                window.siteAudio.revision
                                return window.siteAudio.isMuted(url)
                            }
                            audioMuted: window.privateCovered || siteMuted
                            profile: window.webProfiles.profile(profileId)
                            webChannel: passwordChannel
                            webChannelWorld: 1
                            userScripts.collection: [{
                                name: "Arbiter password forms",
                                injectionPoint: WebEngineScript.DocumentReady,
                                worldId: WebEngineScript.ApplicationWorld,
                                runsOnSubFrames: false,
                                sourceCode: window.passwords.captureScript
                            }, {
                                name: "Arbiter ad blocking",
                                injectionPoint: WebEngineScript.DocumentReady,
                                worldId: WebEngineScript.ApplicationWorld,
                                runsOnSubFrames: false,
                                sourceCode: window.adblocker.cosmeticScript
                            }]
                            WebChannel {
                                id: passwordChannel
                                Component.onCompleted: registerObject("passwordCapture", passwordSession.capture)
                            }
                            PasswordSession {
                                id: passwordSession
                                manager: window.passwords
                                onFocusNeedsValidation: {
                                    if (page === window.webView && !window.privateCovered && !window.settingsOpen && window.passwords.enabled)
                                        passwordSession.validateFocus(page.mainFrame.url)
                                }
                                onFocused: function(origin, rect) { passwordPanel.offerFor(page, origin, rect) }
                                onCaptureNeedsValidation: { if (page === window.webView && !window.privateCovered && !window.settingsOpen) passwordSession.validateCapture(page.mainFrame.url, page.profile.offTheRecord) }
                                onRunFillScript: function(script) {
                                    if (page !== window.webView || window.privateCovered || window.settingsOpen || !window.passwords.enabled) return
                                    page.mainFrame.runJavaScript(script, 1, function(filled) {
                                        window.passwords.reportFillResult(filled === true)
                                    })
                                }
                                onRunPasswordSuggestionScript: function(script) {
                                    if (page !== window.webView || window.privateCovered || window.settingsOpen || !window.passwords.enabled) return
                                    page.mainFrame.runJavaScript(script, 1, function(filled) {
                                        window.passwords.reportSuggestionResult(filled === true)
                                    })
                                }
                            }
                            WebContextMenu {
                                id: pageContextMenu
                                view: page
                                passwordsEnabled: window.passwords.enabled
                                siteMuted: page.siteMuted
                                canMuteSite: window.siteAudio.siteHost(page.url).length > 0
                                shortcutHints: ({
                                    print: window.keyBindings.nativeSequence(window.bindings.print),
                                    passwords: window.keyBindings.nativeSequence(window.bindings.passwords)
                                })
                                onMuteSiteRequested: {
                                    if (!window.privateCovered && !window.shuttingDown)
                                        window.siteAudio.setMuted(page.url, !page.siteMuted)
                                }
                                onInspectRequested: {
                                    if (!window.privateCovered && !window.shuttingDown && !window.settingsOpen) {
                                        window.splits.activate(page.paneGeometry.paneId)
                                        developerTools.openFor(page, true)
                                    }
                                }
                                onPasswordsRequested: Qt.callLater(window.openPasswords)
                                onPrintRequested: Qt.callLater(function() { window.printPage(page) })
                                onPasswordSuggestionRequested: function(field) {
                                    if (page === window.webView && !window.privateCovered && !window.settingsOpen)
                                        passwordSession.suggestPassword(page.mainFrame.url, field)
                                }
                            }
                            onContextMenuRequested: function(request) { if (page.inSplit) window.splits.activate(page.paneGeometry.paneId); passwordPanel.cancelFor(page); pageContextMenu.show(request) }
                            onNewWindowRequested: function(request) { window.openWebRequest(request, workspaceId) }
                            onWindowCloseRequested: {
                                if (closeQueued) return
                                closeQueued = true
                                const closingId = tabId
                                Qt.callLater(function() { window.finishClosingTab(closingId) })
                            }
                            onPrintRequested: window.printPage(page)
                            settings.screenCaptureEnabled: true
                            onPermissionRequested: function(permissionRequest) {
                                if (window.privateCovered || window.shuttingDown) {
                                    sitePermissionQueue.reject(permissionRequest)
                                } else sitePermissionQueue.request(page, permissionRequest, page.mainFrame.url)
                            }
                            onDesktopMediaRequested: function(request) {
                                if (page !== window.webView || window.privateCovered || window.settingsOpen || !window.active) request.cancel()
                                else desktopSharePanel.show(page, request, window.sitePermissionSettings.displayOrigin(page.mainFrame.url))
                            }
                            onWebAuthUxRequested: function(request) {
                                if (page !== window.webView || window.privateCovered || window.settingsOpen) request.cancel()
                                else webAuthnPanel.show(page, request)
                            }
                            Component.onDestruction: {
                                pagePrinter.cancelFor(page)
                                developerTools.closeFor(page)
                                sitePermissionQueue.cancelFor(page)
                                desktopSharePanel.cancelFor(page)
                                nativeShare.cancelFor(page)
                                webAuthnPanel.cancelFor(page)
                                passwordPanel.cancelFor(page)
                            }
                            onTitleChanged: {
                                window.tabs.update(tabId, title, url)
                                if (!profile.offTheRecord && historyVisitId.length > 0)
                                    window.history.updateTitle(historyVisitId, url, title)
                                if (page === window.webView && !window.privateMode) window.siteNavigation.setTitle(title)
                            }
                            onUrlChanged: {
                                pagePrinter.cancelFor(page)
                                sitePermissionQueue.cancelFor(page)
                                desktopSharePanel.cancelFor(page)
                                nativeShare.cancelFor(page)
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
                                    pagePrinter.cancelFor(page)
                                    viewAdBlock.resetCount()
                                    sitePermissionQueue.cancelFor(page)
                                    desktopSharePanel.cancelFor(page)
                                    nativeShare.cancelFor(page)
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

            Rectangle {
                id: homePage
                anchors.fill: parent
                anchors.bottomMargin: developerTools.height
                visible: window.emptyPage && !window.settingsOpen && !window.splits.multiple
                color: "transparent"

                MouseArea {
                    anchors.fill: parent
                    onPressed: window.startSystemMove()
                }
                StartPage {
                    id: homeStart
                    surfaceSource: windowBackground
                    history: window.history
                    browserPreferences: window.browserPreferences
                    privateMode: window.privateMode
                    tabs: window.tabs
                    currentTabId: window.tabs.idAt(window.tabs.currentIndex)
                    onTabRequested: function(tabId) { window.activateSuggestedTab(tabId) }
                    anchors.fill: parent
                    addressShortcut: window.keyBindings.nativeSequence(window.bindings.address)
                    tabsShortcut: window.keyBindings.nativeSequence(window.bindings.tabs)
                    onNavigateRequested: function(text) { window.navigate(text) }
                    onTabsRequested: window.showTabs()
                    onSettingsRequested: window.toggleSettings()
                }
            }

        }

    // Keep glass controls outside their backdrop capture, and clip them to the
    // visible split canvas when a large layout requires scrolling.
    Item {
        id: historySwipeLayer
        x: splitViewport.x; y: splitViewport.y
        width: splitViewport.width; height: splitViewport.height
        clip: true
        z: 5
    }
    DeveloperToolsDock {
        id: developerTools
        objectName: "developerTools"
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        maximumHeight: Math.max(160, window.height - 160)
        onPageFocusRequested: window.focusPage()
    }

    component ModalPanel: GlassSurface { }

    component ModalBackdrop: Rectangle {
        color: Theme.scrim
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
                Basic.ScrollView {
                    id: categoryScroll
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    contentWidth: availableWidth
                    clip: true
                    function revealCategory(index: int) {
                        const item = settingsNavigation.itemAt(index)
                        const flick = contentItem as Flickable
                        if (!item || !flick) return
                        flick.contentY = Math.max(0, Math.min(item.y,
                            Math.max(flick.contentY, item.y + item.height - flick.height)))
                    }
                    onHeightChanged: Qt.callLater(function() { categoryScroll.revealCategory(window.settingsCategory) })
                    ColumnLayout {
                        width: categoryScroll.availableWidth
                        spacing: 8
                        Repeater {
                            id: settingsNavigation
                            model: [qsTr("Appearance"), qsTr("Passwords"), qsTr("Passkeys"), qsTr("Keyboard"), qsTr("Configuration"), qsTr("Import"), qsTr("Performance"), qsTr("Privacy"), qsTr("Extensions"), qsTr("Downloads"), qsTr("About")]
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
                                onActiveFocusChanged: if (activeFocus) categoryScroll.revealCategory(index)
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
                                Keys.onDownPressed: settingsNavigation.itemAt(Math.min(settingsNavigation.count - 1, index + 1)).forceActiveFocus()
                                Keys.onUpPressed: settingsNavigation.itemAt(Math.max(0, index - 1)).forceActiveFocus()
                            }
                        }
                    }
                }
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
                        text: [qsTr("Appearance"), qsTr("Passwords"), qsTr("Passkeys"), qsTr("Keyboard"), qsTr("Configuration"), qsTr("Import"), qsTr("Performance"), qsTr("Privacy"), qsTr("Extensions"), qsTr("Downloads"), qsTr("About")][window.settingsCategory]
                        font.pixelSize: 28
                        font.weight: Font.DemiBold
                        color: Theme.text
                        Layout.bottomMargin: 4
                    }
                    ColumnLayout {
                        visible: window.settingsCategory === 0
                        Layout.fillWidth: true
                        spacing: 20
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
                        Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; Layout.topMargin: 4; Layout.bottomMargin: 4; color: Theme.border }
                        Basic.Label { text: qsTr("Colors & background"); color: Theme.text; font.pixelSize: 18; font.weight: Font.DemiBold }
                        Basic.Label {
                            Layout.fillWidth: true
                            text: qsTr("Customize each mode independently.")
                            color: Theme.muted
                            wrapMode: Text.Wrap
                            Layout.topMargin: -12
                        }
                        RowLayout {
                            spacing: 6
                            UiButton {
                                objectName: "editLightAppearance"
                                text: qsTr("Light")
                                checkable: true
                                autoExclusive: true
                                checked: !window.editingDarkAppearance
                                highlighted: checked
                                Accessible.name: qsTr("Edit light appearance")
                                Accessible.role: Accessible.PageTab
                                Accessible.selected: checked
                                onClicked: window.editingDarkAppearance = false
                            }
                            UiButton {
                                objectName: "editDarkAppearance"
                                text: qsTr("Dark")
                                checkable: true
                                autoExclusive: true
                                checked: window.editingDarkAppearance
                                highlighted: checked
                                Accessible.name: qsTr("Edit dark appearance")
                                Accessible.role: Accessible.PageTab
                                Accessible.selected: checked
                                onClicked: window.editingDarkAppearance = true
                            }
                        }
                        GridLayout {
                            objectName: "lightAppearanceEditor"
                            visible: !window.editingDarkAppearance
                            Layout.fillWidth: true
                            columns: width >= 640 ? 2 : 1
                            columnSpacing: 16
                            rowSpacing: 16
                            UiColorPicker {
                                objectName: "lightAccentPicker"
                                Layout.fillWidth: true
                                framed: true
                                Layout.fillHeight: true
                                title: qsTr("Accent color")
                                value: window.appearance.lightAccent
                                defaultColor: "#2868b8"
                                onColorSelected: color => window.appearance.lightAccent = color
                            }
                            UiColorPicker {
                                objectName: "lightBackgroundPicker"
                                Layout.fillWidth: true
                                framed: true
                                Layout.fillHeight: true
                                title: qsTr("Background color")
                                value: window.appearance.lightBackground
                                defaultColor: "#edf2f7"
                                presets: ["#edf2f7", "#f7f3ed", "#f1f5ed", "#f4eff7", "#eef6f6", "#ffffff", "#e3e7ed", "#f4eeee"]
                                onColorSelected: color => window.appearance.lightBackground = color
                            }
                            UiImagePicker {
                                id: lightImagePicker
                                objectName: "lightBackgroundImagePicker"
                                Layout.fillWidth: true
                                Layout.columnSpan: parent.columns
                                backgroundColor: window.appearance.lightBackground
                                title: qsTr("Background image")
                                value: window.appearance.lightBackgroundImage
                                onFileSelected: function(file) { error = window.appearance.setBackgroundImage(false, file) }
                                onRemoveRequested: window.appearance.clearBackgroundImage(false)
                            }
                        }
                        GridLayout {
                            objectName: "darkAppearanceEditor"
                            visible: window.editingDarkAppearance
                            Layout.fillWidth: true
                            columns: width >= 640 ? 2 : 1
                            columnSpacing: 16
                            rowSpacing: 16
                            UiColorPicker {
                                objectName: "darkAccentPicker"
                                Layout.fillWidth: true
                                framed: true
                                Layout.fillHeight: true
                                title: qsTr("Accent color")
                                value: window.appearance.darkAccent
                                defaultColor: "#8dbbff"
                                onColorSelected: color => window.appearance.darkAccent = color
                            }
                            UiColorPicker {
                                objectName: "darkBackgroundPicker"
                                Layout.fillWidth: true
                                framed: true
                                Layout.fillHeight: true
                                title: qsTr("Background color")
                                value: window.appearance.darkBackground
                                defaultColor: "#11161e"
                                presets: ["#11161e", "#19181d", "#181d19", "#1e1820", "#142023", "#121212", "#202530", "#211b18"]
                                onColorSelected: color => window.appearance.darkBackground = color
                            }
                            UiImagePicker {
                                id: darkImagePicker
                                objectName: "darkBackgroundImagePicker"
                                Layout.fillWidth: true
                                Layout.columnSpan: parent.columns
                                backgroundColor: window.appearance.darkBackground
                                title: qsTr("Background image")
                                value: window.appearance.darkBackgroundImage
                                onFileSelected: function(file) { error = window.appearance.setBackgroundImage(true, file) }
                                onRemoveRequested: window.appearance.clearBackgroundImage(true)
                            }
                        }
                        Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; Layout.topMargin: 4; Layout.bottomMargin: 4; color: Theme.border }
                        Basic.Label { text: qsTr("Window & navigation"); color: Theme.text; font.pixelSize: 18; font.weight: Font.DemiBold }
                        UiCheckBox {
                            objectName: "windowDecorationsToggle"
                            text: qsTr("Show native window decorations")
                            checked: window.appearance.windowDecorations
                            onToggled: window.appearance.windowDecorations = checked
                        }
                        Basic.Label {
                            text: window.appearance.windowDecorations !== window.windowDecorations
                                ? qsTr("Restart Arbiter to apply this change.")
                                : qsTr("Use the system title bar and window buttons. Changes apply after restarting Arbiter.")
                            color: Theme.muted
                            Layout.fillWidth: true
                            wrapMode: Text.Wrap
                        }
                        Basic.Label { text: qsTr("Navigation pill"); color: Theme.text }
                        UiComboBox {
                            objectName: "navigationPillModeChoice"
                            Layout.fillWidth: true
                            model: [qsTr("Visible handle"), qsTr("Reveal at bottom edge"), qsTr("Shortcut only")]
                            currentIndex: !window.appearance.navigationPill ? 2 : window.appearance.navigationRevealMode === "edge" ? 1 : 0
                            Accessible.name: qsTr("Navigation pill")
                            onActivated: {
                                if (currentIndex !== 2) window.appearance.navigationRevealMode = currentIndex === 1 ? "edge" : "handle"
                                window.appearance.navigationPill = currentIndex !== 2
                            }
                        }
                        Basic.Label {
                            text: qsTr("In bottom-edge mode, pause near the center of the bottom edge to reveal navigation. Move away to hide it. The navigation shortcut works in every mode.")
                            color: Theme.muted
                            Layout.fillWidth: true
                            wrapMode: Text.Wrap
                        }
                        UiCheckBox {
                            text: qsTr("Swipe back and forward with horizontal overscroll")
                            checked: window.appearance.historySwipeEnabled
                            onToggled: window.appearance.historySwipeEnabled = checked
                        }
                        Basic.Label {
                            Layout.fillWidth: true
                            text: qsTr("At a page’s horizontal boundary, start a new two-finger swipe. Keep swiping until the arrow fills, then release to navigate. Reverse direction or press Escape to cancel. Scrollable content gets priority, and ordinary mouse-wheel scrolling never navigates.")
                            color: Theme.muted
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
                            text: qsTr("Show website navigation under the tab title or as separate menus in the macOS menu bar.")
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
                        UiCheckBox {
                            text: qsTr("Verify my identity before filling saved passwords")
                            objectName: "passwordBiometricPreference"
                            checked: window.passwords.biometricEnabled
                            enabled: window.passwords.enabled
                            onToggled: window.passwords.biometricEnabled = checked
                        }
                        Basic.Label {
                            text: qsTr("Uses fingerprint verification when available. Your operating system may separately ask for permission to access its credential store.")
                            color: Theme.muted
                            Layout.fillWidth: true
                            wrapMode: Text.Wrap
                        }
                        Basic.Label { text: qsTr("Ask me to verify again"); color: Theme.text }
                        UiComboBox {
                            objectName: "passwordVerificationCooldown"
                            readonly property var durations: [0, 60, 300, 900, 1800, 3600]
                            model: [qsTr("Every time"), qsTr("After 1 minute"), qsTr("After 5 minutes"),
                                    qsTr("After 15 minutes"), qsTr("After 30 minutes"), qsTr("After 1 hour")]
                            currentIndex: durations.indexOf(window.passwords.verificationCooldown)
                            displayText: currentIndex < 0 ? qsTr("After %1 seconds").arg(window.passwords.verificationCooldown) : currentText
                            enabled: window.passwords.enabled && window.passwords.biometricEnabled
                            implicitWidth: 240
                            onActivated: window.passwords.verificationCooldown = durations[currentIndex]
                        }
                        Basic.Label {
                            text: qsTr("Remember a successful fingerprint verification for this long. Locking, sleeping, or restarting clears it. Your operating system still controls access to its credential store.")
                            color: Theme.muted
                            Layout.fillWidth: true
                            wrapMode: Text.Wrap
                        }
                        UiButton { text: qsTr("Manage saved passwords…"); onClicked: window.openPasswordSettings() }
                        UiButton { text: qsTr("Import passwords + history…"); onClicked: window.settingsCategory = 5 }
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
                RowLayout {
                    Layout.fillWidth: true
                    UiTextField {
                        id: shortcutSearch
                        objectName: "shortcutSearch"
                        Layout.fillWidth: true
                        placeholderText: qsTr("Search actions or shortcuts…")
                        Accessible.name: qsTr("Search keyboard shortcuts")
                        onTextChanged: { window.recordingIndex = -1; window.bindingError = "" }
                        Keys.onEscapePressed: function(event) { if (text.length > 0) clear(); else event.accepted = false }
                    }
                    UiButton {
                        text: qsTr("Clear")
                        visible: shortcutSearch.text.length > 0
                        onClicked: { shortcutSearch.clear(); shortcutSearch.forceActiveFocus() }
                    }
                }
                Repeater {
                    id: shortcutRows
                    property int revision: 0
                    readonly property bool hasMatches: {
                        const currentRevision = revision
                        for (let i = 0; i < count; ++i) {
                            const row = itemAt(i)
                            if (row && row.matchesQuery && currentRevision >= 0) return true
                        }
                        return false
                    }
                    onItemAdded: ++revision
                    onItemRemoved: ++revision
                    model: window.keyBindings
                    delegate: RowLayout {
                        id: bindingRow
                        required property int index
                        required property string actionLabel
                        required property string actionId
                        required property string keyLabel
                        readonly property bool matchesQuery: window.shortcutMatches(actionLabel, keyLabel, actionId)
                        objectName: "shortcutRow_" + actionId
                        visible: matchesQuery
                        Layout.fillWidth: true
                        Layout.minimumHeight: 36
                        Basic.Label { text: bindingRow.actionLabel; Layout.fillWidth: true; Layout.minimumWidth: 0; wrapMode: Text.Wrap; font.pixelSize: 14; color: Theme.text }
                        UiButton {
                            id: recordButton
                            objectName: "recordShortcut_" + bindingRow.actionId
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
                    objectName: "noShortcutMatches"
                    text: qsTr("No matching shortcuts.")
                    visible: !shortcutRows.hasMatches
                    color: window.mutedText
                    Layout.fillWidth: true
                }
                Basic.Label {
                    text: window.bindingError
                    visible: text.length > 0
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                    color: window.palette.text
                }
                UiButton {
                    text: qsTr("Reset all shortcuts")
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
                    ColumnLayout {
                        visible: window.settingsCategory === 4
                        Layout.fillWidth: true
                        spacing: 16
                        Basic.Label { text: qsTr("Interface language"); color: Theme.text; Layout.fillWidth: true }
                        UiComboBox {
                            objectName: "interfaceLanguage"
                            Layout.fillWidth: true
                            model: window.localization.languages
                            textRole: "name"
                            currentIndex: window.localization.languages.findIndex(item => item.code === window.localization.language)
                            onActivated: window.localization.language = window.localization.languages[currentIndex].code
                            Accessible.name: qsTr("Interface language")
                        }
                        Basic.Label {
                            text: window.localization.restartRequired ? qsTr("Restart Arbiter to apply the new language.")
                                : qsTr("Use your system language or choose a language for Arbiter. Websites keep their own content and language.")
                            color: Theme.muted
                            Layout.fillWidth: true
                            wrapMode: Text.Wrap
                        }
                        Basic.Label { text: qsTr("Default search engine"); color: Theme.text; Layout.fillWidth: true }
                        UiComboBox {
                            objectName: "defaultSearchEngine"
                            Layout.fillWidth: true
                            model: ["DuckDuckGo", "Google", "Bing", "Brave Search", "Ecosia", "Startpage"]
                            readonly property var engineIds: ["duckduckgo", "google", "bing", "brave", "ecosia", "startpage"]
                            currentIndex: engineIds.indexOf(window.browserPreferences.searchEngine)
                            onActivated: window.browserPreferences.searchEngine = engineIds[currentIndex]
                            Accessible.name: qsTr("Default search engine")
                        }
                        UiButton {
                            objectName: "replayTutorialButton"
                            text: qsTr("Replay welcome tutorial")
                            onClicked: { window.settingsOpen = false; window.onboarding.replayTutorial() }
                        }
                        Basic.Label {
                            text: qsTr("Keep your preferences in your dotfiles.")
                            color: Theme.text
                            Layout.fillWidth: true
                            wrapMode: Text.Wrap
                        }
                        Basic.Label {
                            text: qsTr("Copy this TOML file to back up, restore, or share your settings. Changes made in Settings are saved here. Restart Arbiter after editing or replacing the file externally.")
                            color: Theme.muted
                            Layout.fillWidth: true
                            wrapMode: Text.Wrap
                        }
                        UiTextField {
                            objectName: "configurationPath"
                            Layout.fillWidth: true
                            readOnly: true
                            text: window.settingsFile
                            Accessible.name: qsTr("Settings file location")
                        }
                        UiButton {
                            text: qsTr("Open settings folder")
                            onClicked: Qt.openUrlExternally(window.settingsDirectoryUrl)
                        }
                        Basic.Label {
                            text: qsTr("The location follows XDG_CONFIG_HOME, or ~/.config by default. Files under XDG_CONFIG_DIRS can provide shared defaults; your settings take precedence.")
                            color: Theme.muted
                            Layout.fillWidth: true
                            wrapMode: Text.Wrap
                        }
                        Basic.Label {
                            text: qsTr("This file contains preferences and keyboard shortcuts. Passwords, passkey keys, browsing history, tabs, and site data are stored separately.")
                            color: Theme.muted
                            Layout.fillWidth: true
                            wrapMode: Text.Wrap
                        }
                        Basic.Label {
                            text: window.settingsError
                            visible: text.length > 0
                            color: Theme.text
                            Layout.fillWidth: true
                            wrapMode: Text.Wrap
                        }
                    }
                    AboutPage {
                        visible: window.settingsCategory === 10
                        Layout.fillWidth: true
                    }
                    DownloadsSettings {
                        visible: window.settingsCategory === 9
                        Layout.fillWidth: true
                        manager: window.downloads
                        onShowRequested: window.showDownloads()
                    }
                    ExtensionsPage {
                        visible: window.settingsCategory === 8
                        Layout.fillWidth: true
                        controller: window.extensionCatalog
                        onBrowseRequested: { window.settingsOpen = false; window.openExternalUrl("https://chromewebstore.google.com/") }
                        onPageRequested: function(id, options) { extensionPanel.showExtension(id, options) }
                    }
                    ImportPage {
                        visible: window.settingsCategory === 5
                        Layout.fillWidth: true
                        controller: window.importer
                    }
                    ColumnLayout {
                        visible: window.settingsCategory === 6
                        Layout.fillWidth: true
                        spacing: 20
                        Basic.Label { text: qsTr("Put inactive tabs to sleep after"); color: Theme.text }
                        UiComboBox {
                            objectName: "tabSleepTimeout"
                            readonly property var durations: [0, 60, 300, 900, 1800, 3600]
                            model: [qsTr("Off"), qsTr("1 minute"), qsTr("5 minutes"), qsTr("15 minutes"), qsTr("30 minutes"), qsTr("1 hour")]
                            currentIndex: durations.indexOf(window.tabSleep.timeoutSeconds)
                            displayText: currentIndex < 0 ? qsTr("%1 seconds").arg(window.tabSleep.timeoutSeconds) : currentText
                            implicitWidth: 240
                            Accessible.name: qsTr("Tab sleep timeout")
                            onActivated: window.tabSleep.timeoutSeconds = durations[currentIndex]
                        }
                        Basic.Label {
                            text: qsTr("Sleeping tabs pause background activity and resume when you return. Visible split panes and tabs playing audio stay awake. Page contents are preserved without reloading. Turning this off wakes sleeping tabs.")
                            color: Theme.muted
                            Layout.fillWidth: true
                            wrapMode: Text.Wrap
                        }
                    }
                    ColumnLayout {
                        visible: window.settingsCategory === 7
                        onVisibleChanged: if (visible && window.privacy) window.privacy.load()
                        spacing: 16
                        Layout.fillWidth: true
                        Basic.Label { text: qsTr("Ad blocking"); font.pixelSize: 20; font.weight: Font.DemiBold; color: Theme.text; Layout.fillWidth: true }
                        UiCheckBox {
                            Layout.fillWidth: true
                            leftPadding: 0
                            text: qsTr("Block ads and trackers")
                            checked: window.adblocker.enabled
                            onToggled: window.adblocker.enabled = checked
                        }
                        Basic.Label {
                            Layout.fillWidth: true
                            text: qsTr("EasyList + EasyPrivacy, powered by Brave’s Rust engine. Pause only the focused view using the pill shield or %1. Pauses last until that tab closes. Reload pages after changing the global setting.").arg(window.keyBindings.nativeSequence(window.bindings.toggleAdBlock))
                            wrapMode: Text.Wrap
                            color: Theme.muted
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            Basic.Label { Layout.fillWidth: true; text: window.adblocker.status; color: Theme.muted; wrapMode: Text.Wrap }
                            UiButton { text: window.adblocker.updating ? qsTr("Updating…") : qsTr("Update lists"); enabled: !window.adblocker.updating; onClicked: window.adblocker.updateLists() }
                        }
                        Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.border }
                        SitePermissionsSettings { Layout.fillWidth: true; settings: window.sitePermissionSettings; workspaceChoices: window.workspaces.choices }
                        Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.border }
                        Basic.Label { text: qsTr("Private browsing"); font.pixelSize: 20; font.weight: Font.DemiBold; color: Theme.text }
                        Basic.Label {
                            Layout.fillWidth: true
                            text: qsTr("Private windows keep cookies and site storage in memory and do not save browsing history or restore tabs. Each private window has its own session. Downloads you save and credentials you explicitly create remain on your device.")
                            color: Theme.muted
                            wrapMode: Text.Wrap
                        }
                        UiButton { text: qsTr("New private window"); onClicked: window.newPrivateWindow() }
                        Basic.Label { text: qsTr("Lock after no activity for"); color: Theme.text }
                        UiComboBox {
                            objectName: "privateLockTimeout"
                            implicitWidth: 240
                            readonly property var durations: [60, 120, 300, 600, 900, 1800, 3600]
                            model: [qsTr("1 minute"), qsTr("2 minutes"), qsTr("5 minutes"), qsTr("10 minutes"), qsTr("15 minutes"), qsTr("30 minutes"), qsTr("1 hour")]
                            currentIndex: window.privacy ? durations.indexOf(window.privacy.timeoutSeconds) : -1
                            displayText: currentIndex < 0 && window.privacy ? qsTr("%1 seconds").arg(window.privacy.timeoutSeconds) : currentText
                            onActivated: function(index) { if (window.privacy) window.privacy.timeoutSeconds = durations[index] }
                            Accessible.name: qsTr("Private browsing lock timeout")
                        }
                        Basic.Label {
                            Layout.fillWidth: true
                            text: qsTr("Private windows also lock immediately when switched to the background, minimized, or when your computer locks or sleeps. Use biometrics (or a Windows Hello PIN), or the global private-browsing PIN/password to unlock. Change your PIN/password below by entering its current value.")
                            color: Theme.muted
                            wrapMode: Text.Wrap
                        }
                        Basic.Label {
                            Layout.fillWidth: true
                            text: window.privacy && window.privacy.configured ? qsTr("Change private-browsing PIN/password") : qsTr("Set private-browsing PIN/password")
                            color: Theme.text
                            font.weight: Font.DemiBold
                        }
                        Basic.Label {
                            Layout.fillWidth: true
                            text: qsTr("One PIN (6+ digits) or password (8+ characters) protects all private windows, including after restarting Arbiter. It is stored securely through your operating system. Changing it locks all open private windows.")
                            color: Theme.muted
                            wrapMode: Text.Wrap
                        }
                        UiTextField {
                            id: currentPrivacyPassword
                            objectName: "currentPrivacyPassword"
                            Layout.fillWidth: true
                            visible: window.privacy && window.privacy.configured
                            enabled: window.privacy && window.privacy.ready && !window.privacy.busy
                            echoMode: TextInput.Password
                            maximumLength: 256
                            placeholderText: qsTr("Current PIN or password")
                        }
                        UiTextField {
                            id: newPrivacyPassword
                            objectName: "newPrivacyPassword"
                            Layout.fillWidth: true
                            enabled: window.privacy && window.privacy.ready && !window.privacy.busy
                            echoMode: TextInput.Password
                            maximumLength: 256
                            placeholderText: qsTr("New PIN or password")
                        }
                        UiTextField {
                            id: confirmPrivacyPassword
                            objectName: "confirmPrivacyPassword"
                            Layout.fillWidth: true
                            enabled: window.privacy && window.privacy.ready && !window.privacy.busy
                            echoMode: TextInput.Password
                            maximumLength: 256
                            placeholderText: qsTr("Confirm new PIN or password")
                            onAccepted: savePrivacyPassword.clicked()
                        }
                        UiButton {
                            id: savePrivacyPassword
                            objectName: "savePrivacyPassword"
                            text: window.privacy && window.privacy.configured ? qsTr("Change PIN/password") : qsTr("Set PIN/password")
                            enabled: window.privacy && window.privacy.ready && !window.privacy.busy
                            onClicked: {
                                if (!enabled) return
                                window.privacy.setPassword(currentPrivacyPassword.text, newPrivacyPassword.text, confirmPrivacyPassword.text)
                                currentPrivacyPassword.clear(); newPrivacyPassword.clear(); confirmPrivacyPassword.clear()
                            }
                        }
                        Basic.BusyIndicator { visible: window.privacy && window.privacy.busy; running: visible }
                        Basic.Label { Layout.fillWidth: true; text: window.privacy ? window.privacy.status : ""; color: Theme.muted; wrapMode: Text.Wrap }
                        UiButton { visible: window.privacy && !window.privacy.ready && !window.privacy.busy; text: qsTr("Retry credential store"); onClicked: window.privacy.load() }
                    }
                }
            }
        }
    }

    GlassSurface {
        id: storeInstallBanner
        objectName: "storeInstallBanner"
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.margins: 20
        z: 20
        visible: window.currentStoreItem.length > 0 && !window.settingsOpen && !window.privateCovered
            && !window.webauthn.active && !sitePermissionQueue.active && !desktopSharePanel.request && !tutorial.visible
        width: Math.min(420, parent.width - 40)
        implicitHeight: storeBannerLayout.implicitHeight + 24
        height: implicitHeight
        radius: Theme.menuRadius
        ColumnLayout {
            id: storeBannerLayout
            anchors.fill: parent
            anchors.margins: 12
            spacing: 8
            Basic.Label { Layout.fillWidth: true; text: qsTr("Chrome Web Store extension"); color: Theme.text; font.weight: Font.DemiBold }
            Basic.Label { Layout.fillWidth: true; text: qsTr("Install in Arbiter, then review its permissions. Some extensions require Chrome APIs we don’t support yet."); color: Theme.muted; wrapMode: Text.Wrap }
            UiButton {
                objectName: "addStoreExtensionButton"
                Layout.fillWidth: true
                text: window.extensionCatalog && window.extensionCatalog.busy ? qsTr("Installing…") : qsTr("Add to Arbiter")
                primary: true
                enabled: window.extensionCatalog && !window.extensionCatalog.busy && !window.extensionCatalog.review.id
                onClicked: {
                    const link = window.webView.url.toString()
                    window.openSettings()
                    window.settingsCategory = 8
                    window.extensionCatalog.installFromStore(link)
                }
            }
        }
    }

    // Window movement remains available above every website without a toolbar.
    Item {
        id: pillEdgeZone
        objectName: "pillEdgeZone"
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: developerTools.height
        width: Math.min(320, parent.width * 0.5)
        height: 12
        z: 22
        enabled: window.edgeActionsAllowed && window.appearance.navigationPill && window.appearance.navigationRevealMode === "edge"
        HoverHandler {
            id: pillEdgeHover
            onHoveredChanged: {
                if (hovered) pillEdgeDwell.restart()
                else { pillEdgeDwell.stop(); navigationPill.edgeHovered = false }
            }
        }
        Timer {
            id: pillEdgeDwell
            interval: 180
            onTriggered: if (pillEdgeZone.enabled && pillEdgeHover.hovered) navigationPill.edgeHovered = true
        }
        onEnabledChanged: if (!enabled) { pillEdgeDwell.stop(); navigationPill.edgeHovered = false }
    }
    NavigationPill {
        id: navigationPill
        objectName: "navigationPill"
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: developerTools.height + 12 - slideOffset
        z: 21
        forceHidden: window.privateCovered || window.shuttingDown
        shown: ((window.appearance.navigationPill && (!edgeReveal || engaged)) || window.temporaryPill) && (!window.emptyPage || window.temporaryPill || edgeReveal) && !window.settingsOpen
            && !window.webauthn.active && !addressPopup.visible && !tabsPopup.visible && !historyPanel.visible && !workspacePanel.visible
        view: window.webView
        appearance: window.appearance
        adBlockEnabled: window.adblocker.enabled
        adBlockActive: window.webView ? window.webView.adBlock.enabled : false
        adBlockBlocked: window.webView ? window.webView.adBlock.blocked : 0
        onAdBlockRequested: window.toggleAdBlock()
        siteMuted: window.webView ? window.webView.siteMuted : false
        siteMuteAvailable: window.webView && window.siteAudio.siteHost(window.webView.url).length > 0
        onMuteSiteRequested: {
            if (window.webView && !window.privateCovered && !window.shuttingDown)
                window.siteAudio.setMuted(window.webView.url, !window.webView.siteMuted)
        }
        tabCount: window.focusedWorkspace.tabs
        workspaceName: window.workspaces.currentName
        workspaceColor: window.workspaces.currentColor
        onAddressRequested: window.focusAddress()
        canShare: window.canSharePage
        installedExtensions: window.extensionCatalog ? window.extensionCatalog.items : []
        onExtensionRequested: function(id, options) { extensionPanel.showExtension(id, options) }
        onManageExtensionsRequested: { window.openSettings(); window.settingsCategory = 8 }
        onShareRequested: window.sharePage()
        onRevealProgressChanged: if (!expanded && revealProgress <= 0.001) window.temporaryPill = false
        onTabsRequested: window.showTabs()
        onPasswordsRequested: window.openPasswords()
        onSettingsRequested: window.openSettings()
        onHistoryRequested: window.openHistory()
        activeDownloads: window.downloads.activeCount
        onDeveloperToolsRequested: window.toggleDeveloperTools()
        onDownloadsRequested: window.showDownloads()
        onWorkspacesRequested: window.openWorkspaces()
        onSplitHorizontalRequested: window.splitHorizontal()
        onSplitVerticalRequested: window.splitVertical()
        onSplitWithRequested: window.openSplitWith()
        privateMode: window.privateMode
        onPrivateWindowRequested: window.newPrivateWindow()
        onLockPrivateRequested: window.lockPrivateWindow()
    }
    DownloadsPanel {
        id: downloadsPanel
        objectName: "downloadsPanel"
        manager: window.downloads
    }
    Connections {
        target: window.downloads
        function onStarted() { if (!window.settingsOpen) window.showDownloads() }
    }
    ExtensionReview {
        id: extensionReview
        catalog: window.extensionCatalog
        allowed: !window.privateMode && !window.privateCovered && !window.shuttingDown
            && !window.webauthn.active && !sitePermissionQueue.active && !desktopSharePanel.request && !tutorial.visible
    }
    ExtensionPanel {
        id: extensionPanel
        objectName: "extensionPanel"
        controller: window.extensionCatalog
        browserProfile: window.webView ? window.webView.profile : null
        onInvoked: function(id) { if (window.webView) window.webView.grantExtensionActiveTab(id) }
        onOptionsRequested: function(url) {
            if (window.privateMode || window.privateCovered || window.shuttingDown || !window.webView) return
            window.selectTab(window.workspaces.addTab(window.focusedWorkspaceId, url, false))
        }
        onManageRequested: { window.openSettings(); window.settingsCategory = 8 }
        onNewWindowRequested: function(request) { window.openWebRequest(request, window.focusedWorkspaceId) }
        onVisitRequested: function(url) { extensionPanel.close(); window.openExternalUrl(url) }
    }
    Connections {
        target: window
        function onWebViewChanged() { extensionPanel.close() }
        function onSettingsOpenChanged() { extensionPanel.close() }
        function onPrivateCoveredChanged() { if (window.privateCovered) extensionPanel.close() }
        function onShuttingDownChanged() { if (window.shuttingDown) extensionPanel.close() }
    }
    NativeShare {
        id: nativeShare
        objectName: "nativePageShare"
        host: window
    }
    PagePrinter {
        id: pagePrinter
        objectName: "pagePrinter"
        host: window
        onPdfRequested: function(request, pageSize, orientation) {
            const view = owner
            if (!view || window.privateCovered || window.shuttingDown) { cancel(); return }
            view.printToPdf(function(data) { pagePrinter.submit(request, data) }, pageSize, orientation)
        }
    }
    Connections {
        target: window
        function onWebViewChanged() { pagePrinter.cancel() }
        function onPrivateCoveredChanged() { if (window.privateCovered) pagePrinter.cancel() }
        function onSettingsOpenChanged() { if (window.settingsOpen) pagePrinter.cancel() }
    }
    Timer { id: printErrorTimer; interval: 6000 }
    Connections {
        target: pagePrinter
        function onErrorChanged() { if (pagePrinter.error.length) printErrorTimer.restart() }
    }
    Basic.Label {
        visible: printErrorTimer.running && !window.privateCovered
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 88
        width: Math.min(440, window.width - 32)
        padding: 12
        z: 30
        text: pagePrinter.error
        textFormat: Text.PlainText
        wrapMode: Text.Wrap
        color: Theme.text
        background: GlassSurface { radius: Theme.tooltipRadius; dense: true }
    }
    Connections {
        target: window
        function onWebViewChanged() { nativeShare.cancel() }
    }
    Timer { id: shareErrorTimer; interval: 4000 }
    Connections {
        target: nativeShare
        function onErrorChanged() { if (nativeShare.error.length) shareErrorTimer.restart() }
    }
    Basic.Label {
        visible: shareErrorTimer.running && !window.privateCovered
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 88
        width: Math.min(440, window.width - 32)
        padding: 12
        z: 30
        text: nativeShare.error
        textFormat: Text.PlainText
        wrapMode: Text.Wrap
        color: Theme.text
        background: GlassSurface { radius: 12; dense: true }
    }
    WorkspacePanel {
        id: workspacePanel
        objectName: "workspacePanel"
        manager: window.workspaces
        tabs: window.tabs
        browserPreferences: window.browserPreferences
        privateMode: window.privateMode
        onTabRequested: index => window.selectTab(index)
        onMoreTabsRequested: Qt.callLater(window.showTabs)
        onClosed: Qt.callLater(window.focusPage)
    }
    HistoryPanel {
        id: historyPanel
        privateMode: window.privateMode
        objectName: "historyPanel"
        manager: window.history
        browserPreferences: window.browserPreferences
        onClosed: Qt.callLater(window.focusPage)
        onVisitRequested: function(url, newTab) {
            close()
            if (newTab) window.selectTab(window.tabs.add(url))
            else window.navigate(url)
        }
    }
    BiometricSetup { id: biometricSetup; objectName: "biometricSetup"; controller: window.passkeys }
    PasskeySetup { id: passkeySetup; objectName: "passkeySetup"; controller: window.passkeys }

    PermissionPanel {
        anchors.fill: parent
        controller: sitePermissionQueue
        privateMode: window.privateMode
    }
    DesktopSharePanel {
        id: desktopSharePanel
        anchors.fill: parent
        Connections {
            target: window
            function onWebViewChanged() { if (desktopSharePanel.owner !== window.webView) desktopSharePanel.cancel() }
        }
    }
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
        visible: !window.windowDecorations
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

    Tutorial {
        id: tutorial
        objectName: "browserTutorial"
        controller: window.onboarding
        keyBindings: window.keyBindings
        navigationPill: window.appearance.navigationPill
        onAddressRequested: window.focusAddress()
        onTabsRequested: window.showTabs()
        onWorkspacesRequested: window.openWorkspaces()
        onSplitRequested: window.splitHorizontal()
    }
    SplitWithPicker {
        id: splitWithPicker
        tabs: window.tabs
        workspaces: window.workspaces
        splits: window.splits
        onDismissed: Qt.callLater(window.focusPage)
    }
    Connections {
        target: window.onboarding
        function onChanged() {
            if (!window.privateMode && window.onboarding.tutorialPending && !window.settingsOpen) tutorial.open()
            else if (!window.onboarding.tutorialPending) tutorial.close()
        }
    }
    Timer {
        interval: 250
        running: !window.privateMode && window.onboarding.tutorialPending && !window.settingsOpen
        onTriggered: tutorial.open()
    }

    Basic.Popup {
        popupType: Basic.Popup.Item
        id: tabsPopup
        objectName: "tabsPopup"
        parent: Basic.Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(1080, window.width - 48)
        readonly property real chromeHeight: topPadding + bottomPadding
            + tabsHeading.implicitHeight
            + tabSearch.implicitHeight + tabsHelp.implicitHeight
            + (tabsEmptyMessage.visible ? tabsEmptyMessage.implicitHeight : 0)
            + tabsContent.spacing * (tabsEmptyMessage.visible ? 4 : 3)
        readonly property int rowCount: Math.max(1, Math.ceil(tabChoices.count / tabList.columns))
        // Reserve the real header/footer sizes before allocating complete rows.
        // Only the available window height may force the first row to be clipped.
        height: Math.max(0, Math.min(window.height, chromeHeight + rowCount * tabList.cellHeight,
            Math.max(chromeHeight + tabList.cellHeight, Math.min(760, window.height - 64))))
        padding: 24
        modal: true
        dim: true
        focus: true
        closePolicy: Basic.Popup.CloseOnEscape | Basic.Popup.CloseOnPressOutside
        Basic.Overlay.modal: ModalBackdrop { }
        background: ModalPanel { }
        enter: Transition { NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Theme.enterDuration; easing.type: Easing.OutCubic } }
        exit: Transition { NumberAnimation { property: "opacity"; from: 1; to: 0; duration: Theme.exitDuration; easing.type: Easing.OutCubic } }
        contentItem: ColumnLayout {
            id: tabsContent
            spacing: 12
            RowLayout {
                id: tabsHeading
                Layout.fillHeight: false
                Layout.fillWidth: true
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 3
                    Basic.Label { Layout.fillWidth: true; elide: Text.ElideRight; text: window.focusedWorkspace.name; textFormat: Text.PlainText; font.pixelSize: 22; font.weight: Font.DemiBold }
                    Basic.Label { Layout.fillWidth: true; elide: Text.ElideRight; text: qsTr("%1 open · Pick up where you left off").arg(window.focusedWorkspace.tabs); color: window.mutedText; font.pixelSize: 12 }
                }
                RowLayout {
                    spacing: 4
                    UiIconButton { objectName: "tabsSplitHorizontalButton"; label: qsTr("Split left / right"); iconName: "columns-2"; onClicked: window.splitHorizontal() }
                    UiIconButton { objectName: "tabsSplitVerticalButton"; label: qsTr("Split top / bottom"); iconName: "rows-2"; onClicked: window.splitVertical() }
                    UiIconButton { objectName: "tabsSplitWithButton"; label: qsTr("Split with…"); iconName: "panels-top-left"; onClicked: window.openSplitWith() }
                    UiIconButton { visible: window.splits.multiple; label: qsTr("Single pane"); iconName: "maximize-2"; onClicked: { tabsPopup.close(); window.singlePane() } }
                    Rectangle { Layout.preferredWidth: 1; Layout.preferredHeight: 20; Layout.leftMargin: 6; Layout.rightMargin: 6; color: Theme.border }
                    UiIconButton { objectName: "tabsWorkspacesButton"; label: qsTr("Workspaces…"); iconName: "layers"; onClicked: window.openWorkspaces() }
                    UiIconButton { objectName: "tabsNewTabButton"; label: qsTr("New tab"); iconName: "plus"; onClicked: window.newTab() }
                }
            }
            UiTextField {
                id: tabSearch
                Layout.fillHeight: false
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
                snapMode: GridView.SnapToRow
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
                            UiIcon {
                                visible: tabChoice.sourceIndex === window.tabs.currentIndex
                                name: "check"
                                color: window.accent
                                Layout.preferredWidth: 14
                                Layout.preferredHeight: 14
                            }
                            Basic.Label {
                                text: tabChoice.pageTitle
                                font.pixelSize: 13
                                Layout.fillWidth: true
                                elide: Text.ElideRight
                                font.weight: Font.Medium
                            }
                            UiButton {
                                iconName: tabChoice.pinned ? "pin-off" : "pin"
                                iconSize: 16
                                flat: true
                                Layout.preferredWidth: 26
                                Layout.preferredHeight: 24
                                padding: 0
                                horizontalPadding: 0
                                Accessible.name: tabChoice.pinned ? qsTr("Unpin tab") : qsTr("Pin tab")
                                onClicked: window.tabs.togglePinned(tabChoice.sourceIndex)
                            }
                            UiButton {
                                iconName: "x"
                                iconSize: 16
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
                id: tabsEmptyMessage
                visible: tabChoices.count === 0
                text: qsTr("No matching tabs")
                Layout.alignment: Qt.AlignHCenter
                color: window.mutedText
            }
            Basic.Label {
                id: tabsHelp
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

    ListModel { id: tabChoices }
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

    UiPopup {
        id: addressPopup
        objectName: "addressPopup"
        parent: Basic.Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(760, window.width - 48)
        padding: 0
        modal: true
        dim: true
        focus: true
        closePolicy: Basic.Popup.CloseOnEscape | Basic.Popup.CloseOnPressOutside
        Basic.Overlay.modal: ModalBackdrop { }
        background: Item { }
        contentItem: AddressPrompt {
            id: popupAddress
            surfaceSource: window.glassBackdrop
            objectName: "popupAddressPrompt"
            maximumHeight: Math.min(640, window.height - 64)
            history: window.history
            browserPreferences: window.browserPreferences
            privateMode: window.privateMode
            tabs: window.tabs
            currentTabId: window.tabs.idAt(window.tabs.currentIndex)
            onNavigateRequested: function(input) { window.navigate(input) }
            onTabRequested: function(tabId) { window.activateSuggestedTab(tabId) }
            onCancelRequested: addressPopup.close()
        }
        onAboutToShow: Qt.callLater(function() { popupAddress.focusAddress(true) })
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
        background: GlassSurface { radius: Theme.tooltipRadius; dense: true }
    }
    }
    ShaderEffectSource {
        id: privacyCapture
        sourceItem: window.privateCovered ? browsingContent : null
        // Downsample heavily before blurring to remove legible page detail.
        textureSize: Qt.size(Math.max(1, Math.ceil(window.width / 24)), Math.max(1, Math.ceil(window.height / 24)))
        hideSource: window.privateCovered
        live: window.privateCovered
        visible: false
    }
    MultiEffect {
        anchors.fill: parent
        z: 998
        visible: window.privateCovered
        source: privacyCapture
        blurEnabled: true
        blurMax: 64
        blurMultiplier: 2
        blur: 1
        saturation: -0.5
        autoPaddingEnabled: false
    }
    PrivateShield {
        id: privateShield
        objectName: "privateShield"
        session: window.privateSession
        onCloseRequested: window.close()
    }
    Basic.Label {
        visible: window.privateMode && !window.privateCovered && !window.settingsOpen
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 12
        z: 22
        text: qsTr("Private")
        color: Theme.muted
        font.pixelSize: 11
        background: GlassSurface { radius: 10; dense: true; shadowEnabled: false }
        padding: 6
    }

}
