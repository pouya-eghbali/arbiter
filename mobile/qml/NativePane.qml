import QtQuick
import Arbiter

Item {
    id: root
    required property var record
    required property bool presented
    NativePage {
        id: page
        tabId: root.record.id
        profile: root.record.profile || "shared"
        privateMode: root.record.private || false
        url: root.record.url
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
            if (action !== "navigate")
                page.action(action);
        }
    }
}
