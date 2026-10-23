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
    required property var tabs
    required property var appearance
    required property var siteNavigation
    property int siteNavigationGeneration: 0
    readonly property bool siteNavigationEnabled: Qt.platform.os === "osx" && appearance.siteMenuMode !== "off" && !settingsOpen
    property var webView: null
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
        addressPopup.close()
        tabsPopup.close()
        if (webView) {
            webAuthnPanel.cancelFor(webView)
            passwordPanel.cancelFor(webView)
        }
        for (let i = 0; i < tabViews.count; ++i) {
            const view = tabViews.itemAt(i) as WebEngineView
            if (view) view.focus = false
        }
        webView = null
        homeAddress.forceActiveFocus()
        // Release views while their profile and the window's focus delivery
        // agent are still alive, before ApplicationWindow starts destruction.
        tabViews.model = null
    }

    function focusAddress() {
        if (emptyPage && !settingsOpen) {
            homeAddress.forceActiveFocus()
            homeAddress.selectAll()
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
        if (!webView)
            return
        const text = input.trim()
        if (!text)
            return
        addressPopup.close()
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
    Shortcut { sequence: window.bindings.passwords; enabled: window.recordingIndex < 0 && !window.settingsOpen; onActivated: window.openPasswords() }
    Shortcut { sequence: window.bindings.reload; enabled: window.webView && window.recordingIndex < 0 && !window.settingsOpen; onActivated: window.webView.reload() }
    Shortcut { sequence: window.bindings.back; enabled: window.webView && window.recordingIndex < 0 && !window.settingsOpen; onActivated: window.webView.goBack() }
    Shortcut { sequence: window.bindings.forward; enabled: window.webView && window.recordingIndex < 0 && !window.settingsOpen; onActivated: window.webView.goForward() }
    Shortcut {
        sequence: window.bindings.stop
        enabled: !addressPopup.visible && !tabsPopup.visible && window.recordingIndex < 0 && !window.settingsOpen
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
    Shortcut { sequence: window.bindings.closeWindow; enabled: window.recordingIndex < 0; onActivated: window.close() }

    function focusPage() {
        if (tabsPopup.visible)
            tabSearch.forceActiveFocus()
        else if (addressPopup.visible)
            popupAddress.forceActiveFocus()
        else if (settingsOpen)
            settingsPage.forceActiveFocus()
        else if (emptyPage)
            homeAddress.forceActiveFocus()
        else if (webView)
            webView.forceActiveFocus()
    }

    function syncActiveView() {
        if (webView) webAuthnPanel.cancelFor(webView)
        if (webView) passwordPanel.cancelFor(webView)
        webView = tabViews.itemAt(tabs.currentIndex)
        homeAddress.text = ""
        Qt.callLater(focusPage)
    }

    function selectTab(index: int) {
        addressPopup.close()
        tabsPopup.close()
        settingsOpen = false
        recordingIndex = -1
        tabs.currentIndex = index
        syncActiveView()
    }

    function newTab() {
        selectTab(tabs.add("about:blank"))
    }

    function closeCurrentTab() {
        closeTab(tabs.currentIndex)
    }

    function closeTab(index: int) {
        const view = tabViews.itemAt(index) as WebEngineView
        if (!view)
            return
        // Let Chromium run beforeunload and show its confirmation before
        // destroying a page with unsaved work.
        addressPopup.close()
        tabsPopup.close()
        view.triggerWebAction(WebEngineView.RequestClose)
    }

    function finishClosingTab(index: int) {
        if (tabs.count === 1) {
            window.close()
            return
        }
        addressPopup.close()
        tabs.close(index)
        syncActiveView()
        if (tabsPopup.visible)
            tabSearch.forceActiveFocus()
    }

    function nextTab() { tabs.cycle(1); selectTab(tabs.currentIndex) }
    function previousTab() { tabs.cycle(-1); selectTab(tabs.currentIndex) }
    function reopenTab() { if (tabs.reopen()) selectTab(tabs.currentIndex) }
    function showTabs() { addressPopup.close(); tabsPopup.open() }

    function openExternalUrl(url: string) {
        syncActiveView()
        if (emptyPage && !settingsOpen) {
            navigate(url)
        } else {
            selectTab(tabs.add(url))
        }
    }

    function openWebRequest(request: var) {
        // Preserve background-tab requests and keep the request's opener and
        // pending navigation by handing it directly to the new engine view.
        const background = request.destination === WebEngineView.NewViewInBackgroundTab
        const index = tabs.add("about:blank", !background)
        const view = tabViews.itemAt(index) as WebEngineView
        if (view)
            view.acceptAsNewWindow(request)
        if (!background)
            selectTab(index)
    }

    Connections {
        target: window.tabs
        function onCurrentIndexChanged() { window.syncActiveView() }
    }

    function toggleSettings() {
        if (webView) webAuthnPanel.cancelFor(webView)
        addressPopup.close()
        tabsPopup.close()
        recordingIndex = -1
        bindingError = ""
        settingsOpen = !settingsOpen
        if (settingsOpen)
            settingsPage.forceActiveFocus()
        else if (emptyPage)
            homeAddress.forceActiveFocus()
        else
            webView.forceActiveFocus()
    }

    function openSettings() {
        if (!settingsOpen)
            toggleSettings()
    }

    function openPasswords() {
        addressPopup.close()
        tabsPopup.close()
        if (!settingsOpen) passwordPanel.openFor(webView)
    }

    function openPasswordSettings() { passwordPanel.openManager() }

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

    WebEngineProfile {
        id: browserProfile
        storageName: "Default"
        offTheRecord: false
        httpUserAgent: window.browserUserAgent
    }

    Repeater {
        id: tabViews
        model: window.tabs
        onItemAdded: Qt.callLater(window.syncActiveView)
        onItemRemoved: Qt.callLater(window.syncActiveView)
        delegate: WebEngineView {
            id: page
            required property int index
            required property int tabId
            required property url initialUrl
            property string errorText: ""
            readonly property var passwordAgent: passwordSession
            anchors.fill: parent
            visible: tabsPopup.visible || (index === window.tabs.currentIndex && !window.settingsOpen && !window.emptyPage)
            z: index === window.tabs.currentIndex ? 0 : -1
            settings.focusOnNavigationEnabled: index === window.tabs.currentIndex
            url: initialUrl
            backgroundColor: window.color
            profile: browserProfile
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
            onNewWindowRequested: function(request) { window.openWebRequest(request) }
            onWindowCloseRequested: window.finishClosingTab(index)
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
                if (page === window.webView) window.siteNavigation.setTitle(title)
            }
            onUrlChanged: {
                if (page === window.webView) window.resetSiteNavigation()
                webAuthnPanel.cancelFor(page)
                passwordSession.invalidate()
                passwordPanel.cancelFor(page)
                window.tabs.update(tabId, title, url)
                if (page === window.webView && window.emptyPage) {
                    homeAddress.text = ""
                    Qt.callLater(window.focusPage)
                }
            }
            onJavaScriptConsoleMessage: function(level, message, lineNumber, sourceID) {
                if (window.browserDiagnostics)
                    console.log("Page console:", level, sourceID, lineNumber, message)
            }
            onLoadingChanged: function(request) {
                if (request.status === WebEngineView.LoadStartedStatus) {
                    if (page === window.webView) window.resetSiteNavigation()
                    webAuthnPanel.cancelFor(page)
                    passwordSession.invalidate()
                    passwordPanel.cancelFor(page)
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
        visible: window.emptyPage && !window.settingsOpen
        color: window.palette.window

        MouseArea {
            anchors.fill: parent
            onPressed: window.startSystemMove()
        }
        Column {
            anchors.centerIn: parent
            width: Math.min(560, window.width - 64)
            spacing: 16
            Basic.Label {
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("Arbiter")
                font.pixelSize: 38
                font.weight: Font.Medium
                color: window.palette.text
            }
            AddressField {
                id: homeAddress
                width: parent.width
                Component.onCompleted: forceActiveFocus()
            }
            Basic.Label {
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("%1 to go anywhere").arg(window.keyBindings.nativeSequence(window.bindings.address))
                font.pixelSize: 12
                color: window.mutedText
            }
            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 8
                UiButton {
                    text: qsTr("Tabs · %1").arg(window.keyBindings.nativeSequence(window.bindings.tabs))
                    flat: true
                    onClicked: window.showTabs()
                }
                UiButton {
                    text: qsTr("Settings")
                    flat: true
                    onClicked: window.toggleSettings()
                }
            }
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
            && !window.webauthn.active && !addressPopup.visible && !tabsPopup.visible
        view: window.webView
        appearance: window.appearance
        tabCount: window.tabs.count
        onAddressRequested: window.focusAddress()
        onTabsRequested: window.showTabs()
        onPasswordsRequested: window.openPasswords()
        onSettingsRequested: window.openSettings()
    }
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
                    Basic.Label { text: qsTr("Your tabs"); font.pixelSize: 22; font.weight: Font.DemiBold }
                    Basic.Label { text: qsTr("%1 open · Pick up where you left off").arg(window.tabs.count); color: window.mutedText; font.pixelSize: 12 }
                }
                Item { Layout.fillWidth: true }
                UiButton { text: qsTr("New tab"); onClicked: window.newTab() }
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
        for (let i = 0; i < tabViews.count; ++i) {
            const view = tabViews.itemAt(i) as WebEngineView
            if (!view)
                continue
            const url = view.url.toString()
            const title = url === "about:blank" ? qsTr("New tab") : (view.title || url)
            if (!query || (title + " " + url).toLowerCase().includes(query))
                tabChoices.append({sourceIndex: i, pageTitle: title, pageUrl: url === "about:blank" ? qsTr("Start page") : url})
        }
        tabList.currentIndex = query ? 0 : tabs.currentIndex
    }
    Connections {
        target: tabSearch
        function onTextChanged() { window.refreshTabChoices() }
    }
    Connections {
        target: window.tabs
        function onDataChanged() { window.refreshTabChoices() }
        function onCountChanged() { Qt.callLater(window.refreshTabChoices) }
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
        onClosed: {
            if (window.settingsOpen)
                settingsPage.forceActiveFocus()
            else if (window.emptyPage)
                homeAddress.forceActiveFocus()
            else if (window.webView)
                window.webView.forceActiveFocus()
        }
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
