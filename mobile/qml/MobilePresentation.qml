pragma ComponentBehavior: Bound
import QtQuick
import Arbiter

// Keep native content alive through its exit; privacy concealment is immediate.
Item {
    property bool presented: true
    property bool concealed: false
    property real progress: presented && !concealed ? 1 : 0
    visible: !concealed && (presented || progress > 0.001)
    opacity: progress
    Behavior on progress {
        NumberAnimation {
            duration: concealed || Theme.reducedMotion ? 0 : presented ? 220 : 160
            easing.type: Easing.OutCubic
        }
    }
}
