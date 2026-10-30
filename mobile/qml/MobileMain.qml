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
    color: Theme.window
    property string panel: browser.onboardingDone ? "" : "onboarding"
    property string activeTab: browser.activeId
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
    function refresh() {
        current = browser.tabs.find(t => t.id === browser.activeId) || {};
        visibleTabs = browser.tabs.filter(t => t.workspace === browser.workspaceId && Boolean(t.private) === browser.privateMode);
    }
    function open(name) {
        panel = name;
        query = "";
    }
    function go(value) {
        browser.navigate(value);
        panel = "";
        Qt.inputMethod.hide();
    }
    Component.onCompleted: refresh()
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

    // Native WebViews occupy their own window. Keep the pill in a reserved safe
    // region and hide the native surface for QML sheets, avoiding unsupported
    // native-view/QML overlap on Android and iOS.
    Item {
        id: content
        anchors {
            left: parent.left
            right: parent.right
            top: parent.top
            bottom: pill.top
            bottomMargin: 12
        }
        Image {
            anchors.fill: parent
            source: (Theme.dark ? browser.settings.darkImage : browser.settings.lightImage) || ""
            fillMode: Image.PreserveAspectCrop
            opacity: .6
            visible: source.toString().length > 0
        }
        Repeater {
            model: browser.screens
            delegate: Loader {
                id: pageLoader
                required property var tab
                readonly property var record: tab.record
                anchors.fill: parent
                active: false
                visible: record.id === browser.activeId && window.panel === "" && !browser.privateLocked
                function configure() {
                    if (record.sleeping || !record.url.length) {
                        active = false;
                        return;
                    }
                    if (browser.backend === "webengine" && !engineProfiles.item) {
                        active = false;
                        return;
                    }
                    if (!source.toString().length) {
                        const properties = {
                            record: Qt.binding(() => pageLoader.record),
                            presented: Qt.binding(() => pageLoader.visible)
                        };
                        if (browser.backend === "webengine")
                            properties.pageProfile = engineProfiles.profile(record);
                        setSource(browser.backend === "webengine" ? "WebEnginePane.qml" : "NativePane.qml", properties);
                    }
                    active = true;
                }
                onRecordChanged: configure()
                Component.onCompleted: configure()
                Connections {
                    target: engineProfiles
                    function onLoaded() {
                        pageLoader.configure();
                    }
                }
            }
        }
        ColumnLayout {
            anchors.centerIn: parent
            width: Math.min(parent.width - 48, 480)
            spacing: 26
            visible: !window.current.url && window.panel === "" && !browser.privateLocked
            RowLayout {
                Layout.alignment: Qt.AlignHCenter
                spacing: 14
                Image {
                    source: "qrc:/brand/valkyrie-source.png"
                    sourceSize.width: 60
                    Layout.preferredWidth: 60
                    Layout.preferredHeight: 60
                    fillMode: Image.PreserveAspectFit
                    layer.enabled: !Theme.dark
                    layer.effect: MultiEffect {
                        colorization: 1
                        colorizationColor: Theme.text
                    }
                }
                Text {
                    textFormat: Text.PlainText
                    text: "Arbiter"
                    color: Theme.text
                    font.pixelSize: 36
                    font.weight: Font.DemiBold
                }
            }
            UiButton {
                Layout.fillWidth: true
                text: qsTr("Search or enter address")
                iconName: "search"
                implicitHeight: 58
                onClicked: window.open("address")
            }
            Text {
                textFormat: Text.PlainText
                Layout.fillWidth: true
                text: browser.privateMode ? qsTr("Private browsing") : qsTr("Your space on the web")
                color: Theme.muted
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
            }
        }
        ColumnLayout {
            anchors.centerIn: parent
            width: Math.min(parent.width - 48, 420)
            spacing: 20
            visible: window.current.sleeping && window.current.error && window.panel === "" && !browser.privateLocked
            Text {
                textFormat: Text.PlainText
                text: window.current.error || ""
                color: Theme.text
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
            UiButton {
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
                Layout.preferredWidth: 44
                Layout.preferredHeight: 44
                Layout.alignment: Qt.AlignHCenter
                color: Theme.accent
            }
            Text {
                textFormat: Text.PlainText
                text: qsTr("Private browsing is locked")
                font.pixelSize: 25
                color: Theme.text
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
            }
            UiButton {
                Layout.fillWidth: true
                text: qsTr("Unlock")
                primary: true
                onClicked: browser.unlockPrivate()
            }
            UiButton {
                Layout.fillWidth: true
                text: qsTr("Return to regular browsing")
                onClicked: browser.togglePrivate()
            }
        }
    }
    Rectangle {
        id: pill
        anchors {
            left: parent.left
            right: parent.right
            bottom: parent.bottom
            leftMargin: 12
            rightMargin: 12
            bottomMargin: 10
        }
        height: browser.fieldFocused ? 102 : 58
        radius: 29
        color: Theme.glassDense
        visible: window.panel === ""
        opacity: visible ? 1 : 0
        Behavior on opacity {
            NumberAnimation {
                duration: Theme.enterDuration
            }
        }
        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 6
            spacing: 2
            RowLayout {
                visible: browser.fieldFocused
                Layout.fillWidth: true
                implicitHeight: 38
                UiButton {
                    Layout.fillWidth: true
                    flat: true
                    text: qsTr("Passwords")
                    iconName: "key-round"
                    onClicked: browser.passwordAccounts()
                }
                UiButton {
                    Layout.fillWidth: true
                    flat: true
                    text: qsTr("Suggest password")
                    iconName: "sparkles"
                    onClicked: browser.suggestPassword()
                }
                UiIconButton {
                    iconName: "x"
                    label: qsTr("Dismiss")
                    onClicked: browser.dismissSuggestions()
                }
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 2
                UiIconButton {
                    iconName: "arrow-left"
                    label: qsTr("Back")
                    enabled: window.current.back || false
                    onClicked: browser.pageAction(browser.activeId, "back", "")
                }
                UiIconButton {
                    iconName: "arrow-right"
                    label: qsTr("Forward")
                    enabled: window.current.forward || false
                    onClicked: browser.pageAction(browser.activeId, "forward", "")
                }
                UiButton {
                    Layout.fillWidth: true
                    flat: true
                    text: window.current.url ? window.current.url.replace(/^https?:\/\//, "").split("/")[0] : qsTr("Search")
                    onClicked: window.open("address")
                    onPressAndHold: window.open("tabs")
                }
                UiIconButton {
                    iconName: "panels-top-left"
                    label: qsTr("Tabs")
                    onClicked: window.open("tabs")
                }
                UiIconButton {
                    iconName: "ellipsis"
                    label: qsTr("More")
                    onClicked: window.open("more")
                }
            }
        }
    }
    // Sheets replace the native surface; all controls remain QML, including on phones.
    Rectangle {
        anchors.fill: parent
        visible: window.panel !== ""
        color: Theme.window
        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 20
            spacing: 14
            RowLayout {
                Layout.fillWidth: true
                Text {
                    textFormat: Text.PlainText
                    Layout.fillWidth: true
                    text: window.panel === "onboarding" ? "Arbiter" : window.panel === "address" ? qsTr("Go somewhere") : window.panel === "tabs" ? qsTr("Tabs") : window.panel === "more" ? qsTr("Arbiter") : window.panel === "settings" ? qsTr("Settings") : window.panel === "history" ? qsTr("History") : window.panel === "downloads" ? qsTr("Downloads") : window.panel === "workspaces" ? qsTr("Workspaces") : window.panel === "vault" ? qsTr("Passwords & passkeys") : window.panel === "save" ? qsTr("Save password?") : window.panel === "accounts" ? qsTr("Passwords") : qsTr("Pair with your desktop")
                    font.pixelSize: 27
                    font.weight: Font.DemiBold
                    color: Theme.text
                }
                UiIconButton {
                    iconName: "x"
                    label: qsTr("Close")
                    visible: window.panel !== "onboarding"
                    onClicked: {
                        if (window.panel === "save")
                            browser.dismissLogin();
                        window.panel = "";
                    }
                }
            }
            UiTextField {
                id: filter
                visible: ["address", "tabs", "history", "vault"].includes(window.panel)
                Layout.fillWidth: true
                placeholderText: window.panel === "address" ? qsTr("Search or enter address") : qsTr("Search")
                text: window.query
                onTextEdited: window.query = text
                onAccepted: if (window.panel === "address")
                    window.go(text)
                Component.onCompleted: if (visible)
                    forceActiveFocus()
            }
            Controls.ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                contentWidth: availableWidth
                ColumnLayout {
                    width: parent.width
                    spacing: 14
                    Repeater {
                        model: window.panel === "address" ? browser.suggestions(window.query) : []
                        delegate: UiButton {
                            required property var modelData
                            Layout.fillWidth: true
                            text: modelData.title || modelData.url
                            textAlignment: Text.AlignLeft
                            iconName: modelData.tab ? "panels-top-left" : "globe"
                            onClicked: {
                                if (modelData.tab) {
                                    browser.selectTab(modelData.tab);
                                    window.panel = "";
                                } else
                                    window.go(modelData.url);
                            }
                        }
                    }
                    UiButton {
                        visible: window.panel === "address" && window.query.length > 0
                        Layout.fillWidth: true
                        primary: true
                        text: qsTr("Go")
                        iconName: "arrow-right"
                        onClicked: window.go(window.query)
                    }
                    RowLayout {
                        visible: window.panel === "tabs"
                        Layout.fillWidth: true
                        UiButton {
                            text: qsTr("Workspaces")
                            iconName: "layers"
                            onClicked: window.open("workspaces")
                        }
                        Item {
                            Layout.fillWidth: true
                        }
                        UiButton {
                            text: qsTr("New tab")
                            iconName: "plus"
                            primary: true
                            onClicked: {
                                browser.addTab();
                                window.panel = "";
                            }
                        }
                    }
                    Repeater {
                        model: window.panel === "tabs" ? window.visibleTabs.filter(t => !window.query || t.title.toLowerCase().includes(window.query.toLowerCase()) || t.url.includes(window.query)) : []
                        delegate: Rectangle {
                            required property var modelData
                            Layout.fillWidth: true
                            implicitHeight: 82
                            radius: 20
                            color: modelData.id === browser.activeId ? Theme.selection : Theme.field
                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: 12
                                UiIcon {
                                    name: modelData.private ? "lock" : modelData.sleeping ? "moon" : "globe"
                                    color: Theme.accent
                                    Layout.preferredWidth: 24
                                    Layout.preferredHeight: 24
                                }
                                UiButton {
                                    Layout.fillWidth: true
                                    flat: true
                                    text: modelData.title || qsTr("New tab")
                                    textAlignment: Text.AlignLeft
                                    onClicked: {
                                        browser.selectTab(modelData.id);
                                        window.panel = "";
                                    }
                                }
                                UiIconButton {
                                    iconName: "x"
                                    label: qsTr("Close tab")
                                    onClicked: browser.closeTab(modelData.id)
                                }
                            }
                        }
                    }
                    Repeater {
                        model: window.panel === "more" ? [["layers", qsTr("Workspaces"), "workspaces"], ["shield-check", browser.blockingFor(window.current.url || "") ? qsTr("Pause ad blocking for this site") : qsTr("Enable ad blocking for this site"), "block"], ["share-2", qsTr("Share"), "share"], ["key-round", qsTr("Passwords & passkeys"), "vault"], ["fingerprint", qsTr("Fill password"), "fill"], ["sparkles", qsTr("Suggest password"), "generate"], ["history", qsTr("History"), "history"], ["download", qsTr("Downloads"), "downloads"], ["lock", browser.privateMode ? qsTr("Return to regular browsing") : qsTr("Private browsing"), "private"], ["rotate-cw", qsTr("Reload"), "reload"], ["settings", qsTr("Settings"), "settings"]] : []
                        delegate: UiButton {
                            required property var modelData
                            Layout.fillWidth: true
                            flat: true
                            iconName: modelData[0]
                            text: modelData[1]
                            textAlignment: Text.AlignLeft
                            implicitHeight: 48
                            onClicked: {
                                const action = modelData[2];
                                if (action === "block") {
                                    browser.toggleSiteBlocking();
                                    window.panel = "";
                                } else if (action === "share") {
                                    window.panel = "";
                                    browser.share();
                                } else if (action === "private") {
                                    browser.togglePrivate();
                                    window.panel = "";
                                } else if (action === "reload") {
                                    browser.pageAction(browser.activeId, "reload", "");
                                    window.panel = "";
                                } else if (action === "fill") {
                                    browser.passwordAccounts();
                                } else if (action === "generate") {
                                    browser.suggestPassword();
                                    window.panel = "";
                                } else
                                    window.open(action);
                            }
                        }
                    }
                    Repeater {
                        model: window.panel === "workspaces" ? browser.workspaces : []
                        delegate: Rectangle {
                            required property var modelData
                            Layout.fillWidth: true
                            implicitHeight: 90
                            radius: 20
                            color: modelData.id === browser.workspaceId ? Theme.selection : Theme.field
                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: 12
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    UiButton {
                                        Layout.fillWidth: true
                                        flat: true
                                        text: modelData.name
                                        textAlignment: Text.AlignLeft
                                        onClicked: {
                                            browser.selectWorkspace(modelData.id);
                                            window.panel = "";
                                        }
                                    }
                                    Text {
                                        textFormat: Text.PlainText
                                        color: Theme.muted
                                        text: modelData.isolated ? qsTr("Separate session") : qsTr("Shared session")
                                    }
                                }
                                UiIconButton {
                                    iconName: "trash-2"
                                    label: qsTr("Delete workspace")
                                    visible: browser.workspaces.length > 1
                                    onClicked: browser.removeWorkspace(modelData.id)
                                }
                            }
                        }
                    }
                    RowLayout {
                        visible: window.panel === "workspaces"
                        Layout.fillWidth: true
                        UiTextField {
                            id: workspaceName
                            Layout.fillWidth: true
                            placeholderText: qsTr("Workspace name")
                        }
                        UiIconButton {
                            iconName: "plus"
                            label: qsTr("Add workspace")
                            onClicked: {
                                browser.addWorkspace(workspaceName.text, isolate.checked);
                                workspaceName.clear();
                            }
                        }
                    }
                    UiCheckBox {
                        id: isolate
                        visible: window.panel === "workspaces"
                        text: qsTr("Use a separate session")
                    }
                    Repeater {
                        model: window.panel === "history" ? browser.history.filter(t => !window.query || t.url.toLowerCase().includes(window.query.toLowerCase()) || t.title.toLowerCase().includes(window.query.toLowerCase())) : []
                        delegate: ColumnLayout {
                            required property var modelData
                            Layout.fillWidth: true
                            Text {
                                textFormat: Text.PlainText
                                text: new Date(modelData.time).toLocaleDateString(Qt.locale(), Locale.LongFormat)
                                color: Theme.muted
                                font.pixelSize: 12
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                UiButton {
                                    Layout.fillWidth: true
                                    flat: true
                                    text: modelData.title || modelData.url
                                    textAlignment: Text.AlignLeft
                                    onClicked: window.go(modelData.url)
                                }
                                UiIconButton {
                                    iconName: "trash-2"
                                    label: qsTr("Delete")
                                    onClicked: browser.removeHistory(modelData.url)
                                }
                            }
                        }
                    }
                    UiButton {
                        visible: window.panel === "history"
                        text: window.query ? qsTr("Delete matching history") : qsTr("Clear history")
                        onClicked: historyConfirmation.open()
                    }
                    Repeater {
                        model: window.panel === "downloads" ? browser.downloads : []
                        delegate: ColumnLayout {
                            required property var modelData
                            Layout.fillWidth: true
                            Text {
                                textFormat: Text.PlainText
                                text: modelData.name || modelData.url
                                color: Theme.text
                                elide: Text.ElideMiddle
                                Layout.fillWidth: true
                            }
                            Text {
                                textFormat: Text.PlainText
                                text: modelData.status || qsTr("Downloading…")
                                color: Theme.muted
                                Layout.fillWidth: true
                                wrapMode: Text.WordWrap
                            }
                            Controls.ProgressBar {
                                Layout.fillWidth: true
                                from: 0
                                to: Math.max(1, modelData.total || 1)
                                value: modelData.received || 0
                            }
                            UiButton {
                                text: qsTr("Open")
                                enabled: modelData.complete || false
                                onClicked: browser.openDownload(modelData.id)
                            }
                        }
                    }
                    Text {
                        textFormat: Text.PlainText
                        visible: window.panel === "downloads" && browser.downloads.length === 0
                        text: qsTr("No downloads yet")
                        color: Theme.muted
                    }
                    Text {
                        textFormat: Text.PlainText
                        visible: window.panel === "vault"
                        text: qsTr("Saved passwords and passkeys stay encrypted on this device.")
                        wrapMode: Text.WordWrap
                        color: Theme.muted
                        Layout.fillWidth: true
                    }
                    UiButton {
                        visible: window.panel === "vault" && !browser.vault.unlocked
                        Layout.fillWidth: true
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
                                    color: Theme.text
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }
                                Text {
                                    textFormat: Text.PlainText
                                    text: modelData.username || ""
                                    color: Theme.muted
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }
                            }
                            UiIconButton {
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
                        color: Theme.muted
                        Layout.fillWidth: true
                    }
                    Repeater {
                        model: window.panel === "vault" ? browser.vault.conflictChoices : []
                        delegate: UiButton {
                            required property var modelData
                            Layout.fillWidth: true
                            text: modelData.deleted ? qsTr("Keep deletion") : (modelData.origin || modelData.rpId) + " · " + (modelData.username || "") + " · " + modelData.device
                            textAlignment: Text.AlignLeft
                            onClicked: browser.vault.resolveConflict(modelData.id, modelData.version)
                        }
                    }
                    Text {
                        textFormat: Text.PlainText
                        visible: window.panel === "vault" && browser.vault.error.length > 0
                        text: browser.vault.error
                        wrapMode: Text.WordWrap
                        color: Theme.text
                        Layout.fillWidth: true
                    }
                    UiButton {
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
                    UiButton {
                        visible: window.panel === "vault"
                        text: qsTr("Enable credential provider")
                        onClicked: browser.enableCredentialProvider()
                    }
                    UiButton {
                        visible: window.panel === "vault" && browser.vault.unlocked
                        text: qsTr("Lock")
                        onClicked: browser.vault.lock()
                    }
                    Text {
                        textFormat: Text.PlainText
                        visible: window.panel === "pair"
                        text: qsTr("In Arbiter on desktop, open Settings → Sync, unlock the vault and copy its pairing invitation. Verify the desktop name and certificate fingerprint before pairing.")
                        color: Theme.muted
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                    UiButton {
                        visible: window.panel === "pair"
                        text: qsTr("Scan QR code")
                        iconName: "qr-code"
                        onClicked: window.open("scan")
                    }
                    Loader {
                        visible: window.panel === "scan"
                        active: visible
                        Layout.fillWidth: true
                        source: "PairingScanner.qml"
                        onLoaded: item.invitation.connect(value => {
                            browser.vault.pair(value);
                        })
                    }
                    UiTextField {
                        id: invitation
                        visible: window.panel === "pair"
                        Layout.fillWidth: true
                        placeholderText: qsTr("Pairing invitation")
                        echoMode: TextInput.Password
                    }
                    UiButton {
                        visible: window.panel === "pair"
                        text: qsTr("Continue")
                        primary: true
                        onClicked: browser.vault.pair(invitation.text)
                    }
                    Text {
                        textFormat: Text.PlainText
                        visible: window.panel === "pairReview"
                        text: window.pairDesktop + "\n\n" + window.pairPin
                        color: Theme.text
                        wrapMode: Text.WrapAnywhere
                        Layout.fillWidth: true
                    }
                    UiButton {
                        visible: window.panel === "pairReview"
                        text: qsTr("Pair")
                        primary: true
                        onClicked: {
                            browser.vault.confirmPair();
                            window.open("vault");
                        }
                    }
                    Text {
                        textFormat: Text.PlainText
                        visible: window.panel === "save"
                        text: window.pendingOrigin
                        color: Theme.muted
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                    UiTextField {
                        id: saveUser
                        visible: window.panel === "save"
                        text: window.pendingUsername
                        placeholderText: qsTr("Username")
                        Layout.fillWidth: true
                    }
                    UiButton {
                        visible: window.panel === "save"
                        text: qsTr("Save")
                        primary: true
                        onClicked: {
                            browser.saveLogin(saveUser.text);
                            window.panel = "";
                        }
                    }
                    UiButton {
                        visible: window.panel === "save"
                        text: qsTr("Not now")
                        onClicked: {
                            browser.dismissLogin();
                            window.panel = "";
                        }
                    }
                    Repeater {
                        model: window.panel === "accounts" ? window.accounts : []
                        delegate: UiButton {
                            required property var modelData
                            Layout.fillWidth: true
                            text: modelData.username || qsTr("Saved account")
                            onClicked: {
                                browser.fillPassword(modelData.id);
                                window.panel = "";
                            }
                        }
                    }
                    Loader {
                        active: window.panel === "settings"
                        Layout.fillWidth: true
                        source: "MobileSettings.qml"
                    }
                    ColumnLayout {
                        visible: window.panel === "onboarding"
                        Layout.fillWidth: true
                        spacing: 24
                        Image {
                            source: "qrc:/brand/valkyrie-source.png"
                            Layout.preferredWidth: 96
                            Layout.preferredHeight: 96
                            Layout.alignment: Qt.AlignHCenter
                            fillMode: Image.PreserveAspectFit
                            layer.enabled: !Theme.dark
                            layer.effect: MultiEffect {
                                colorization: 1
                                colorizationColor: Theme.text
                            }
                            opacity: 1
                            Behavior on opacity {
                                NumberAnimation {
                                    duration: Theme.enterDuration
                                }
                            }
                        }
                        Text {
                            textFormat: Text.PlainText
                            Layout.fillWidth: true
                            text: window.onboardingStep === 0 ? qsTr("Welcome to Arbiter") : window.onboardingStep === 1 ? qsTr("Make yourself at home") : window.onboardingStep === 2 ? qsTr("Bring your vault along") : qsTr("You’re ready")
                            font.pixelSize: 30
                            font.weight: Font.DemiBold
                            color: Theme.text
                            wrapMode: Text.WordWrap
                        }
                        Text {
                            textFormat: Text.PlainText
                            Layout.fillWidth: true
                            color: Theme.muted
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
                                delegate: UiButton {
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
                        UiCheckBox {
                            visible: window.onboardingStep === 0
                            text: qsTr("Block ads and trackers")
                            checked: browser.settings.adblockEnabled
                            onToggled: browser.setSetting("adblockEnabled", checked)
                        }
                        UiButton {
                            visible: window.onboardingStep === 2
                            text: qsTr("Pair with your desktop")
                            onClicked: {
                                browser.setSetting("onboardingDone", true);
                                window.open("pair");
                            }
                        }
                        UiButton {
                            visible: window.onboardingStep === 2
                            text: qsTr("Set as default browser")
                            onClicked: browser.setDefaultBrowser()
                        }
                        UiButton {
                            Layout.fillWidth: true
                            primary: true
                            text: window.onboardingStep === 3 ? qsTr("Start browsing") : qsTr("Continue")
                            onClicked: {
                                if (window.onboardingStep === 3) {
                                    browser.setSetting("onboardingDone", true);
                                    window.panel = "";
                                } else
                                    window.onboardingStep++;
                            }
                        }
                        UiButton {
                            Layout.fillWidth: true
                            text: qsTr("Skip setup")
                            onClicked: {
                                browser.setSetting("onboardingDone", true);
                                window.panel = "";
                            }
                        }
                    }
                }
            }
        }
    }
    Controls.Dialog {
        id: historyConfirmation
        anchors.centerIn: parent
        width: Math.min(parent.width - 32, 400)
        modal: true
        title: qsTr("Delete history?")
        standardButtons: Controls.Dialog.Cancel | Controls.Dialog.Ok
        onAccepted: browser.clearHistory(window.query)
    }
    Controls.Popup {
        id: toast
        property string text: ""
        anchors.centerIn: parent
        width: Math.min(parent.width - 40, 420)
        padding: 20
        contentItem: Text {
            textFormat: Text.PlainText
            text: toast.text
            color: Theme.text
            wrapMode: Text.WordWrap
        }
        background: Rectangle {
            color: Theme.popupSolid
            radius: 18
        }
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
