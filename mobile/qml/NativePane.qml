import QtQuick
import Arbiter

Item {
    id: root
    required property var record
    required property bool presented
    property real coverProgress: 0
    property rect overlayRect: Qt.rect(0, 0, 0, 0)
    NativePage {
        id: page
        coverProgress: root.coverProgress
        tabId: root.record.id
        profile: root.record.profile || "shared"
        privateMode: root.record.private || false
        url: root.record.url
        overlayRect: root.overlayRect
        onBrowserEvent: (tabId, type, data) => browser.updatePage(tabId, type, data)
        Component.onCompleted: start()
    }
    WindowContainer {
        anchors.fill: parent
        window: page.nativeWindow
        visible: root.presented
    }
    Connections {
        target: browser
        function onPageAction(tabId, action, argument) {
            if (tabId !== root.record.id)
                return;
            if (action === "capture")
                page.capture();
            else if (action === "find") {
                const options = JSON.parse(argument);
                page.find(options.query, options.backwards);
            } else if (action !== "navigate")
                page.action(action);
        }
    }
}
