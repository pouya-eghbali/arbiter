import QtQuick

Shortcut {
    required property string binding
    property bool blocked: false
    property string platformName: Qt.platform.os
    sequence: binding
    context: Qt.WindowShortcut
    enabled: platformName !== "osx" && !blocked
}
