pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic as Basic
import Arbiter

UiTextField {
    id: field
    property string pickerTitle: qsTr("Choose date")
    readonly property date selectedDate: parseDate(text)
    placeholderText: "YYYY-MM-DD"
    maximumLength: 10
    rightPadding: 44
    onVisibleChanged: if (!visible) calendarPopup.close()
    function parseDate(value: string): date {
        const parts = value.trim().split("-")
        if (parts.length !== 3) return new Date(NaN)
        const date = new Date(Number(parts[0]), Number(parts[1]) - 1, Number(parts[2]))
        return Qt.formatDate(date, "yyyy-MM-dd") === value.trim() ? date : new Date(NaN)
    }
    function sameDay(a: date, b: date): bool {
        return a.getFullYear() === b.getFullYear() && a.getMonth() === b.getMonth() && a.getDate() === b.getDate()
    }
    function openCalendar() {
        calendarPopup.cursorDate = isNaN(selectedDate.getTime()) ? new Date() : selectedDate
        calendarPopup.viewDate = new Date(calendarPopup.cursorDate.getFullYear(), calendarPopup.cursorDate.getMonth(), 1)
        calendarPopup.open()
    }
    Keys.onPressed: event => {
        if (event.key === Qt.Key_Down && event.modifiers & Qt.AltModifier) {
            openCalendar(); event.accepted = true
        }
    }
    UiIconButton {
        objectName: field.objectName + "CalendarButton"
        anchors.right: parent.right
        anchors.rightMargin: 6
        anchors.verticalCenter: parent.verticalCenter
        label: field.pickerTitle
        iconName: "calendar"
        onClicked: field.openCalendar()
    }
    UiPopup {
        id: calendarPopup
        objectName: field.objectName + "Calendar"
        property date cursorDate: new Date()
        property date viewDate: new Date()
        function choose(date: date) {
            field.text = Qt.formatDate(date, "yyyy-MM-dd")
            close()
        }
        function moveDays(amount: int) {
            cursorDate = new Date(cursorDate.getFullYear(), cursorDate.getMonth(), cursorDate.getDate() + amount)
            viewDate = new Date(cursorDate.getFullYear(), cursorDate.getMonth(), 1)
        }
        function moveMonths(amount: int) {
            const lastDay = new Date(viewDate.getFullYear(), viewDate.getMonth() + amount + 1, 0).getDate()
            cursorDate = new Date(viewDate.getFullYear(), viewDate.getMonth() + amount, Math.min(cursorDate.getDate(), lastDay))
            viewDate = new Date(cursorDate.getFullYear(), cursorDate.getMonth(), 1)
        }
        parent: Basic.Overlay.overlay
        property point fieldPosition: Qt.point(0, 0)
        function reposition() { fieldPosition = field.mapToItem(calendarPopup.parent, 0, 0) }
        x: Math.max(12, Math.min(fieldPosition.x, calendarPopup.parent.width - width - 12))
        y: Math.max(12, Math.min(fieldPosition.y + field.height + height + 18 <= calendarPopup.parent.height
            ? fieldPosition.y + field.height + 6 : fieldPosition.y - height - 6, calendarPopup.parent.height - height - 12))
        width: Math.max(0, Math.min(320, calendarPopup.parent.width - 24))
        height: Math.max(0, Math.min(implicitHeight, calendarPopup.parent.height - 24))
        padding: 16
        modal: true
        dim: false
        focus: true
        closePolicy: Basic.Popup.CloseOnEscape | Basic.Popup.CloseOnPressOutside
        background: GlassSurface { dense: true; radius: Theme.menuRadius }
        // mapToItem() does not track ancestor layout changes in a binding.
        onAboutToShow: reposition()
        onOpened: days.forceActiveFocus()
        onClosed: if (field.visible) field.forceActiveFocus()
        Connections {
            target: calendarPopup.parent
            function onWidthChanged() { if (calendarPopup.visible) Qt.callLater(calendarPopup.reposition) }
            function onHeightChanged() { if (calendarPopup.visible) Qt.callLater(calendarPopup.reposition) }
        }
        contentItem: ColumnLayout {
            spacing: 10
            Basic.Label { Layout.fillWidth: true; text: field.pickerTitle; color: Theme.muted; font.pixelSize: 12; elide: Text.ElideRight }
            RowLayout {
                Layout.fillWidth: true
                UiIconButton { objectName: field.objectName + "PreviousMonth"; label: qsTr("Previous month"); iconName: "arrow-left"; onClicked: calendarPopup.moveMonths(-1) }
                Basic.Label { Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter; text: Qt.formatDate(calendarPopup.viewDate, "MMMM yyyy"); color: Theme.text; font.weight: Font.DemiBold; Accessible.role: Accessible.StaticText }
                UiIconButton { objectName: field.objectName + "NextMonth"; label: qsTr("Next month"); iconName: "arrow-right"; onClicked: calendarPopup.moveMonths(1) }
            }
            Basic.DayOfWeekRow {
                Layout.fillWidth: true
                Layout.preferredHeight: 24
                locale: days.locale
                spacing: 2
                delegate: Basic.Label {
                    required property string narrowName
                    text: narrowName; color: Theme.muted; font.pixelSize: 11
                    horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                }
            }
            Basic.MonthGrid {
                id: days
                objectName: field.objectName + "CalendarDays"
                Layout.fillWidth: true
                Layout.preferredHeight: 218
                month: calendarPopup.viewDate.getMonth()
                year: calendarPopup.viewDate.getFullYear()
                spacing: 2
                Accessible.name: field.pickerTitle
                Accessible.description: qsTr("Use arrow keys to move by day, Page Up or Page Down to change month, and Enter to select.")
                Keys.onLeftPressed: calendarPopup.moveDays(-1)
                Keys.onRightPressed: calendarPopup.moveDays(1)
                Keys.onUpPressed: calendarPopup.moveDays(-7)
                Keys.onDownPressed: calendarPopup.moveDays(7)
                Keys.onPressed: event => {
                    if (event.key === Qt.Key_PageUp || event.key === Qt.Key_PageDown) {
                        calendarPopup.moveMonths(event.key === Qt.Key_PageUp ? -1 : 1); event.accepted = true
                    } else if (event.key === Qt.Key_Home || event.key === Qt.Key_End) {
                        calendarPopup.cursorDate = new Date(days.year, days.month + (event.key === Qt.Key_End ? 1 : 0), event.key === Qt.Key_End ? 0 : 1)
                        event.accepted = true
                    }
                }
                Keys.onReturnPressed: calendarPopup.choose(calendarPopup.cursorDate)
                Keys.onEnterPressed: calendarPopup.choose(calendarPopup.cursorDate)
                Keys.onSpacePressed: calendarPopup.choose(calendarPopup.cursorDate)
                delegate: Basic.ItemDelegate {
                    id: day
                    required property var model
                    objectName: field.objectName + "Day" + Qt.formatDate(model.date, "yyyy-MM-dd")
                    readonly property bool selected: field.sameDay(model.date, field.selectedDate)
                    readonly property bool cursor: field.sameDay(model.date, calendarPopup.cursorDate)
                    padding: 0
                    hoverEnabled: true
                    Accessible.name: Qt.formatDate(model.date, "dddd, d MMMM yyyy")
                    Accessible.role: Accessible.Button
                    Accessible.selected: selected
                    background: Rectangle {
                        radius: 9
                        color: day.selected ? Theme.accent : day.down ? Theme.pressed : day.hovered ? Theme.hover : "transparent"
                        border.width: day.cursor && days.activeFocus || day.activeFocus ? 1 : 0
                        border.color: Theme.accent
                    }
                    contentItem: Basic.Label {
                        text: day.model.day
                        horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                        font.pixelSize: 12
                        color: day.selected ? Theme.contrastingText(Theme.accent) : day.model.month === days.month ? Theme.text : Theme.muted
                        font.weight: day.model.today ? Font.DemiBold : Font.Normal
                    }
                    onClicked: calendarPopup.choose(model.date)
                }
            }
            RowLayout {
                Layout.fillWidth: true
                UiButton { objectName: field.objectName + "ClearDateButton"; text: qsTr("Clear"); flat: true; onClicked: { field.clear(); calendarPopup.close() } }
                Item { Layout.fillWidth: true }
                UiButton { objectName: field.objectName + "TodayButton"; text: qsTr("Today"); flat: true; onClicked: calendarPopup.choose(new Date()) }
            }
        }
    }
}
