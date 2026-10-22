pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Effects
import QtQuick.Layouts
import QtQuick.Controls.Basic as Basic
import QtWebEngine

Basic.ApplicationWindow {
    id: window
    required property string browserUserAgent
    required property bool browserDiagnostics
    required property var tabs
    property var webView: null
    required property var keyBindings
    readonly property var bindings: keyBindings.bindings
    property bool settingsOpen: false
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
    palette.window: "#0c0c0c"
    palette.base: "#111111"
    palette.alternateBase: "#181818"
    palette.text: "#eeeeee"
    palette.windowText: "#eeeeee"
    palette.button: "#181818"
    palette.buttonText: "#eeeeee"
    palette.placeholderText: "#777777"
    palette.highlight: "#484848"
    palette.highlightedText: "#ffffff"
    palette.mid: "#333333"
    readonly property color subtleLine: "#1affffff"
    readonly property color mutedText: "#999999"
    readonly property color fieldFill: "#1c1c1c"
    readonly property color accent: "#bdbdbd"

    readonly property bool emptyPage: !webView || webView.url.toString() === "about:blank"

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
            anchors.fill: parent
            visible: tabsPopup.visible || (index === window.tabs.currentIndex && !window.settingsOpen && !window.emptyPage)
            z: index === window.tabs.currentIndex ? 0 : -1
            settings.focusOnNavigationEnabled: index === window.tabs.currentIndex
            url: initialUrl
            backgroundColor: window.color
            profile: browserProfile
            WebContextMenu {
                id: pageContextMenu
                view: page
            }
            onContextMenuRequested: function(request) { pageContextMenu.show(request) }
            onNewWindowRequested: function(request) { window.openWebRequest(request) }
            onWindowCloseRequested: window.finishClosingTab(index)
            onTitleChanged: window.tabs.update(tabId, title, url)
            onUrlChanged: {
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

    component RoundedField: Basic.TextField {
        id: field
        implicitHeight: 46
        height: implicitHeight
        leftPadding: 16
        rightPadding: 18
        topPadding: 0
        bottomPadding: 0
        font.pixelSize: 15
        verticalAlignment: TextInput.AlignVCenter
        color: window.palette.text
        placeholderTextColor: window.palette.placeholderText
        selectionColor: "#484848"
        selectedTextColor: window.palette.highlightedText
        placeholderText: qsTr("Search or enter address")
        Accessible.name: qsTr("Address and search")
        selectByMouse: true
        background: Rectangle {
            radius: 8
            color: field.activeFocus ? "#242424" : window.fieldFill
            Behavior on color { ColorAnimation { duration: 120 } }
        }
    }

    component AddressField: RoundedField {
        onAccepted: window.navigate(text)
    }

    component SurfaceButton: Basic.Button {
        id: button
        implicitHeight: 34
        implicitWidth: Math.max(34, implicitContentWidth + 28)
        padding: 8
        horizontalPadding: 14
        hoverEnabled: true
        contentItem: Basic.Label {
            text: button.text
            color: button.enabled ? window.palette.text : window.mutedText
            font: button.font
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
        background: Rectangle {
            radius: 6
            color: button.down ? "#383838"
                : button.hovered || button.highlighted || button.activeFocus ? "#2c2c2c"
                : button.flat ? "transparent" : window.fieldFill
            Behavior on color { ColorAnimation { duration: 120 } }
        }
    }

    component ModalPanel: Rectangle {
        radius: 12
        color: "#141414"
    }

    component ModalBackdrop: Rectangle {
        color: "#b3000000"
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
                SurfaceButton {
                    text: qsTr("Tabs · %1").arg(window.keyBindings.nativeSequence(window.bindings.tabs))
                    flat: true
                    onClicked: window.showTabs()
                }
                SurfaceButton {
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
        color: window.palette.window
        z: 10
        Basic.ScrollView {
            anchors.fill: parent
            anchors.margins: 40
            contentWidth: availableWidth
            ColumnLayout {
                width: Math.min(720, settingsPage.width - 80)
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 16
                RowLayout {
                    Layout.fillWidth: true
                    Basic.Label { text: qsTr("Settings"); font.pixelSize: 28; Layout.fillWidth: true }
                    SurfaceButton { text: qsTr("Done"); onClicked: window.toggleSettings() }
                }
                Basic.Label { text: qsTr("Keyboard bindings"); font.pixelSize: 20; font.weight: Font.Medium }
                Rectangle { Layout.fillWidth: true; height: 1; color: window.subtleLine }
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
                        SurfaceButton {
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
                SurfaceButton {
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

    // Window movement remains available above every website without a toolbar.
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
                SurfaceButton { text: qsTr("New tab"); onClicked: window.newTab() }
            }
            RoundedField {
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
                        color: tabChoice.highlighted ? "#303030"
                            : tabChoice.hovered ? "#242424" : "transparent"
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
                            SurfaceButton {
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
        anchors.centerIn: parent
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
            else
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
