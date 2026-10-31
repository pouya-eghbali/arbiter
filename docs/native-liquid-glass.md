# Native Liquid Glass

Arbiter uses Apple's untinted Regular material for the floating navigation pill
on macOS 26+ and iOS 26+, and for the mobile More sheet. Enable or disable it in
Appearance → Liquid Glass (`appearance.liquidGlass` in desktop TOML;
`liquidGlass` in the mobile settings). It defaults on where the OS and SDK support
it. User-selected colors continue to control Glacier when native glass is off;
they are not applied as a native glass tint. Other surfaces retain Glacier.

## Integration

`NativeGlassHost` hosts the existing QML controls in a transparent child
QQuickWindow, placed in NSGlassEffectView.contentView or
UIVisualEffectView.contentView. This keeps the actual native glass between the
page and its controls. The native effect samples the composited view hierarchy;
no screenshot is supplied to the native material. On iOS the WKWebView pill
cutout is removed while native glass is active, so the actual page remains
behind the material. Existing in-memory previews remain useful for tab thumbnails
and non-native overlays.

The bridge uses public APIs only: NSGlassEffectView on macOS and UIGlassEffect
with UIVisualEffectView on iOS. macOS interactive glass is enabled on macOS 27+.
The [qt-liquid-glass project](https://github.com/fsalinas26/qt-liquid-glass) was reviewed as a reference; it is not vendored or
linked. Its QWidget integration and private extended variants do not directly
fit this Qt Quick/iOS bridge.

Native colors come from the effect's effective system appearance. The bridge
tracks geometry, visibility, and accessibility changes, and restores controls to
their original QML parent when disabled. Hidden or disabled hosts immediately
hide their native content, including private-window covers. Reduce Transparency
uses an opaque Glacier fallback. Older Apple systems, builds with older SDKs,
non-Apple platforms, and the offscreen renderer use Glacier.

On iOS, a plain UIView wrapped by a foreign QWindow carries the scene offset;
the Qt foreground stays at local (0,0) inside the material's contentView. A QML
WindowContainer registers the material with Qt Quick's native stacking, so a
returning WKWebView cannot cover it. Native windows are retained while panels
are hidden, avoiding render/touch teardown during ordinary menu and tab changes.

## Validation

- `arbiter --native-glass-smoke-test`: isolated desktop pill, native/Glacier
  toggles, private-cover visibility, address focus, and More button input;
  does not open user profiles.
- Desktop mobile fixture `--design-screen=nativechecks`: real native child-window
  pointer events, pill swipes, More → Settings actions, repeated teardown,
  preference toggling, and the opaque accessibility fallback. Run with the
  default Cocoa/Metal platform, not `QT_QPA_PLATFORM=offscreen`.
- iOS simulator `SIMCTL_CHILD_ARBITER_UI_SMOKE=1`: repeated touch-driven menu
  and tab-overview transitions over a local WKWebView page. Touch targets are
  resolved through UIKit hit testing before Qt injection; checks cover outside
  taps, the menu close button, and tab-overview dismissal. This catches geometry
  and stacking faults that invoking QML actions directly misses. Test support is
  excluded from physical-device builds. See mobile-interface.md for launch commands.
- Existing software screen matrix continues checking the Glacier path.

`ARBITER_GLASS_FORCE_OPAQUE=1` exercises the reduced-transparency branch in an
isolated test process without changing system preferences. Native surfaces need
OS/simulator screenshots for visual verification: QQuickWindow.grabWindow only
captures that window's scene graph, not native child views.
