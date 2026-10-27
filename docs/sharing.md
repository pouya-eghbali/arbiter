# Navigation reveal and native page sharing

## Controls

- **Command+Shift+L** on macOS / **Ctrl+Shift+L** on Windows/Linux reveals
  the navigation pill and focuses its address button. This also works when the
  pill is disabled in Appearance: the reveal is temporary and does not change
  the saved preference. Escape dismisses it; clicking away lets it collapse.
- **Command+Shift+S** / **Ctrl+Shift+S** shares the focused view's page.
- The pill has a Share button. Narrow windows offer the same action in More.
- Both shortcuts are customizable in Settings → Keyboard.
- **Command+L / Ctrl+L** still opens the editable address prompt directly.

Sharing uses the current view, including the focused pane in a split layout.
Blank/internal pages do not offer a share action. Credentials embedded in a URL
are stripped; the path, query, and fragment are preserved. Copy Link writes text
and a URI list so recipients can use the clipboard's appropriate representation.

## Platforms

macOS uses AppKit's **NSSharingServicePicker**. The OS populates the services
from installed apps (for example AirDrop, Messages, and Mail). Arbiter adds a
**Copy link** service through the picker delegate. The page title is supplied as
the sharing subject. The picker is anchored to the pill's share button or near
the bottom of the window when invoked from the keyboard with no visible pill.
The sharing API requires macOS 13+. The release's Qt kit can require a newer OS; see [release requirements](releases.md).

Windows uses **DataTransferManager** through **IDataTransferManagerInterop** for
the originating HWND. It supplies a WebLink and text, and adds Copy Link with
**ShareProvidersRequested**. Its icon is an in-memory stream, so an unpackaged
Qt app does not depend on `ms-appx` assets. Windows owns the sheet's dismissal;
the registered request/provider session is revoked on navigation, another share,
locking, or window destruction. Native Windows runtime verification is pending.

Linux desktop environments do not expose one universal XDG share sheet. Building
with `-DARBITER_KDE_PURPOSE=ON` uses KDE Purpose and its installed ShareUrl
providers (requires KF6Purpose built for the same Qt installation). Otherwise
Arbiter shows a desktop menu with Copy Link and Email Link, using the configured
mail handler. There is no custom copy-only share card. Linux/Purpose runtime
verification is pending.

## Ownership and privacy

A window owns its NativeShare controller. Its originating view is guarded by
QPointer. Navigation, tab/pane changes, Settings, shutdown, and private locking
cancel the app's share session. Copy callbacks recheck the originating view and
private lock state, and carry a generation number so a delayed callback from an
old session cannot copy a newly opened page. macOS closes the native picker on
cancellation. Windows has no public dismissal method; cancel revokes the app's
handlers and prevents further data callbacks from returning a page.

## Verification

```sh
cmake --build build-patched-tests --target nativeshare_test browserpreferences_test keybindings_test --parallel 24
ctest --test-dir build-patched-tests -R '^(nativeshare|browserpreferences|keybindings)$' --output-on-failure
# Optional real desktop picker test; opens example.com and closes without a target.
ARBITER_TEST_NATIVE_SHARE=1 QT_QPA_PLATFORM=cocoa build-patched-tests/nativeshare_test nativePickerOpensAndCancels
```

Clipboard tests use the offscreen platform's isolated clipboard. The native test
only opens/cancels a synthetic page's picker; target apps are not invoked.
The app UI check also exercises real key events, a temporary reveal while the
saved pill preference is off, Escape, and the Share button.

API references: [AppKit sharing picker](https://developer.apple.com/documentation/appkit/nssharingservicepicker),
[Windows desktop share UI](https://learn.microsoft.com/en-us/windows/apps/develop/ui-input/display-ui-objects#winui-with-c),
[custom Windows providers](https://blogs.windows.com/windowsdeveloper/2017/04/06/new-share-experience-windows-10-creators-update/).
