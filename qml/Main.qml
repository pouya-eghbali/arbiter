import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtWebEngine

ApplicationWindow {
    id: window
    width: 1280
    height: 820
    minimumWidth: 640
    minimumHeight: 480
    visible: true
    title: webView.title ? webView.title + " — Arbiter" : "Arbiter"
    color: "#17191c"
    palette.window: "#17191c"
    palette.windowText: "#e8e8ea"
    palette.base: "#25282d"
    palette.text: "#e8e8ea"
    palette.button: "#25282d"
    palette.buttonText: "#e8e8ea"
    palette.highlight: "#7e9cd8"

    function focusAddress() {
        address.forceActiveFocus()
        address.selectAll()
    }

    function navigate(input: string) {
        const text = input.trim()
        if (!text)
            return
        if (/^(https?:\/\/|file:\/\/|about:)/i.test(text))
            webView.url = text
        else if (!/\s/.test(text) && (text.includes(".") || /^localhost(:\d+)?(\/|$)/i.test(text)))
            webView.url = (/^localhost(:|\/|$)/i.test(text) ? "http://" : "https://") + text
        else
            webView.url = "https://duckduckgo.com/?q=" + encodeURIComponent(text)
        webView.forceActiveFocus()
    }

    Shortcut { sequence: "Ctrl+L"; onActivated: window.focusAddress() }
    Shortcut { sequence: "Meta+L"; onActivated: window.focusAddress() }
    Shortcut { sequence: StandardKey.Refresh; onActivated: webView.reload() }
    Shortcut { sequence: StandardKey.Back; onActivated: webView.goBack() }
    Shortcut { sequence: StandardKey.Forward; onActivated: webView.goForward() }
    Shortcut { sequence: "Escape"; onActivated: webView.stop() }

    header: ColumnLayout {
        spacing: 0
        RowLayout {
            Layout.fillWidth: true
            Layout.margins: 10
            spacing: 8
            ToolButton {
                text: "←"
                enabled: webView.canGoBack
                Accessible.name: qsTr("Back")
                onClicked: webView.goBack()
            }
            ToolButton {
                text: "→"
                enabled: webView.canGoForward
                Accessible.name: qsTr("Forward")
                onClicked: webView.goForward()
            }
            ToolButton {
                text: webView.loading ? "×" : "↻"
                Accessible.name: webView.loading ? qsTr("Stop") : qsTr("Reload")
                onClicked: webView.loading ? webView.stop() : webView.reload()
            }
            TextField {
                id: address
                Layout.fillWidth: true
                placeholderText: qsTr("Search or enter address")
                Accessible.name: qsTr("Address and search")
                selectByMouse: true
                onAccepted: window.navigate(text)
                onActiveFocusChanged: {
                    if (!activeFocus)
                        text = webView.url.toString() === "about:blank" ? "" : webView.url.toString()
                }
                Component.onCompleted: window.focusAddress()
            }
        }
        ProgressBar {
            Layout.fillWidth: true
            Layout.preferredHeight: 2
            from: 0
            to: 100
            value: webView.loadProgress
            visible: webView.loading
        }
    }

    WebEngineView {
        id: webView
        anchors.fill: parent
        url: "about:blank"
        backgroundColor: window.color
        profile: WebEngineProfile {
            storageName: "Default"
            offTheRecord: false
        }
        onUrlChanged: {
            if (!address.activeFocus)
                address.text = url.toString() === "about:blank" ? "" : url.toString()
        }
        onLoadingChanged: function(request) {
            if (request.status === WebEngineView.LoadFailedStatus)
                loadError.text = request.errorString
            else if (request.status === WebEngineView.LoadStartedStatus)
                loadError.text = ""
        }
    }

    Label {
        id: loadError
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        padding: 12
        visible: text.length > 0
        wrapMode: Text.Wrap
        color: "#ffb4ab"
        background: Rectangle { color: "#25282d" }
    }
}
