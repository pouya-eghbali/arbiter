# History swipe navigation

Arbiter uses horizontal precision scrolling at a page boundary for Back/Forward.
Side hover buttons and mouse edge-drag history navigation have been removed.
The bottom-edge pill reveal is separate. Tabs remains available through the pill,
menus, and its customizable keyboard shortcut; there is no corner gesture.

## Behavior researched

Chrome gives page scrolling priority, uses renderer acknowledgement and root
overscroll before showing history feedback, rejects ordinary mouse wheels and
momentum-only starts, and checks direction and a completion threshold. On macOS,
a gesture that initially scrolls content belongs to that content; start a fresh
swipe at its boundary to navigate. CSS `overscroll-behavior-x: contain/none` can
suppress browser history swiping.

Primary implementation references:

- [Chrome macOS history swiper](https://chromium.googlesource.com/chromium/src/+/refs/heads/main/chrome/browser/renderer_host/chrome_render_widget_host_view_mac_history_swiper.mm)
- [Chromium overscroll controller](https://chromium.googlesource.com/chromium/src/+/refs/heads/main/content/browser/renderer_host/overscroll_controller.cc)
- [Chromium overscroll configuration](https://chromium.googlesource.com/chromium/src/+/refs/heads/main/content/browser/renderer_host/overscroll_configuration.cc)

The implementation was checked against the Chromium 140 sources in our pinned
Qt WebEngine checkout. Chrome has platform-specific gesture and overlay code;
Arbiter uses Qt's phased scroll input and its own glass indicator and thresholds.

## Implementation

The Qt engine patch forwards browser-process `GestureEventAck` and
`DidOverscroll` reports to the native `HistorySwipe` object associated with each
view through `_arbiterHistorySwipe`. This QObject is not exposed to page script
or WebChannel. Notifications are queued, so completing navigation cannot destroy
the view while its Chromium event callback is on the stack. The SDK manifest's
`history_overscroll` flag prevents running with an older engine lacking the hook.

Only real, non-synthetic touchpad-style scroll beginnings are eligible. Modified
scrolling and momentum-only beginnings are rejected. The compositor determines
whether nested scroll areas, frames, script listeners, or the root viewport
consume the scroll; consuming content cancels the history gesture. Root
overscroll must also permit the browser default action.

The compositor authorizes boundary feedback; its offsets are not used as input
distances because their units vary with device scale and elasticity. Progress
uses acknowledged input deltas in view DIPs. The arrow appears after 24 horizontal
input pixels with horizontal movement
at least 2.5 times vertical overscroll. The completion distance is 18% of the view
width, clamped to 120–240 pixels. The progress ring fills toward this distance;
navigation occurs only at scroll end, once per gesture. Reversal, short gestures,
Escape, loss of focus, navigation, layout width changes, modal UI, private-window
locking, and a 1.5-second stalled-input timeout cancel it. Momentum cannot start
or add progress to a gesture. No DOM `scrollWidth` heuristics are used.

Back/Forward actions activate the originating split view. Settings → Appearance
controls history swiping; `appearance.historySwipeEnabled` defaults to true. The existing keyboard bindings
are unchanged. Phased gestures depend on the OS/input backend; ordinary mouse
wheels are deliberately unsupported.

## Validation

`historyswipe_test` checks thresholds, cancellation, direction, availability,
bridge lifetime, and watchdog behavior. `historyswipe_engine_test` sends phased
wheel input through actual Qt WebEngine pages, covering Back/Forward at boundaries,
root/nested/frame scrolling, CSS contain/none, prevented wheel input, vertical
scrolling, ordinary wheels, momentum, short gestures, and reversal.

```sh
cmake --build build-patched-tests --target historyswipe_test historyswipe_engine_test --parallel 24
ctest --test-dir build-patched-tests -R '^historyswipe' --output-on-failure
```
