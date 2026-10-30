import QtQuick
import QtWebEngine
import QtWebChannel
import Arbiter

Item {
    id: root
    required property var record
    required property bool presented
    required property var pageProfile
    WebEngineView {
        id: web
        anchors.fill: parent
        visible: root.presented
        profile: root.pageProfile
        url: root.record.url
        backgroundColor: "white"
        settings.pluginsEnabled: true
        settings.pdfViewerEnabled: true
        settings.javascriptCanOpenWindows: false
        webChannel: channel
        webChannelWorld: WebEngineScript.ApplicationWorld
        userScripts: [
            WebEngineScript {
                injectionPoint: WebEngineScript.DocumentReady
                worldId: WebEngineScript.ApplicationWorld
                runsOnSubFrames: false
                sourceCode: captureSource
            }
        ]
        property string captureSource: browser.formScript(root.record.private) // native isolated channel is prepended below
        onUrlChanged: browser.updatePage(root.record.id, "location", {
            url: url.toString(),
            back: canGoBack,
            forward: canGoForward
        })
        onTitleChanged: browser.updatePage(root.record.id, "title", {
            title: title
        })
        onLoadProgressChanged: browser.updatePage(root.record.id, "progress", {
            progress: loadProgress
        })
        onRecentlyAudibleChanged: browser.updatePage(root.record.id, "activityState", {
            audio: recentlyAudible
        })
        onLoadingChanged: request => browser.updatePage(root.record.id, request.status === WebEngineView.LoadStartedStatus ? "start" : "finish", {
                success: request.status !== WebEngineView.LoadFailedStatus
            })
        onNewWindowRequested: request => {
            if (request.userInitiated)
                browser.addTab(request.requestedUrl.toString());
        }
        onPermissionRequested: permission => permission.deny()
        onCertificateError: error => error.rejectCertificate()
    }
    WebChannel {
        id: channel
        registeredObjects: [capture]
    }
    QtObject {
        id: capture
        WebChannel.id: "arbiterForms"
        function submit(json) {
            try {
                if (["credentialFocus", "loginSubmitted", "loginSucceeded", "activityState"].includes(json.type))
                    browser.updatePage(root.record.id, json.type, json);
            } catch (e) {}
        }
    }
    Connections {
        target: browser
        function onPageAction(tabId, action, argument) {
            if (tabId !== root.record.id)
                return;
            if (action === "back")
                web.goBack();
            else if (action === "forward")
                web.goForward();
            else if (action === "reload")
                web.reload();
            else if (action === "script")
                web.runJavaScript(argument, WebEngineScript.ApplicationWorld);
            else if (action === "inspectLogin")
                web.runJavaScript("globalThis.__arbiterHasPassword?.()??true", WebEngineScript.ApplicationWorld, result => browser.updatePage(root.record.id, "loginInspection", {
                        hasPassword: result
                    }));
        }
    }
}
