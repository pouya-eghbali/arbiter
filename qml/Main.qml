pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic as Basic
import QtWebEngine

Basic.ApplicationWindow {
    id: window
    required property string browserUserAgent
    required property bool browserDiagnostics
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
    title: settingsOpen ? qsTr("Settings — Arbiter") : webView.title ? webView.title + " — Arbiter" : "Arbiter"
    color: palette.window

    readonly property bool emptyPage: webView.url.toString() === "about:blank"

    function focusAddress() {
        if (emptyPage && !settingsOpen) {
            homeAddress.forceActiveFocus()
            homeAddress.selectAll()
        } else {
            if (!addressPopup.visible) {
                popupAddress.text = settingsOpen ? "arbiter://settings" : webView.url.toString()
                addressPopup.open()
            } else {
                popupAddress.forceActiveFocus()
                popupAddress.selectAll()
            }
        }
    }

    function navigate(input: string) {
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
    Shortcut { sequence: window.bindings.reload; enabled: window.recordingIndex < 0 && !window.settingsOpen; onActivated: webView.reload() }
    Shortcut { sequence: window.bindings.back; enabled: window.recordingIndex < 0 && !window.settingsOpen; onActivated: webView.goBack() }
    Shortcut { sequence: window.bindings.forward; enabled: window.recordingIndex < 0 && !window.settingsOpen; onActivated: webView.goForward() }
    Shortcut {
        sequence: window.bindings.stop
        enabled: !addressPopup.visible && window.recordingIndex < 0 && !window.settingsOpen
        onActivated: { webView.stop(); webView.forceActiveFocus() }
    }
    Shortcut { sequence: window.bindings.close; enabled: window.recordingIndex < 0; onActivated: window.close() }
    Shortcut { sequence: window.bindings.fullscreen; enabled: window.recordingIndex < 0; onActivated: window.toggleFullScreen() }
    Shortcut { sequence: window.bindings.expand; enabled: window.recordingIndex < 0; onActivated: window.toggleExpanded() }

    function toggleSettings() {
        addressPopup.close()
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

    WebEngineView {
        id: webView
        anchors.fill: parent
        visible: !window.settingsOpen && !window.emptyPage
        url: "about:blank"
        backgroundColor: window.color
        profile: WebEngineProfile {
            storageName: "Default"
            offTheRecord: false
            httpUserAgent: window.browserUserAgent
        }
        onJavaScriptConsoleMessage: function(level, message, lineNumber, sourceID) {
            if (window.browserDiagnostics)
                console.log("Page console:", level, sourceID, lineNumber, message)
        }
        onUrlChanged: {
            if (window.emptyPage) {
                homeAddress.text = ""
                if (!window.settingsOpen)
                    Qt.callLater(function() { homeAddress.forceActiveFocus() })
            }
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
            if (request.status === WebEngineView.LoadFailedStatus) {
                // HTTP errors may carry a working sign-in or challenge page.
                // Let the site explain those; reserve the overlay for network errors.
                loadError.text = request.isErrorPage ? request.errorString : ""
            } else if (request.status === WebEngineView.LoadStartedStatus
                       || request.status === WebEngineView.LoadSucceededStatus) {
                loadError.text = ""
            }
        }
    }

    component AddressField: Basic.TextField {
        height: 48
        leftPadding: 18
        rightPadding: 18
        topPadding: 0
        bottomPadding: 0
        font.pixelSize: 16
        verticalAlignment: TextInput.AlignVCenter
        color: window.palette.text
        placeholderTextColor: window.palette.placeholderText
        selectionColor: window.palette.highlight
        selectedTextColor: window.palette.highlightedText
        placeholderText: qsTr("Search or enter address")
        Accessible.name: qsTr("Address and search")
        selectByMouse: true
        background: Rectangle {
            radius: 14
            color: window.palette.base
            border.color: parent.activeFocus ? window.palette.highlight : window.palette.mid
            border.width: 1
        }
        onAccepted: window.navigate(text)
    }

    Rectangle {
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
            spacing: 20
            Basic.Label {
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("Arbiter")
                font.pixelSize: 30
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
                color: window.palette.placeholderText
            }
            Basic.Button {
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("Settings")
                flat: true
                onClicked: window.toggleSettings()
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
            anchors.margins: 32
            contentWidth: availableWidth
            ColumnLayout {
                width: Math.min(720, settingsPage.width - 64)
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 16
                RowLayout {
                    Layout.fillWidth: true
                    Basic.Label { text: qsTr("Settings"); font.pixelSize: 28; Layout.fillWidth: true }
                    Basic.Button { text: qsTr("Done"); onClicked: window.toggleSettings() }
                }
                Basic.Label { text: qsTr("Keyboard bindings"); font.pixelSize: 18; font.weight: Font.Medium }
                Basic.Label {
                    text: qsTr("Click a shortcut, then press the new combination. Changes save automatically. Escape cancels recording.")
                    wrapMode: Text.Wrap
                    Layout.fillWidth: true
                    color: window.palette.placeholderText
                }
                Repeater {
                    model: window.keyBindings
                    delegate: RowLayout {
                        id: bindingRow
                        required property int index
                        required property string actionLabel
                        required property string keyLabel
                        Layout.fillWidth: true
                        Basic.Label { text: bindingRow.actionLabel; Layout.fillWidth: true }
                        Basic.Button {
                            id: recordButton
                            Layout.preferredWidth: 210
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
                Basic.Button {
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
                    color: window.palette.placeholderText
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
        id: addressPopup
        parent: Basic.Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(640, window.width - 48)
        padding: 12
        modal: true
        dim: true
        focus: true
        closePolicy: Basic.Popup.CloseOnEscape | Basic.Popup.CloseOnPressOutside
        background: Rectangle {
            radius: 20
            color: window.palette.window
            border.color: window.palette.mid
        }
        contentItem: AddressField {
            id: popupAddress
            implicitHeight: 48
            Accessible.name: qsTr("Edit address")
        }
        onOpened: {
            popupAddress.forceActiveFocus()
            popupAddress.selectAll()
        }
        onClosed: {
            if (window.settingsOpen)
                settingsPage.forceActiveFocus()
            else if (window.emptyPage)
                homeAddress.forceActiveFocus()
            else
                webView.forceActiveFocus()
        }
    }

    Basic.Label {
        id: loadError
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
