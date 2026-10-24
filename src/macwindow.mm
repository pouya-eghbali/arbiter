#include <QWindow>
#include <QDebug>
#import <AppKit/AppKit.h>

void configureMacWindow(QWindow *window)
{
    // Decorated windows retain Qt/AppKit's standard title bar and controls.
    if (!window->flags().testFlag(Qt::ExpandedClientAreaHint))
        return;
    // Keep the titled native frame for AppKit's corner clipping and shadow,
    // while Qt extends the page across its transparent title-bar area.
    NSView *view = reinterpret_cast<NSView *>(window->winId());
    NSWindow *nativeWindow = view.window;
    nativeWindow.titleVisibility = NSWindowTitleHidden;
    nativeWindow.titlebarAppearsTransparent = YES;
    nativeWindow.hasShadow = YES;
    for (NSWindowButton button : {NSWindowCloseButton, NSWindowMiniaturizeButton,
                                  NSWindowZoomButton}) {
        [nativeWindow standardWindowButton:button].hidden = YES;
    }
    // Track native drag events so WebEngine cannot consume the gesture.
    __block bool dragging = false;
    __block NSPoint dragOrigin;
    __block NSPoint pointerOrigin;
    id dragMonitor = [NSEvent addLocalMonitorForEventsMatchingMask:
        NSEventMaskLeftMouseDown | NSEventMaskLeftMouseDragged | NSEventMaskLeftMouseUp
        handler:^NSEvent *(NSEvent *event) {
            if (dragging && event.type == NSEventTypeLeftMouseUp) {
                dragging = false;
                if (qEnvironmentVariableIsSet("ARBITER_DIAGNOSTICS")) {
                    const NSPoint after = nativeWindow.frame.origin;
                    qInfo() << "Window drag:" << dragOrigin.x << dragOrigin.y << "->" << after.x << after.y;
                }
                return nil;
            }
            if (dragging && event.type == NSEventTypeLeftMouseDragged) {
                const NSPoint pointer = [event.window convertPointToScreen:event.locationInWindow];
                [nativeWindow setFrameOrigin:NSMakePoint(dragOrigin.x + pointer.x - pointerOrigin.x,
                                                        dragOrigin.y + pointer.y - pointerOrigin.y)];
                return nil;
            }
            if (event.type != NSEventTypeLeftMouseDown || event.window != nativeWindow
                || (nativeWindow.styleMask & NSWindowStyleMaskFullScreen))
                return event;
            // Let Qt handle pane swapping even in the window's drag strip.
            const auto modifiers = event.modifierFlags & NSEventModifierFlagDeviceIndependentFlagsMask;
            if (modifiers == (NSEventModifierFlagCommand | NSEventModifierFlagShift))
                return event;
            const bool topEdge = event.locationInWindow.y >= NSHeight(nativeWindow.contentView.bounds) - 10;
            const bool optionDrag = (event.modifierFlags & NSEventModifierFlagDeviceIndependentFlagsMask) == NSEventModifierFlagOption;
            if (!topEdge && !optionDrag)
                return event;
            if (topEdge && event.clickCount == 2) {
                [nativeWindow zoom:nil];
            } else {
                dragging = true;
                dragOrigin = nativeWindow.frame.origin;
                pointerOrigin = [nativeWindow convertPointToScreen:event.locationInWindow];
            }
            return nil;
        }];
    QObject::connect(window, &QObject::destroyed, [dragMonitor] {
        [NSEvent removeMonitor:dragMonitor];
    });
}
