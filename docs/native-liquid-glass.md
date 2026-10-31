# Native Liquid Glass

Arbiter uses Apple's untinted Regular material for floating browser UI on macOS
26+ and iOS 26+. Appearance → Liquid Glass controls the entire rollout
(`appearance.liquidGlass` in desktop TOML; `liquidGlass` in mobile settings).
Custom colors continue to control Glacier when native material is disabled.
Settings pages, web pages, and ordinary content surfaces remain solid.

On macOS, the shared popup layer covers menus and tooltips, dropdowns, address
search, tab and workspace selectors, permission and authentication dialogs,
download/history/extension panels, and other floating dialogs. The navigation
pill uses the same material. On iOS, the pill, More, search and find sheets,
tab/workspace selectors, workspace editor, and shared popup controls use native
material. Native sheets inherit semantic foreground colors instead of applying
user background colors as a glass tint.

## Integration

The bridge uses public APIs only: NSGlassEffectView on macOS and UIGlassEffect
with UIVisualEffectView on iOS. macOS interactive pill glass is enabled on macOS
27+. The [qt-liquid-glass project](https://github.com/fsalinas26/qt-liquid-glass)
was reviewed as a reference; it is not vendored or linked.

Desktop floating popups use Qt's Popup.Window implementation. A native glass
view sits behind the existing Qt content view in that window. Qt retains its
original native view hierarchy, keyboard focus, nested menu routing, and mouse
grab handling. No individual popup controls are moved to another window.

For the pill and mobile sheets, NativeGlassHost places a transparent child
QQuickWindow inside the native material's contentView. A foreign QWindow wraps
a plain UIView/NSView carrying the scene offset, while the foreground uses local
(0,0) geometry. A QML WindowContainer registers it with Qt Quick's native
stacking so a returning web view cannot cover it. Native input windows survive
ordinary panel transitions. Inactive hosts restore focus only if their own
window actually held it; hidden surfaces do not poll or reclaim focus.
Materials attach during the opening transition and follow its opacity; Glacier
is reserved for the fallback path rather than painted while native glass starts.
Native popup hosts use Qt’s public native event filter to observe AppKit mouse
events before normal view delivery. The backing window stays modeless to avoid
Cocoa dropping parent-window clicks inside runModalSession; the QML popup keeps
its modal overlay, close policy, and outside-input guard. Qt event delivery
remains available for other backends and tests.
They respect close policies and child menus, and block modal click-through. The
pill stays in its native host while More is open, keeping material and anchors
stable.

SurfacePalette records the actual native foreground color per window. Shared
controls resolve text, secondary text, fields and hover states against that
material. This leaves ordinary settings/content colors unchanged. Mobile input
geometry maps the keyboard from the focused window into the main layout before
adjusting sheet positions.

UiPopup, UiMenu and UiToolTip close when their owning private window is covered,
including native windows outside the main Qt overlay. Hidden or disabled child
hosts immediately hide their native content. Reduce Transparency uses opaque
Glacier. Older Apple systems, older SDKs, non-Apple platforms, and the offscreen
renderer use Glacier. The native material samples the composited hierarchy;
no screenshot is supplied to it.

Tab preview textures cannot cross QQuickWindows. Desktop native tab overview
therefore retains bounded grab results in memory for the visible workspace,
and clears them on dismissal/private covering. Glacier keeps its existing live
scene-graph previews. No preview images are written to disk.

## Validation

- `arbiter --native-glass-smoke-test`: isolated pill lifecycle, actual native
  foreground bounds, address focus, and More input. No user profiles are opened.
- `arbiter --native-overlays-smoke-test`: repeated popup input, nested menus,
  Escape/outside dismissal (including native page windows), protected dialogs,
  glass during the opening fade, private covering and native/Glacier toggling.
  Native mouse checks post AppKit events and include the real workspace selector
  and pill menu; fallback checks use Qt input. Activation uses public AppKit APIs. Native runs need uninterrupted foreground access; switching to
  another application correctly dismisses the popups. With the offscreen
  platform, the same fixture exercises Glacier without activating any windows.
- Mobile desktop fixture `--design-screen=nativechecks`: swipe/menu actions,
  repeated transitions, accessibility fallback and private locking. Run with
  Cocoa/Metal rather than `QT_QPA_PLATFORM=offscreen` for native checks.
- iOS simulator `SIMCTL_CHILD_ARBITER_UI_SMOKE=1`: UIKit hit testing before Qt
  touch injection, including search typing, workspace editing/cancellation,
  menu dismissal and tab overview cycles. This support is excluded from device
  builds. See mobile-interface.md for launch commands.
- The software screen matrix checks Glacier across phone sizes and themes.

`ARBITER_GLASS_FORCE_OPAQUE=1` exercises reduced transparency without changing
system preferences. Native surfaces require OS/simulator screenshots for visual
verification: QQuickWindow.grabWindow captures the scene graph, not native views.
