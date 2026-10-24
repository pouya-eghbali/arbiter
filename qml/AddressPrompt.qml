pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic as Basic

FocusScope {
    id: prompt
    property var browserPreferences: null
    property bool privateMode: false
    property var history: null
    property var tabs: null
    property int currentTabId: 0
    property real maximumHeight: 580
    property bool dismissed: false
    property var entries: []
    property string builtQuery: ""
    readonly property alias addressField: field
    property alias text: field.text
    readonly property bool expanded: activeFocus && !dismissed && maximumHeight >= 180
    readonly property real listHeight: expanded ? Math.min(entries.length, 8, Math.floor(Math.max(0, maximumHeight - 124) / 52)) * 52 : 0
    implicitHeight: 64 + (expanded ? listHeight + 60 : 0)
    signal navigateRequested(string input)
    signal tabRequested(int tabId)
    signal cancelRequested()
    readonly property var popular: [
        {title: "Wikipedia", url: "https://www.wikipedia.org/"},
        {title: "YouTube", url: "https://www.youtube.com/"},
        {title: "GitHub", url: "https://github.com/"},
        {title: "Reddit", url: "https://www.reddit.com/"},
        {title: "Hacker News", url: "https://news.ycombinator.com/"},
        {title: "MDN Web Docs", url: "https://developer.mozilla.org/"},
        {title: "DuckDuckGo", url: "https://duckduckgo.com/"},
        {title: "Stack Overflow", url: "https://stackoverflow.com/"}
    ]
    function focusAddress(select: bool) {
        dismissed = false
        field.forceActiveFocus()
        if (select) field.selectAll()
        rebuild()
    }
    function rebuild() {
        if (!visible) return
        const query = field.text.trim().toLowerCase()
        const previous = suggestions.currentIndex >= 0 ? entries[suggestions.currentIndex] : null
        const words = query.split(/\s+/).filter(word => word.length > 0)
        const result = [], seen = ({})
        function add(title, url, kind, tabId) {
            if (!/^https?:\/\//i.test(url) || result.length >= 20) return
            const withoutFragment = url.replace(/#.*$/, "")
            const canonical = withoutFragment.includes("?") ? withoutFragment : withoutFragment.replace(/\/$/, "")
            if (seen[canonical]) return
            if (!words.every(word => (title + " " + url).toLowerCase().includes(word))) return
            seen[canonical] = true
            result.push({title: title || url, url: url, kind: kind, tabId: tabId || 0})
        }
        if (tabs) for (const tab of tabs.choices()) {
            if (tab.id !== currentTabId) add(tab.title, tab.url, "tab", tab.id)
            if (result.length >= (query.length > 0 ? 8 : 3)) break
        }
        if (history) for (const visit of history.suggestions(field.text, query.length > 0 ? 8 : 4)) add(visit.title, visit.url, "history", 0)
        for (const site of popular) add(site.title, site.url, "popular", 0)
        entries = result
        suggestions.currentIndex = previous && builtQuery === query
            ? result.findIndex(entry => entry.url === previous.url && entry.tabId === previous.tabId) : -1
        builtQuery = query
    }
    function activate(index: int) {
        if (index < 0 || index >= entries.length) return
        const entry = entries[index]
        if (entry.kind === "tab") tabRequested(entry.tabId)
        else navigateRequested(entry.url)
    }
    function submit() {
        if (suggestions.currentIndex >= 0) activate(suggestions.currentIndex)
        else if (field.text.trim().length > 0) navigateRequested(field.text)
    }
    function moveSelection(direction: int) {
        if (!expanded || entries.length === 0) return
        suggestions.currentIndex = suggestions.currentIndex < 0
            ? (direction > 0 ? 0 : entries.length - 1)
            : Math.max(-1, Math.min(entries.length - 1, suggestions.currentIndex + direction))
        if (suggestions.currentIndex >= 0) suggestions.positionViewAtIndex(suggestions.currentIndex, ListView.Contain)
    }
    onVisibleChanged: if (visible) refresh.restart()
    onCurrentTabIdChanged: refresh.restart()
    Component.onCompleted: rebuild()
    Timer { id: refresh; interval: 60; onTriggered: prompt.rebuild() }
    Connections {
        target: prompt.history
        function onCountChanged() { refresh.restart() }
    }
    Connections {
        target: prompt.tabs
        function onDataChanged() { refresh.restart() }
        function onCountChanged() { refresh.restart() }
    }
    Rectangle { anchors.fill: parent; radius: prompt.expanded ? 24 : 18; color: Theme.popupSolid; border.color: Theme.border }
    RowLayout {
        id: searchRow
        anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top
        anchors.leftMargin: 18; anchors.rightMargin: 18
        height: 64
        spacing: 12
        UiIcon { name: "search"; color: Theme.text; Layout.preferredWidth: 20; Layout.preferredHeight: 20 }
        UiTextField {
            id: field
            objectName: "addressPromptField"
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.alignment: Qt.AlignVCenter
            verticalAlignment: TextInput.AlignVCenter
            implicitHeight: 36
            padding: 0
            font.pixelSize: prompt.width < 400 ? 15 : 18
            placeholderText: qsTr("Search or enter address")
            Accessible.name: placeholderText
            background: Item { }
            selectByMouse: true
            maximumLength: 8192
            onTextChanged: { suggestions.currentIndex = -1; refresh.restart() }
            onTextEdited: prompt.dismissed = false
            onActiveFocusChanged: if (activeFocus) { prompt.dismissed = false; prompt.rebuild() }
            onAccepted: prompt.submit()
            Keys.onDownPressed: { if (refresh.running) { refresh.stop(); prompt.rebuild() }; prompt.moveSelection(1) }
            Keys.onUpPressed: { if (refresh.running) { refresh.stop(); prompt.rebuild() }; prompt.moveSelection(-1) }
            Keys.onEscapePressed: { prompt.dismissed = true; suggestions.currentIndex = -1; prompt.cancelRequested() }
            TapHandler { onTapped: { prompt.dismissed = false; prompt.rebuild() } }
        }
    }
    ListView {
        id: suggestions
        objectName: "addressSuggestions"
        anchors.left: parent.left; anchors.right: parent.right
        anchors.top: parent.top; anchors.topMargin: 64
        anchors.leftMargin: 10; anchors.rightMargin: 10
        height: prompt.listHeight
        visible: prompt.expanded
        model: prompt.entries
        clip: true
        currentIndex: -1
        boundsBehavior: Flickable.StopAtBounds
        Basic.ScrollBar.vertical: Basic.ScrollBar { policy: suggestions.contentHeight > suggestions.height ? Basic.ScrollBar.AsNeeded : Basic.ScrollBar.AlwaysOff }
        delegate: UiButton {
            id: suggestion
            required property var modelData
            required property int index
            width: suggestions.width
            height: 52
            padding: 10
            flat: true
            highlighted: suggestions.currentIndex === index
            Accessible.name: modelData.title + ", " + (modelData.kind === "tab" ? qsTr("Switch to tab") : modelData.kind === "history" ? qsTr("History") : qsTr("Popular site"))
            contentItem: RowLayout {
                spacing: 12
                Rectangle {
                    implicitWidth: 28; implicitHeight: 28; radius: 7; color: Theme.field
                    id: iconTile
                    property url iconSource: ""
                    function refreshIcon() {
                        iconSource = prompt.browserPreferences && !prompt.privateMode
                            ? prompt.browserPreferences.favicon(suggestion.modelData.url, true) : ""
                    }
                    Component.onCompleted: refreshIcon()
                    Connections {
                        target: prompt.browserPreferences
                        function onFaviconReady(origin: string) {
                            if (origin === prompt.browserPreferences.siteOrigin(suggestion.modelData.url)) iconTile.refreshIcon()
                        }
                    }
                    Connections { target: suggestion; function onModelDataChanged() { iconTile.refreshIcon() } }
                    Image {
                        id: siteIcon
                        anchors.centerIn: parent
                        width: 20; height: 20
                        sourceSize: Qt.size(32, 32)
                        source: iconTile.iconSource
                        asynchronous: true
                        cache: true
                        fillMode: Image.PreserveAspectFit
                    }
                    Basic.Label { anchors.centerIn: parent; visible: siteIcon.status !== Image.Ready; text: suggestion.modelData.title.substring(0,1).toUpperCase(); color: Theme.text; font.pixelSize: 14; font.weight: Font.DemiBold }
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2
                    Basic.Label { Layout.fillWidth: true; text: suggestion.modelData.title; textFormat: Text.PlainText; elide: Text.ElideRight; color: Theme.text; font.pixelSize: 14 }
                    Basic.Label { Layout.fillWidth: true; text: suggestion.modelData.url.replace(/^https?:\/\//, ""); textFormat: Text.PlainText; elide: Text.ElideMiddle; color: Theme.muted; font.pixelSize: 11 }
                }
                Basic.Label {
                    visible: prompt.width >= 420
                    text: suggestion.modelData.kind === "tab" ? qsTr("Switch to tab") : suggestion.modelData.kind === "history" ? qsTr("History") : qsTr("Popular")
                    color: Theme.muted; font.pixelSize: 12
                }
            }
            onClicked: prompt.activate(index)
        }
    }
    RowLayout {
        visible: prompt.expanded
        anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
        anchors.margins: 14
        height: 36
        Basic.Label { Layout.fillWidth: true; text: prompt.entries.length === 0 ? qsTr("Search the web or enter a URL") : prompt.width < 400 ? qsTr("↑ ↓  Select") : qsTr("↑ ↓  Select     esc  Dismiss"); color: Theme.muted; font.pixelSize: 11 }
        UiButton { text: qsTr("Go  ↵"); primary: true; implicitHeight: 36; enabled: field.text.trim().length > 0 || suggestions.currentIndex >= 0; onClicked: prompt.submit() }
    }
}
