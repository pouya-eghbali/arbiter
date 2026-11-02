pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic as Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import QtQuick.Effects
import Arbiter

Controls.ApplicationWindow {
    id: window
    width: 430
    height: 880
    visible: true
    title: "Arbiter"
    font.pixelSize: 15 * MobileMetrics.textScale
    palette.highlight: Theme.accent
    palette.highlightedText: Theme.accentText
    palette.window: Theme.window
    palette.windowText: Theme.text
    palette.text: Theme.text
    palette.base: Theme.field
    flags: Qt.Window | (Qt.platform.os === "ios" ? Qt.ExpandedClientAreaHint : 0)
    // Let the page reach the home indicator; keep controls in the safe area.
    bottomPadding: 0
    readonly property real bottomInset: window.SafeArea.margins.bottom
    readonly property color surfaceColor: browsingSurface && current.url && !browser.privateLocked ? (current.background || "white") : Theme.window
    color: surfaceColor
    onSurfaceColorChanged: browser.setSystemAppearance(surfaceColor)

    readonly property bool privateCovered: browser.privateLocked
    property string panel: browser.onboardingDone ? "" : "onboarding"
    property var current: ({})
    property var visibleTabs: []
    property string query: ""
    property var accounts: []
    property string pendingOrigin: ""
    property string pendingUsername: ""
    property string pairDesktop: ""
    property string pairPin: ""
    property int onboardingStep: 0
    property bool colorDark: Theme.dark
    readonly property bool darkMode: browser.settings.theme === "dark" || (browser.settings.theme === "system" && Application.styleHints.colorScheme === Qt.Dark)
    readonly property bool specialPanel: ["address", "tabs", "workspaces", "more", "find"].includes(panel)
    readonly property bool browsingSurface: panel === "" || panel === "find"
    NativeGlassHost { id: pageGlassCapability; visible: false }
    readonly property bool nativeFloatingOverlays: Qt.platform.os === "ios" && browser.backend === "webview" && Theme.liquidGlass && pageGlassCapability.available
    readonly property bool floatingPanel: ["address", "tabs", "workspaces", "more"].includes(panel)
    readonly property real pageCoverProgress: nativeFloatingOverlays ? Math.max(overlayProgress, floatingPanel ? .001 : 0, toast.visible ? 1 : 0) : 0
    property string pendingPanel: ""
    property string pendingTab: ""
    property string pendingAddress: ""
    property bool pendingPreviewReady: false
    property int pendingPreviewFrames: 0
    function cancelPendingPanel() { snapshotWait.stop(); pendingPanel = ""; pendingTab = ""; pendingAddress = ""; pendingPreviewReady = false; pendingPreviewFrames = 0 }
    function finishPendingPanel() {
        if (!pendingPanel || !pendingPreviewReady || pendingTab !== browser.activeId || browser.privateLocked) return;
        if (pagePreview.status !== Image.Ready || pagePreview.source.toString() !== browser.previews[pendingTab]) return;
        const next = pendingPanel;
        cancelPendingPanel();
        showPanel(next);
    }
    onFrameSwapped: {
        if (!pendingPanel || !pendingPreviewReady || pagePreview.status !== Image.Ready) return;
        // Let the decoded image reach the scene graph before removing the
        // native view. One extra frame also absorbs an already-queued swap.
        if (pendingPreviewFrames > 0) { --pendingPreviewFrames; window.update(); }
        else finishPendingPanel();
    }
    Timer {
        id: snapshotWait
        interval: 1200
        onTriggered: {
            if (pagePreview.status === Image.Ready) { window.pendingPreviewReady = true; window.pendingPreviewFrames = 1; window.update(); }
            else { window.cancelPendingPanel(); browser.message(qsTr("The page is still loading. Please try again.")); }
        }
    }
    property real previewKeyboardInset: 0
    readonly property real targetKeyboardInset: {
        const revision = SurfacePalette.keyboardRevision;
        const height = content.height;
        if (browser.backend === "preview" && previewKeyboardInset > 0) return previewKeyboardInset;
        if (SurfacePalette.nativeKeyboardAvailable && height > 0) return SurfacePalette.keyboardInset(content);
        return Qt.inputMethod.visible && Qt.inputMethod.keyboardRectangle.y > 0 ? Math.max(0, height - window.keyboardTop()) : 0;
    }
    property real keyboardInset: targetKeyboardInset
    Behavior on keyboardInset {
        NumberAnimation {
            duration: browser.privateLocked || Theme.reducedMotion ? 0 : SurfacePalette.nativeKeyboardAvailable ? SurfacePalette.keyboardDuration : 250
            easing.type: SurfacePalette.nativeKeyboardAvailable ? SurfacePalette.keyboardEasing : Easing.OutCubic
        }
    }
    readonly property real overlayProgress: Math.max(addressSheet.progress, moreSheet.progress, tabsOverview.progress, workspacesSheet.progress)
    Connections {
        target: browser
        function onPreviewReady(tabId) {
            if (window.pendingTab !== tabId) return;
            window.pendingPreviewReady = true;
            window.pendingPreviewFrames = 2;
            window.update();
        }
    }
    function keyboardTop() {
        const revision = SurfacePalette.revision;
        return SurfacePalette.keyboardTop(content, Qt.inputMethod.keyboardRectangle.y);
    }
    onKeyboardInsetChanged: Qt.callLater(keepFieldVisible)
    onActiveFocusItemChanged: Qt.callLater(keepFieldVisible)
    function keepFieldVisible() {
        const field = window.activeFocusItem;
        if (!field || window.specialPanel || !pageScroll.visible)
            return;
        let ancestor = field.parent;
        while (ancestor && ancestor !== pageBody)
            ancestor = ancestor.parent;
        if (ancestor !== pageBody)
            return;
        const top = field.mapToItem(pageBody, 0, 0).y;
        const viewport = pageScroll.availableHeight;
        const currentY = pageScroll.contentItem.contentY;
        if (top + field.height + 16 > currentY + viewport)
            pageScroll.contentItem.contentY = Math.max(0, top + field.height + 16 - viewport);
        else if (top - 16 < currentY)
            pageScroll.contentItem.contentY = Math.max(0, top - 16);
    }
    readonly property Item glassBackdrop: backdropScene
    readonly property string workspaceName: (browser.workspaces.find(w => w.id === browser.workspaceId) || {}).name || qsTr("Personal")
    // Use the resting toolbar geometry even while a sheet hides the pill. Opening
    // a browser panel must not resize/reflow the website behind the glass.
    readonly property real toolbarBottomInset: pill.height + Math.max(14, bottomInset + 8) + 8
    readonly property rect nativeOverlayRect: {
        const control = panel === "find" ? findBar : pill;
        if (control.nativeActive) return Qt.rect(0, 0, 0, 0);
        if (!browsingSurface || !control.visible || content.width <= 0 || content.height <= 0)
            return Qt.rect(0, 0, 0, 0);
        return Qt.rect(control.x / content.width, control.y / content.height, control.width / content.width, control.height / content.height);
    }
    function refresh() {
        const next = browser.tabs.find(t => t.id === browser.activeId) || {};
        if (pendingTab && (pendingTab !== browser.activeId || browser.privateLocked || pendingAddress !== (next.url || ""))) cancelPendingPanel();
        if (browser.privateLocked && panel !== "onboarding") {
            panel = "";
            Qt.inputMethod.hide();
        }
        current = next;
        visibleTabs = browser.tabs.filter(t => t.workspace === browser.workspaceId && Boolean(t.private) === browser.privateMode);
    }
    function open(name) {
        cancelPendingPanel();
        // Native glass samples the live WebView. Glacier gets a decoded preview
        // before hiding that view; switching between panels never recaptures it.
        if (!nativeFloatingOverlays && browsingSurface && !toast.visible && current.url && !current.sleeping && name !== "find" && browser.backend !== "preview") {
            pendingPanel = name;
            pendingTab = browser.activeId;
            pendingAddress = current.url;
            snapshotWait.restart();
            browser.captureActive();
            return;
        }
        showPanel(name);
    }
    function showPanel(name) {
        if (name !== "address" && name !== "find")
            Qt.inputMethod.hide();
        panel = name;
        query = "";
        pageScroll.contentItem.contentY = 0;
    }
    function dismiss() {
        cancelPendingPanel();
        if (panel === "settings" && settingsLoader.item && settingsLoader.item.category) {
            settingsLoader.item.category = "";
            return;
        }
        Qt.inputMethod.hide();
        if (panel === "find")
            browser.findText("");
        panel = "";
    }
    function go(value) {
        browser.navigate(value);
        dismiss();
    }
    function moreAction(name) {
        if (name === "share") {
            dismiss();
            browser.share();
        } else if (name === "reload") {
            dismiss();
            if (window.current.sleeping)
                browser.selectTab(browser.activeId);
            else
                browser.pageAction(browser.activeId, "reload", "");
        } else if (name === "block") {
            browser.toggleSiteBlocking();
            dismiss();
        } else if (name === "private") {
            if (browser.privateMode)
                browser.togglePrivate();
            else
                browser.newPrivateTab();
            dismiss();
        } else
            open(name);
    }
    Shortcut {
        sequence: "Escape"
        enabled: window.panel !== "" && window.panel !== "onboarding"
        onActivated: window.dismiss()
    }
    Shortcut {
        sequence: "Back"
        enabled: browser.privateLocked || window.panel !== "" && window.panel !== "onboarding" || Boolean(window.current.back)
        onActivated: {
            if (browser.privateLocked)
                browser.togglePrivate();
            else if (window.panel !== "")
                window.dismiss();
            else
                browser.pageAction(browser.activeId, "back", "");
        }
    }
    Component.onCompleted: {
        refresh();
        browser.setSystemAppearance(surfaceColor);
    }
    Connections {
        target: browser
        function onChanged() {
            window.refresh();
        }
        function onMessage(text) {
            toast.text = text;
            toast.open();
        }
        function onLoginOffered(origin, username) {
            window.pendingOrigin = origin;
            window.pendingUsername = username;
            window.open("save");
        }
        function onAccountsOffered(accounts) {
            window.accounts = accounts;
            window.open("accounts");
        }
    }
    Connections {
        target: browser.vault
        function onPairingReview(desktop, pin) {
            window.pairDesktop = desktop;
            window.pairPin = pin;
            window.open("pairReview");
        }
    }
    Binding { target: Theme; property: "liquidGlass"; value: browser.settings.liquidGlass === true }
    Binding {
        target: Theme
        property: "dark"
        value: window.darkMode
    }
    Binding {
        target: Theme
        property: "lightAccent"
        value: browser.settings.lightAccent
    }
    Binding {
        target: Theme
        property: "darkAccent"
        value: browser.settings.darkAccent
    }
    Binding {
        target: Theme
        property: "lightBackground"
        value: browser.settings.lightBackground
    }
    Binding {
        target: Theme
        property: "darkBackground"
        value: browser.settings.darkBackground
    }

    Item {
        id: content
        anchors.fill: parent
        focus: true
        Keys.onBackPressed: event => {
            if (window.pendingPanel) { window.cancelPendingPanel(); event.accepted = true; return; }
            if (window.panel !== "" && window.panel !== "onboarding") {
                window.dismiss();
                event.accepted = true;
            } else if (window.current.back) {
                browser.pageAction(browser.activeId, "back", "");
                event.accepted = true;
            } else
                event.accepted = false;
        }
        Keys.onEscapePressed: event => {
            if (window.pendingPanel) { window.cancelPendingPanel(); event.accepted = true; return; }
            if (window.panel !== "" && window.panel !== "onboarding") {
                window.dismiss();
                event.accepted = true;
            } else
                event.accepted = false;
        }
        Item {
            id: backdropScene
            anchors.fill: parent
            Image {
                anchors.fill: parent
                source: (Theme.dark ? browser.settings.darkImage : browser.settings.lightImage) || ""
                fillMode: Image.PreserveAspectCrop
                opacity: .6
                visible: source.toString().length > 0
            }
            Image {
                id: pagePreview
                objectName: "pagePreview"
                anchors.fill: parent
                onStatusChanged: if (status === Image.Ready) window.update()
                source: browser.previews[browser.activeId] || ""
                fillMode: Image.Stretch
                cache: false
                visible: Boolean(window.current.url) && !browser.privateLocked
            }
        }
        Repeater {
            model: browser.screens
            delegate: Loader {
                id: pageLoader
                required property var tab
                readonly property var record: tab.record
                anchors.fill: parent
                active: false
                visible: record.id === browser.activeId && !browser.privateLocked
                    && (window.browsingSurface || window.nativeFloatingOverlays && window.floatingPanel)
                    && (window.nativeFloatingOverlays || !toast.visible)
                function configure() {
                    if (record.sleeping || !record.url.length) {
                        active = false;
                        return;
                    }
                    if (browser.backend === "webengine" && !engineProfiles.item) {
                        active = false;
                        return;
                    }
                    if (!active || !source.toString().length) {
                        const properties = {
                            record: Qt.binding(() => pageLoader.record),
                            presented: Qt.binding(() => pageLoader.visible),
                            coverProgress: Qt.binding(() => window.pageCoverProgress),
                            overlayRect: Qt.binding(() => window.nativeOverlayRect)
                        };
                        if (browser.backend === "webengine")
                            properties.pageProfile = engineProfiles.profile(record);
                        else
                            properties.toolbarBottomInset = Qt.binding(() => Qt.platform.os === "ios" ? window.toolbarBottomInset : 0);
                        setSource(browser.backend === "webengine" ? "WebEnginePane.qml" : "NativePane.qml", properties);
                    }
                    active = true;
                }
                // Creating a native page can synchronously publish its first state.
                // Defer until the tab-record binding has finished evaluating.
                onRecordChanged: Qt.callLater(configure)
                Component.onCompleted: Qt.callLater(configure)
                Connections {
                    target: engineProfiles
                    function onLoaded() {
                        pageLoader.configure();
                    }
                }
            }
        }
        MobileStartPage {
            anchors.fill: parent
            anchors.bottomMargin: 90 + window.bottomInset
            visible: !window.current.url && window.panel === "" && !browser.privateLocked
            onNavigate: url => window.go(url)
        }
        ColumnLayout {
            anchors.centerIn: parent
            width: Math.min(parent.width - 48, 420)
            spacing: 20
            visible: Boolean(window.current.sleeping && window.current.error) && window.panel === "" && !browser.privateLocked
            Text {
                textFormat: Text.PlainText
                text: window.current.error || ""
                color: Theme.textFor(parent)
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
            MobileButton {
                text: qsTr("Reload")
                onClicked: browser.selectTab(browser.activeId)
            }
        }
        ColumnLayout {
            visible: browser.privateLocked && window.panel === ""
            anchors.centerIn: parent
            width: Math.min(parent.width - 48, 420)
            spacing: 22
            UiIcon {
                name: "lock"
                Layout.preferredWidth: MobileMetrics.touchTarget
                Layout.preferredHeight: MobileMetrics.touchTarget
                Layout.alignment: Qt.AlignHCenter
                color: Theme.accent
            }
            Text {
                textFormat: Text.PlainText
                text: qsTr("Private browsing is locked")
                font.pixelSize: 25 * MobileMetrics.textScale
                color: Theme.textFor(parent)
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
            }
            MobileButton {
                Layout.fillWidth: true
                text: qsTr("Unlock")
                primary: true
                onClicked: browser.unlockPrivate()
            }
            MobileButton {
                Layout.fillWidth: true
                text: qsTr("Return to regular browsing")
                onClicked: browser.togglePrivate()
            }
        }
        ShaderEffectSource {
            id: backdropTexture
            sourceItem: backdropScene
            anchors.fill: parent
            visible: false
            live: true
        }
        MultiEffect {
            anchors.fill: parent
            source: backdropTexture
            blurEnabled: true
            blurMax: 48
            blur: .65
            autoPaddingEnabled: false
            visible: !browser.privateLocked && window.overlayProgress > 0.001
            opacity: window.overlayProgress
            saturation: -.1
        }
        MobilePill {
            id: pill
            x: 16
            width: parent.width - 32
            y: parent.height - height - Math.max(14, window.bottomInset + 8) - window.keyboardInset + 12 * (1 - pill.progress)
            height: implicitHeight
            current: window.current
            enabled: !browser.privateLocked
            tabCount: window.visibleTabs.length
            suggestions: browser.fieldFocused
            presented: window.panel === "" && !browser.privateLocked
            concealed: browser.privateLocked
            onSearchRequested: window.open("address")
            onTabsRequested: window.open("tabs")
            onMoreRequested: window.open("more")
            onCycleRequested: direction => {
                browser.captureActive();
                browser.cycleTab(direction);
            }
        }
        MobilePresentation {
            id: findPresentation
            anchors.fill: parent
            presented: window.panel === "find"
            concealed: browser.privateLocked
            onPresentedChanged: if (presented) Qt.callLater(() => { if (findPresentation.presented) findBar.focusControl(findInput) }); else findBar.desiredFocus = null
            NativeSheet {
                id: findBar
                onEscapeRequested: window.dismiss()
                x: 12
                width: parent.width - 24
                height: 120
                y: parent.height - height - Math.max(12, window.bottomInset + 8) - window.keyboardInset + 16 * (1 - findPresentation.progress)
                interactive: findPresentation.presented
                radius: 26
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 6
                    RowLayout {
                        Layout.fillWidth: true
                        UiIcon {
                            name: "search"
                            color: Theme.mutedFor(parent)
                            Layout.preferredWidth: 18
                            Layout.preferredHeight: 18
                        }
                        MobileField {
                            id: findInput
                            Layout.fillWidth: true
                            placeholderText: qsTr("Find in page")
                            onTextEdited: browser.findText(text)
                            onAccepted: browser.findText(text)
                        }
                        MobileCloseButton {
                            label: qsTr("Close")
                            onClicked: window.dismiss()
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            text: qsTr("Find in page")
                            color: Theme.mutedFor(parent)
                            font.pixelSize: 12 * MobileMetrics.textScale
                            Layout.fillWidth: true
                        }
                        MobileIconButton {
                            iconName: "arrow-up"
                            label: qsTr("Previous match")
                            onClicked: browser.findText(findInput.text, true)
                        }
                        MobileIconButton {
                            iconName: "arrow-down"
                            label: qsTr("Next match")
                            onClicked: browser.findText(findInput.text)
                        }
                    }
                }
                onVisibleChanged: if (!visible) desiredFocus = null
            }
        }

        Rectangle {
            anchors.fill: parent
            color: Qt.alpha(Theme.window, .35)
            visible: !browser.privateLocked && (tabsOverview.progress > 0.001 || workspacesSheet.progress > 0.001)
            opacity: Math.max(tabsOverview.progress, workspacesSheet.progress)
            TapHandler { onTapped: window.dismiss() }
        }
        MobileTabsOverview {
            id: tabsOverview
            anchors.fill: parent
            anchors.bottomMargin: window.bottomInset
            presented: window.panel === "tabs"
            concealed: browser.privateLocked
            tabs: window.visibleTabs
            workspaceName: window.workspaceName
            onDismiss: window.dismiss()
            onWorkspacesRequested: window.open("workspaces")
        }
        MobileWorkspaces {
            id: workspacesSheet
            objectName: "workspaceScreen"
            keyboardInset: window.keyboardInset
            anchors.fill: parent
            anchors.bottomMargin: window.bottomInset
            presented: window.panel === "workspaces"
            concealed: browser.privateLocked
            onDismiss: window.dismiss()
        }
        MobileSearchSheet {
            id: addressSheet
            objectName: "mobileAddressSheet"
            anchors.fill: parent
            presented: window.panel === "address"
            concealed: browser.privateLocked
            address: window.current.url || ""
            keyboardInset: Math.max(window.keyboardInset, window.bottomInset)
            onDismiss: window.dismiss()
            onNavigate: value => window.go(value)
            onTabSelected: id => {
                browser.selectTab(id);
                window.dismiss();
            }
        }
        MobileMoreSheet {
            id: moreSheet
            anchors.fill: parent
            bottomInset: window.bottomInset
            presented: window.panel === "more"
            concealed: browser.privateLocked
            hasPage: Boolean(window.current.url)
            blocking: browser.blockingFor(window.current.url || "")
            onDismiss: window.dismiss()
            onAction: name => window.moreAction(name)
        }
    }
    // Sheets replace the native surface; all controls remain QML, including on phones.
    Rectangle {
        anchors.fill: parent
        visible: window.panel !== "" && !window.specialPanel
        color: Theme.window
        ColumnLayout {
            width: Math.min(parent.width - 40, 560)
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.top
            anchors.topMargin: 12
            anchors.bottom: parent.bottom
            anchors.bottomMargin: Math.max(window.bottomInset, window.keyboardInset) + 16
            spacing: 16
            RowLayout {
                Layout.fillWidth: true
                MobileIconButton {
                    visible: window.panel === "settings" && Boolean(settingsLoader.item && settingsLoader.item.category)
                    iconName: "arrow-left"
                    label: qsTr("Settings")
                    onClicked: window.dismiss()
                }
                Text {
                    textFormat: Text.PlainText
                    Layout.fillWidth: true
                    text: window.panel === "onboarding" ? "Arbiter" : window.panel === "settings" ? (settingsLoader.item ? settingsLoader.item.pageTitle : qsTr("Settings")) : window.panel === "history" ? qsTr("History") : window.panel === "downloads" ? qsTr("Downloads") : window.panel === "vault" ? qsTr("Passwords & passkeys") : window.panel === "save" ? qsTr("Save password?") : window.panel === "accounts" ? qsTr("Passwords") : qsTr("Pair with your desktop")
                    font.pixelSize: 24 * MobileMetrics.textScale
                    wrapMode: Text.WordWrap
                    font.weight: Font.DemiBold
                    color: Theme.textFor(parent)
                }
                MobileIconButton {
                    iconName: "x"
                    label: qsTr("Close")
                    visible: window.panel !== "onboarding"
                    onClicked: {
                        if (window.panel === "save")
                            browser.dismissLogin();
                        Qt.inputMethod.hide();
                        window.panel = "";
                    }
                }
            }
            MobileField {
                id: filter
                visible: window.panel === "history" || window.panel === "vault" && browser.vault.unlocked
                Layout.fillWidth: true
                placeholderText: qsTr("Search")
                text: window.query
                onTextEdited: window.query = text
                onAccepted: if (window.panel === "address")
                    window.go(text)
            }
            Controls.ScrollView {
                id: pageScroll
                objectName: "pageScroll"
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                contentWidth: availableWidth
                Controls.ScrollBar.horizontal.policy: Controls.ScrollBar.AlwaysOff
                ColumnLayout {
                    id: pageBody
                    width: pageScroll.availableWidth
                    spacing: 16
                    Repeater {
                        id: historyRows
                        model: window.panel === "history" ? browser.history.filter(t => !window.query || t.url.toLowerCase().includes(window.query.toLowerCase()) || t.title.toLowerCase().includes(window.query.toLowerCase())) : []
                        delegate: Rectangle {
                            required property var modelData
                            Layout.fillWidth: true
                            implicitHeight: 88
                            radius: 16
                            color: Theme.fieldFor(parent)
                            MobileButton {
                                anchors.fill: parent
                                text: ""
                                flat: true
                                Accessible.name: modelData.title || modelData.url
                                background: Item {}
                                onClicked: window.go(modelData.url)
                            }
                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: 12
                                spacing: 12
                                UiIcon {
                                    name: "history"
                                    color: Theme.mutedFor(parent)
                                    Layout.preferredWidth: 21
                                    Layout.preferredHeight: 21
                                }
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 4
                                    Text {
                                        text: modelData.title || modelData.url
                                        textFormat: Text.PlainText
                                        color: Theme.textFor(parent)
                                        font.pixelSize: 15 * MobileMetrics.textScale
                                        elide: Text.ElideRight
                                        Layout.fillWidth: true
                                    }
                                    Text {
                                        text: modelData.url.replace(/^https?:\/\//, "")
                                        textFormat: Text.PlainText
                                        color: Theme.mutedFor(parent)
                                        font.pixelSize: 12 * MobileMetrics.textScale
                                        elide: Text.ElideRight
                                        Layout.fillWidth: true
                                    }
                                    Text {
                                        text: new Date(modelData.time).toLocaleDateString(Qt.locale(), Locale.ShortFormat)
                                        color: Theme.mutedFor(parent)
                                        font.pixelSize: 11 * MobileMetrics.textScale
                                        Layout.fillWidth: true
                                    }
                                }
                                MobileIconButton {
                                    iconName: "trash-2"
                                    label: qsTr("Delete")
                                    onClicked: browser.removeHistory(modelData.url)
                                }
                            }
                        }
                    }
                    MobileEmptyState {
                        Layout.fillWidth: true
                        visible: window.panel === "history" && historyRows.count === 0
                        iconName: "history"
                        title: window.query ? qsTr("No matching history") : qsTr("No history yet")
                    }
                    MobileButton {
                        Layout.fillWidth: true
                        visible: window.panel === "history" && historyRows.count > 0
                        text: window.query ? qsTr("Delete matching history") : qsTr("Clear history")
                        iconName: "trash-2"
                        flat: true
                        onClicked: historyConfirmation.open()
                    }
                    Repeater {
                        model: window.panel === "downloads" ? browser.downloads : []
                        delegate: MobileSection {
                            required property var modelData
                            Layout.fillWidth: true
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 12
                                UiIcon {
                                    name: "download"
                                    color: Theme.accent
                                    Layout.preferredWidth: 24
                                    Layout.preferredHeight: 24
                                }
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 6
                                    Text {
                                        text: modelData.name || modelData.url
                                        textFormat: Text.PlainText
                                        color: Theme.textFor(parent)
                                        font.pixelSize: 15 * MobileMetrics.textScale
                                        elide: Text.ElideMiddle
                                        Layout.fillWidth: true
                                    }
                                    Text {
                                        text: modelData.status || qsTr("Downloading…")
                                        textFormat: Text.PlainText
                                        color: Theme.mutedFor(parent)
                                        font.pixelSize: 13 * MobileMetrics.textScale
                                        wrapMode: Text.WordWrap
                                        Layout.fillWidth: true
                                    }
                                }
                                MobileIconButton {
                                    iconName: "external-link"
                                    label: qsTr("Open")
                                    enabled: modelData.complete || false
                                    onClicked: browser.openDownload(modelData.id)
                                }
                            }
                            Controls.ProgressBar {
                                Layout.fillWidth: true
                                visible: !modelData.complete && !modelData.error
                                from: 0
                                to: Math.max(1, modelData.total || 1)
                                value: modelData.received || 0
                            }
                        }
                    }
                    MobileEmptyState {
                        Layout.fillWidth: true
                        visible: window.panel === "downloads" && browser.downloads.length === 0
                        iconName: "download"
                        title: qsTr("No downloads yet")
                    }
                    Text {
                        textFormat: Text.PlainText
                        visible: window.panel === "vault"
                        text: qsTr("Saved passwords and passkeys stay encrypted on this device.")
                        wrapMode: Text.WordWrap
                        color: Theme.mutedFor(parent)
                        Layout.fillWidth: true
                    }
                    MobileButton {
                        Layout.fillWidth: true
                        visible: window.panel === "vault" && !browser.vault.unlocked
                        text: qsTr("Unlock")
                        primary: true
                        iconName: "fingerprint"
                        onClicked: browser.vault.unlock()
                    }
                    Repeater {
                        model: window.panel === "vault" ? browser.vault.entries.filter(t => !window.query || JSON.stringify(t).toLowerCase().includes(window.query.toLowerCase())) : []
                        delegate: RowLayout {
                            required property var modelData
                            Layout.fillWidth: true
                            UiIcon {
                                name: modelData.kind === "passkey" ? "fingerprint" : "key-round"
                                Layout.preferredWidth: 24
                                Layout.preferredHeight: 24
                                color: Theme.accent
                            }
                            ColumnLayout {
                                Layout.fillWidth: true
                                Text {
                                    textFormat: Text.PlainText
                                    text: modelData.origin || modelData.rpId
                                    color: Theme.textFor(parent)
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }
                                Text {
                                    textFormat: Text.PlainText
                                    text: modelData.username || ""
                                    color: Theme.mutedFor(parent)
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }
                            }
                            MobileIconButton {
                                iconName: "trash-2"
                                label: qsTr("Delete")
                                onClicked: browser.vault.remove(modelData.id)
                            }
                        }
                    }
                    Text {
                        textFormat: Text.PlainText
                        visible: window.panel === "vault" && browser.vault.conflicts > 0
                        text: qsTr("Both versions were kept. Choose which change to keep on your devices.")
                        wrapMode: Text.WordWrap
                        color: Theme.mutedFor(parent)
                        Layout.fillWidth: true
                    }
                    Repeater {
                        model: window.panel === "vault" ? browser.vault.conflictChoices : []
                        delegate: MobileButton {
                            Layout.fillWidth: true
                            required property var modelData
                            text: (modelData.origin || modelData.rpId || "") + " · " + (modelData.username || "") + " · " + modelData.device + " — " + (modelData.deleted ? qsTr("Delete everywhere") : qsTr("Keep this version"))
                            enabled: !browser.vault.busy
                            textAlignment: Text.AlignLeft
                            onClicked: browser.vault.resolveConflict(modelData.id, modelData.version, modelData.generation)
                        }
                    }
                    Text {
                        textFormat: Text.PlainText
                        visible: window.panel === "vault" && browser.vault.error.length > 0
                        text: browser.vault.error
                        wrapMode: Text.WordWrap
                        color: Theme.textFor(parent)
                        Layout.fillWidth: true
                    }
                    MobileButton {
                        Layout.fillWidth: true
                        visible: window.panel === "vault"
                        text: browser.vault.peerName ? qsTr("Sync now") : qsTr("Pair with your desktop")
                        iconName: "refresh-cw"
                        enabled: !browser.vault.busy
                        onClicked: {
                            if (browser.vault.peerName)
                                browser.vault.sync();
                            else
                                window.open("pair");
                        }
                    }
                    MobileButton {
                        Layout.fillWidth: true
                        visible: window.panel === "vault"
                        text: qsTr("Enable credential provider")
                        onClicked: browser.enableCredentialProvider()
                    }
                    MobileButton {
                        Layout.fillWidth: true
                        visible: window.panel === "vault" && browser.vault.unlocked
                        text: qsTr("Lock")
                        onClicked: browser.vault.lock()
                    }
                    Controls.BusyIndicator {
                        visible: ["pair", "pairReview", "scan", "vault"].includes(window.panel) && browser.vault.busy
                        running: visible
                        Layout.alignment: Qt.AlignHCenter
                        Layout.preferredWidth: 32
                        Layout.preferredHeight: 32
                    }
                    Text {
                        visible: ["pair", "pairReview", "scan"].includes(window.panel) && browser.vault.error.length > 0
                        text: browser.vault.error
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        color: Theme.textFor(parent)
                    }
                    Text {
                        textFormat: Text.PlainText
                        visible: window.panel === "pair"
                        text: qsTr("In Arbiter on desktop, open Settings → Sync, unlock the vault and copy its pairing invitation. Verify the desktop name and certificate fingerprint before pairing.")
                        color: Theme.mutedFor(parent)
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                    MobileButton {
                        Layout.fillWidth: true
                        visible: window.panel === "pair"
                        text: qsTr("Scan QR code")
                        iconName: "qr-code"
                        onClicked: window.open("scan")
                    }
                    MobileEmptyState {
                        Layout.fillWidth: true
                        visible: window.panel === "scan" && browser.backend === "preview"
                        title: qsTr("Scan QR code")
                        iconName: "qr-code"
                    }
                    Loader {
                        visible: window.panel === "scan"
                        active: visible && browser.backend !== "preview"
                        Layout.fillWidth: true
                        source: "PairingScanner.qml"
                        onLoaded: item.invitation.connect(value => {
                            browser.vault.pair(value);
                        })
                    }
                    MobileField {
                        id: invitation
                        onAccepted: if (text.length && !browser.vault.busy)
                            browser.vault.pair(text)
                        visible: window.panel === "pair"
                        Layout.fillWidth: true
                        placeholderText: qsTr("Pairing invitation")
                        echoMode: TextInput.Password
                    }
                    MobileButton {
                        Layout.fillWidth: true
                        visible: window.panel === "pair"
                        text: qsTr("Continue")
                        primary: true
                        enabled: invitation.text.trim().length > 0 && !browser.vault.busy
                        onClicked: browser.vault.pair(invitation.text)
                    }
                    Text {
                        textFormat: Text.PlainText
                        visible: window.panel === "pairReview"
                        text: window.pairDesktop
                        font.pixelSize: 18 * MobileMetrics.textScale
                        font.weight: Font.Medium
                        color: Theme.textFor(parent)
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                    Text {
                        visible: window.panel === "pairReview"
                        text: window.pairPin
                        font.pixelSize: 32 * MobileMetrics.textScale
                        font.family: "monospace"
                        font.letterSpacing: 2
                        color: Theme.accent
                        Layout.fillWidth: true
                        horizontalAlignment: Text.AlignHCenter
                        Layout.topMargin: 12
                        Layout.bottomMargin: 12
                    }
                    MobileButton {
                        Layout.fillWidth: true
                        visible: window.panel === "pairReview"
                        text: qsTr("Pair")
                        primary: true
                        enabled: !browser.vault.busy
                        onClicked: {
                            browser.vault.confirmPair();
                            window.open("vault");
                        }
                    }
                    Text {
                        textFormat: Text.PlainText
                        visible: window.panel === "save"
                        text: window.pendingOrigin
                        color: Theme.mutedFor(parent)
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                    MobileField {
                        id: saveUser
                        visible: window.panel === "save"
                        text: window.pendingUsername
                        placeholderText: qsTr("Username")
                        Layout.fillWidth: true
                    }
                    MobileButton {
                        Layout.fillWidth: true
                        visible: window.panel === "save"
                        text: qsTr("Save")
                        primary: true
                        onClicked: {
                            browser.saveLogin(saveUser.text);
                            window.panel = "";
                        }
                    }
                    MobileButton {
                        Layout.fillWidth: true
                        visible: window.panel === "save"
                        text: qsTr("Not now")
                        onClicked: {
                            browser.dismissLogin();
                            window.panel = "";
                        }
                    }
                    Repeater {
                        model: window.panel === "accounts" ? window.accounts : []
                        delegate: MobileButton {
                            Layout.fillWidth: true
                            required property var modelData
                            text: modelData.username || qsTr("Saved account")
                            onClicked: {
                                browser.fillPassword(modelData.id);
                                window.panel = "";
                            }
                        }
                    }
                    MobileEmptyState {
                        Layout.fillWidth: true
                        visible: window.panel === "accounts" && window.accounts.length === 0
                        iconName: "key-round"
                        title: qsTr("No saved accounts")
                    }
                    MobileEmptyState {
                        Layout.fillWidth: true
                        visible: window.panel === "vault" && browser.vault.unlocked && browser.vault.entries.length === 0
                        iconName: "key-round"
                        title: qsTr("No saved passwords or passkeys")
                    }
                    Loader {
                        id: settingsLoader
                        objectName: "settingsLoader"
                        active: window.panel === "settings"
                        Layout.fillWidth: true
                        source: "MobileSettings.qml"
                        onLoaded: {
                            item.pairRequested.connect(() => window.open("pair"));
                            item.vaultRequested.connect(() => window.open("vault"));
                            item.historyRequested.connect(() => window.open("history"));
                            item.downloadsRequested.connect(() => window.open("downloads"));
                        }
                    }
                    Connections {
                        target: settingsLoader.item
                        function onCategoryChanged() {
                            pageScroll.contentItem.contentY = 0;
                        }
                    }
                    ColumnLayout {
                        visible: window.panel === "onboarding"
                        Layout.fillWidth: true
                        Layout.topMargin: 16
                        spacing: 20
                        MobileLogo {
                            Layout.preferredWidth: window.onboardingStep === 0 ? 80 : 48
                            Layout.preferredHeight: window.onboardingStep === 0 ? 80 : 48
                            Layout.alignment: Qt.AlignHCenter
                        }
                        Text {
                            textFormat: Text.PlainText
                            Layout.fillWidth: true
                            text: window.onboardingStep === 0 ? qsTr("Welcome to Arbiter") : window.onboardingStep === 1 ? qsTr("Make yourself at home") : window.onboardingStep === 2 ? qsTr("Bring your vault along") : qsTr("You’re ready")
                            font.pixelSize: 30 * MobileMetrics.textScale
                            font.weight: Font.DemiBold
                            color: Theme.textFor(parent)
                            wrapMode: Text.WordWrap
                        }
                        Text {
                            textFormat: Text.PlainText
                            Layout.fillWidth: true
                            color: Theme.mutedFor(parent)
                            wrapMode: Text.WordWrap
                            text: window.onboardingStep === 0 ? qsTr("A quieter browser. Your tabs, your spaces, your rules.") : window.onboardingStep === 1 ? qsTr("Choose how Arbiter feels. You can change everything later in Settings.") : window.onboardingStep === 2 ? qsTr("Sync passwords and passkeys directly with your desktop over the same Wi-Fi. No cloud account.") : qsTr("Use the pill to search, go back, share a page, and open your tabs. Workspaces keep different parts of your life together. Private browsing locks when you leave the app.")
                        }
                        MobileLanguagePicker {
                            visible: window.onboardingStep === 0
                            Layout.fillWidth: true
                        }
                        RowLayout {
                            visible: window.onboardingStep === 1
                            Layout.fillWidth: true
                            Repeater {
                                model: [[qsTr("System"), "system"], [qsTr("Light"), "light"], [qsTr("Dark"), "dark"]]
                                delegate: MobileButton {
                                    Layout.fillWidth: true
                                    Layout.preferredWidth: 1
                                    required property var modelData
                                    text: modelData[0]
                                    checkable: true
                                    checked: browser.settings.theme === modelData[1]
                                    onClicked: browser.setSetting("theme", modelData[1])
                                }
                            }
                        }
                        MobileColors {
                            visible: window.onboardingStep === 1
                            Layout.fillWidth: true
                        }
                        MobileToggle {
                            visible: window.onboardingStep === 0
                            text: qsTr("Block ads and trackers")
                            checked: browser.settings.adblockEnabled
                            onToggled: browser.setSetting("adblockEnabled", checked)
                        }
                        MobileButton {
                            Layout.fillWidth: true
                            visible: window.onboardingStep === 2
                            text: qsTr("Pair with your desktop")
                            onClicked: {
                                browser.setSetting("onboardingDone", true);
                                window.open("pair");
                            }
                        }
                        MobileButton {
                            Layout.fillWidth: true
                            visible: window.onboardingStep === 2 && browser.defaultBrowserAvailable
                            text: qsTr("Set as default browser")
                            onClicked: browser.setDefaultBrowser()
                        }
                    }
                }
            }
            ColumnLayout {
                visible: window.panel === "onboarding"
                Layout.fillWidth: true
                spacing: 8
                RowLayout {
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignHCenter
                    spacing: 6
                    Item {
                        Layout.fillWidth: true
                    }
                    Repeater {
                        model: 4
                        delegate: Rectangle {
                            required property int index
                            Layout.preferredWidth: index === window.onboardingStep ? 20 : 6
                            Layout.preferredHeight: 6
                            radius: 3
                            color: index === window.onboardingStep ? Theme.accent : Theme.borderFor(parent)
                        }
                    }
                    Item {
                        Layout.fillWidth: true
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    MobileIconButton {
                        visible: window.onboardingStep > 0
                        iconName: "arrow-left"
                        label: qsTr("Back")
                        onClicked: {
                            window.onboardingStep--;
                            pageScroll.contentItem.contentY = 0;
                        }
                    }
                    MobileButton {
                        Layout.fillWidth: true
                        primary: true
                        text: window.onboardingStep === 3 ? qsTr("Start browsing") : qsTr("Continue")
                        onClicked: {
                            if (window.onboardingStep === 3) {
                                browser.setSetting("onboardingDone", true);
                                window.panel = "";
                            } else
                                window.onboardingStep++;
                            pageScroll.contentItem.contentY = 0;
                        }
                    }
                }
                MobileButton {
                    Layout.fillWidth: true
                    flat: true
                    text: qsTr("Skip setup")
                    onClicked: {
                        browser.setSetting("onboardingDone", true);
                        window.panel = "";
                    }
                }
            }
        }
    }
    UiPopup {
        id: historyConfirmation
        objectName: "mobileHistoryConfirmation"
        anchors.centerIn: parent
        width: Math.min(parent.width - 40, 400)
        modal: true
        focus: true
        padding: 20
        background: GlassSurface {
            radius: 24
            dense: true
        }
        contentItem: ColumnLayout {
            spacing: 20
            Text {
                text: qsTr("Delete history?")
                font.pixelSize: 22 * MobileMetrics.textScale
                font.weight: Font.DemiBold
                color: Theme.textFor(parent)
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
            Text {
                text: window.query ? qsTr("Delete matching history") : qsTr("Clear history")
                color: Theme.mutedFor(parent)
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
            MobileButton {
                Layout.fillWidth: true
                primary: true
                text: qsTr("Delete")
                onClicked: {
                    browser.clearHistory(window.query);
                    historyConfirmation.close();
                }
            }
            MobileButton {
                Layout.fillWidth: true
                flat: true
                text: qsTr("Cancel")
                onClicked: historyConfirmation.close()
            }
        }
    }
    UiPopup {
        id: toast
        focus: false
        closePolicy: Controls.Popup.NoAutoClose
        property string text: ""
        anchors.centerIn: parent
        width: Math.min(parent.width - 40, 420)
        padding: 20
        contentItem: Text {
            textFormat: Text.PlainText
            text: toast.text
            color: Theme.textFor(parent)
            wrapMode: Text.WordWrap
        }
        background: GlassSurface { radius: 18; dense: true }
        onOpened: toastTimer.restart()
        Timer {
            id: toastTimer
            interval: 4000
            onTriggered: toast.close()
        }
    }
    Loader {
        id: engineProfiles
        active: browser.backend === "webengine"
        source: active ? "EngineProfiles.qml" : ""
        function profile(record) {
            return item ? item.profile(record) : null;
        }
    }
}
