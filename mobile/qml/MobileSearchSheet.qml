pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic as Controls
import QtQuick.Layouts
import Arbiter

Item {
    id: root
    property real keyboardInset: 0
    property string address: ""
    property string query: ""
    signal dismiss
    signal navigate(string value)
    signal tabSelected(string id)
    onVisibleChanged: if (visible) {
        input.text = address;
        query = "";
        Qt.callLater(() => {
            input.forceActiveFocus();
            input.selectAll();
        });
    }
    Rectangle {
        anchors.fill: parent
        color: Qt.alpha(Theme.window, .36)
        TapHandler {
            onTapped: root.dismiss()
        }
    }
    Item {
        id: sheet
        width: Math.min(parent.width - 24, 536)
        x: (parent.width - width) / 2
        height: Math.max(0, Math.min(440, parent.height - root.keyboardInset - 16))
        y: parent.height - root.keyboardInset - height - 8
        GlassSurface {
            anchors.fill: parent
            radius: 27
            dense: true
        }
        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 16
            spacing: 10
            Rectangle {
                Layout.alignment: Qt.AlignHCenter
                width: 30
                height: 4
                radius: 2
                color: Qt.alpha(Theme.muted, .4)
            }
            RowLayout {
                Layout.minimumHeight: 48
                Layout.fillWidth: true
                spacing: 9
                UiIcon {
                    name: "search"
                    color: Theme.muted
                    Layout.preferredWidth: 20
                    Layout.preferredHeight: 20
                }
                Controls.TextField {
                    id: input
                    Layout.fillWidth: true
                    implicitHeight: 48
                    placeholderText: qsTr("Search or enter address")
                    font.pixelSize: 16
                    color: Theme.text
                    placeholderTextColor: Theme.muted
                    selectionColor: Theme.selection
                    selectedTextColor: Theme.text
                    selectByMouse: true
                    inputMethodHints: Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText
                    background: Rectangle {
                        radius: 16
                        color: Theme.field
                    }
                    leftPadding: 12
                    rightPadding: 12
                    onTextEdited: root.query = text
                    onAccepted: root.navigate(text)
                }
                MobileIconButton {
                    iconName: "x"
                    label: qsTr("Close")
                    implicitWidth: 44
                    onClicked: root.dismiss()
                }
            }
            ListView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumHeight: 0
                clip: true
                spacing: 2
                boundsBehavior: Flickable.StopAtBounds
                model: (root.query.length ? [
                        {
                            title: qsTr("Search the web"),
                            url: root.query,
                            search: true
                        }
                    ] : []).concat(browser.suggestions(root.query))
                Controls.ScrollBar.vertical: Controls.ScrollBar {}
                delegate: Controls.AbstractButton {
                    id: row
                    required property var modelData
                    width: ListView.view.width
                    height: 51
                    readonly property string badge: modelData.tab ? qsTr("Open tab") : !modelData.search && browser.history.some(h => h.url === modelData.url) ? qsTr("History") : ""
                    Accessible.name: modelData.title || modelData.url
                    onClicked: if (modelData.tab)
                        root.tabSelected(modelData.tab)
                    else
                        root.navigate(modelData.url)
                    background: Rectangle {
                        radius: 14
                        color: row.down || row.hovered ? Theme.hover : "transparent"
                    }
                    contentItem: RowLayout {
                        spacing: 11
                        UiIcon {
                            Layout.leftMargin: 8
                            name: row.modelData.search ? "search" : row.modelData.tab ? "copy" : "globe"
                            color: Theme.muted
                            Layout.preferredWidth: 20
                            Layout.preferredHeight: 20
                        }
                        Text {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            verticalAlignment: Text.AlignVCenter
                            textFormat: Text.PlainText
                            text: row.modelData.title || row.modelData.url
                            color: Theme.text
                            font.pixelSize: 14
                            elide: Text.ElideRight
                        }
                        Rectangle {
                            visible: row.badge.length > 0
                            implicitWidth: badge.implicitWidth + 14
                            implicitHeight: 25
                            radius: 12
                            color: Theme.field
                            Text {
                                id: badge
                                anchors.centerIn: parent
                                text: row.badge
                                color: Theme.muted
                                font.pixelSize: 10
                            }
                        }
                        UiIcon {
                            name: "chevron-right"
                            color: Theme.muted
                            Layout.preferredWidth: 15
                            Layout.preferredHeight: 15
                            Layout.rightMargin: 6
                        }
                    }
                }
            }
        }
    }
}
