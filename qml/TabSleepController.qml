pragma ComponentBehavior: Bound
import QtQuick
import QtWebEngine

QtObject {
    id: controller
    required property var view
    required property int timeoutSeconds
    property bool keepAwake: false
    readonly property bool eligible: view && timeoutSeconds > 0 && !keepAwake
        && !view.visible && !view.recentlyAudible && !view.loading
        && view.recommendedState !== WebEngineView.LifecycleState.Active

    // Freeze instead of discarding: preserve the document and in-page state.
    onEligibleChanged: if (!eligible) wake()
    onTimeoutSecondsChanged: wake()
    function wake() {
        if (view && view.lifecycleState !== WebEngineView.LifecycleState.Active)
            view.lifecycleState = WebEngineView.LifecycleState.Active
    }
    property Timer sleepTimer: Timer {
        interval: Math.max(1, controller.timeoutSeconds) * 1000
        running: controller.eligible && controller.view.lifecycleState === WebEngineView.LifecycleState.Active
        repeat: false
        onTriggered: {
            if (controller.eligible)
                controller.view.lifecycleState = WebEngineView.LifecycleState.Frozen
        }
    }
}
