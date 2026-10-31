# Mobile interface

The approved mobile direction has six primary states. The UI is shared Qt Quick;
WKWebView and Android System WebView only supply the page surface.

- **Start:** a small wordmark and six compact favorites in a 3 × 2 grid. Search
  belongs in the floating bottom pill, without a duplicate central input.
- **Browsing:** the page fills the viewport and continues behind the pill. Back,
  address, tab count and More stay reachable at the bottom.
- **Search:** a rounded bottom sheet over a dimmed page, with the input and compact
  open-tab/history suggestions above the keyboard.
- **Tabs:** a two-column thumbnail grid, individual close buttons, a workspace
  selector, and a bottom capsule for private mode, New tab and Done.
- **Workspaces:** vertical scrolling, compact cards, three tab previews per card,
  and a count for the remaining tabs.
- **More:** a bottom action sheet with Share, Reload and Find above grouped rows.

Swipe the pill horizontally to cycle tabs in the current workspace/mode; swipe
up for tab overview. Standard buttons remain usable without gestures.

On iOS, the expanded client area paints behind the status bar and home indicator.
The native page reaches the bottom edge; only interactive controls are inset above
the home indicator. The status-bar surface follows WKWebView's observed page
background while browsing, and the configured UI background on the start screen
or overlays. UIKit chooses contrasting status-bar text from that surface.

## Native compositing

A foreign native view normally covers overlapping Qt Quick items. The adapters
therefore cut out only the rounded floating-control region, and route touches in
that region back to Qt Quick. The native page retains its full viewport height.
In-memory page snapshots supply the pill's sampled glass backdrop, sheet backdrop
and tab thumbnails. Native preview capture excludes the visual cutout.

Snapshots are never included in session JSON or written to disk. Closing tabs
removes them; locking private browsing clears private previews and closes exposed
private UI. Re-entering private mode does not clear an existing authentication lock.

## Visual verification

The developer fixture renderer is desktop-only and opt-in. It uses temporary
session files and fabricated page images, without browsing or opening a real vault:

```sh
cmake -S mobile -B build-mobile-qt-preview \
  -DCMAKE_PREFIX_PATH=/path/to/Qt/6.12.0/macos \
  -DARBITER_MOBILE_DESIGN_PREVIEW=ON
cmake --build build-mobile-qt-preview --parallel 20
QT_QPA_PLATFORM=offscreen \
  build-mobile-qt-preview/arbiter_mobile.app/Contents/MacOS/arbiter_mobile \
  --design-screen=tabs --design-output=/tmp/arbiter-tabs.png
```

Screen names: `start`, `browsing`, `search`, `tabs`, `workspaces`, `more`.
`interactions` runs the actual QML pointer tests for horizontal/upward pill swipes
and closing a tab without dismissing overview. Offscreen rendering may simplify
shader effects; verify the final glass treatment on the native build.
Phone builds reject the design-fixture flag. Native Android integration tests
check rendering outside the pill cutout, uncut thumbnail capture, touch routing,
workspace storage isolation, and the Rust blocker. Also inspect actual iOS
simulator/device rendering before installing an updated development build.

## Shared mobile layout rules

MobileButton, MobileField, MobileToggle, MobileComboBox and MobileIconButton keep
48-point form controls and at least 44-point icon touch targets separate from
desktop sizing. MobileSection supplies consistent group padding and descriptions, while standalone
buttons and dropdowns keep only their own surface without a surrounding card;
MobileEmptyState and MobileLogo handle empty views and theme-aware branding.
Full pages use a centered column capped at 560 points, safe-area-aware footers,
vertical scrolling and keyboard avoidance. Settings uses a category index with
back navigation. Onboarding keeps its navigation outside the scrolling content.

Run the visual matrix after building the desktop fixture target:

```sh
python3 tools/mobile/render_interface.py \
  --app build-mobile-qt-preview/arbiter_mobile.app/Contents/MacOS/arbiter_mobile
```

It renders 52 cases across 390×844, 320×568 and 844×390, including light/dark,
empty lists, every settings category, onboarding steps, pairing/account prompts,
private lock, keyboard insets and scrolled forms. Logs fail the command on QML
runtime errors; inspect the images as well. The default output is in ignored
`output/mobile-ui-audit`. Fixtures use fabricated data and never unlock a real
vault; the scanner fixture does not request camera permission. Native permission
sheets and the final GPU glass treatment still need native-device verification.
