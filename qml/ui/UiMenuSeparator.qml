pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic as Basic
import Arbiter

Basic.MenuSeparator {
    topPadding: 5
    bottomPadding: 5
    leftPadding: 8
    rightPadding: 8
    implicitHeight: visible ? 11 : 0
    contentItem: Rectangle { implicitHeight: 1; color: Theme.borderFor(parent) }
}
